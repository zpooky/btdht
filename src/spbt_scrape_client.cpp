#include "spbt_scrape_client.h"

#include <algorithm>
#include <climits>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <sqlite3.h>

#include <hash/djb2.h>
#include <hash/fnv.h>
#include <io/file.h>

#include "dht.h"
#include "scrape.h"

#include "Log.h"

#define INFO_HASH_v1 20

extern "C" {
struct dht_scrape_msg {
  unsigned int ipv4;
  unsigned short port;
  unsigned char info_hash[64];
  unsigned int l_info_hash;
  unsigned char magic[4];
};

#define PUBLISH_HAVE 1

struct bt_to_dht_publish_msg {
  int flag;
  unsigned char info_hash[64];
  unsigned int l_info_hash;
  unsigned char magic[4];
};

struct bt_to_dht_backoff_msg {
  int backoff;
  unsigned char magic[4];
};

enum bt_to_dht_msg_kind {
  BT_TO_DHT_MSG_UNKNOWN = 0,
  BT_TO_DHT_MSG_PUBLISH = 1,
  BT_TO_DHT_MSG_BACKOFF = 2,
};

struct bt_to_dht_msg {
  enum bt_to_dht_msg_kind kind;
  union {
    struct bt_to_dht_publish_msg publish;
    struct bt_to_dht_backoff_msg backoff;
  };
};
}

static bool
__db_seed_cache(dht::DHTMeta_spbt_scrape_client &self, sqlite3 *db) {
  char cached_bloomfilter[PATH_MAX] = {0};
  bool res = false;

  fprintf(stderr, "%s: START\n", __func__);
  if (xdg_share_dir(cached_bloomfilter)) {
    ::strcat(cached_bloomfilter, "/spdht/scrape_bloomfilter.raw");

    fprintf(stderr, "%s:cached_bloomfilter[%.*s]\n", __func__, (int)PATH_MAX,
            cached_bloomfilter);
    fd raw_fd = fs::open_read(cached_bloomfilter);
    if (bool(raw_fd)) {
      struct stat st {};
      if (fstat(int(raw_fd), &st) == 0) {
        if (st.st_size == sizeof(self.cache.bitset.raw)) {
          size_t readed;
          if ((readed = fs::read(raw_fd, self.cache.bitset.raw,
                                 sizeof(self.cache.bitset.raw))) ==
              (size_t)st.st_size) {
            res = true;
          } else {
            fprintf(stderr, "%s:0\n", __func__);
            assertxs(false, readed, sizeof(self.cache.bitset.raw), st.st_size);
          }
        } else {
          fprintf(stderr, "%s:1\n", __func__);
          assertxs(false, sizeof(self.cache.bitset.raw), st.st_size);
        }
      } else {
        fprintf(stderr, "%s:2\n", __func__);
      }
    } else {
      fprintf(stderr, "%s:3\n", __func__);
    }
    unlink(cached_bloomfilter);
  } else {
    fprintf(stderr, "%s:4\n", __func__);
  }

  if (!res) {
    const char *dml = "SELECT info_hash FROM torrent";
    sqlite3_stmt *stmt = NULL;

    fprintf(stderr, "%s: db START\n", __func__);
    int r;
    if ((r = sqlite3_prepare(db, dml, -1, /*OUT*/ &stmt, NULL)) == SQLITE_OK) {
      uint32_t failed_length = 0;

      while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
        const int column = 0;
        int l_ih = std::min(sqlite3_column_bytes(stmt, column), INFO_HASH_v1);
        const void *info_hash = sqlite3_column_blob(stmt, column);

        if (l_ih == INFO_HASH_v1) {
          dht::Infohash ih;
          memcpy(ih.id, info_hash, l_ih);
          if (!insert(self.cache, ih.id)) {
            ++self.cache_length;
          } else {
            ++failed_length;
          }

          if ((self.cache_length % 300000) == 0) {
            fprintf(stdout, "%s: self.cache_length: %u\n", __func__,
                    self.cache_length);
          }
        } else {
          fprintf(stderr, "%s:5\n", __func__);
          assertx(false);
        }
      } // while

      fprintf(stdout, "theoretical_max_capacity: %zu, length: %u, failed: %u\n",
              theoretical_max_capacity(self.cache), self.cache_length,
              failed_length);
      res = true;
    } else {
      fprintf(stderr, "%s: sqlite3_prepare (%d)\n", __func__, r);
    }
    if (stmt) {
      sqlite3_finalize(stmt);
    }
    fprintf(stderr, "%s: db END\n", __func__);
  }

  fprintf(stderr, "%s: END (%d)\n", __func__, res);
  return res;
}

static std::size_t
djb_infohash(const dht::Key &ih) noexcept {
  return djb2::encode32(ih, sizeof(ih));
}

static std::size_t
fnv_infohash(const dht::Key &ih) noexcept {
  return fnv_1a::encode32(ih, sizeof(ih));
}

dht::DHTMeta_spbt_scrape_client::DHTMeta_spbt_scrape_client(
    Timestamp &now, const char *scrape_socket_path, const char *db_path)
    : hashers{}
    , cache{hashers}
    , cache_length{0}
    , unix_socket_file{socket(AF_UNIX, SOCK_DGRAM, 0)}
    , dir_fd{}
    , now{now} {

  assertx_n(insert(hashers, djb_infohash));
  assertx_n(insert(hashers, fnv_infohash));

  if (strlen(scrape_socket_path) > 0) {
    char scrape_socket_path2[PATH_MAX]{};

    strcpy(scrape_socket_path2, scrape_socket_path);
    sp::fd tmp(open(scrape_socket_path2, O_PATH));
    swap(tmp, dir_fd);
    strcat(scrape_socket_path2, "/spbt_scrape.socket");
    name.sun_family = AF_UNIX;
    strcpy(name.sun_path, scrape_socket_path2);
  }
  fprintf(stderr, "%s: scrape_socket_path2[%s]\n", __func__, name.sun_path);
  fprintf(stderr, "%s: db_path[%s]\n", __func__, db_path);

  if (strlen(db_path) > 0) {
    sqlite3 *db = nullptr;
    int flags = SQLITE_OPEN_READONLY;
    int r;

    if ((r = sqlite3_open_v2(db_path, &db, flags, NULL)) != SQLITE_OK) {
      fprintf(stderr, "ERROR: opening database '%s': %s (%d)\n", db_path,
              sqlite3_errmsg(db), r);
      assertx(false);
    } else {
      __db_seed_cache(*this, db);
      sqlite3_close_v2(db);
    }
  }
  fprintf(stderr, "%s: END\n", __func__);
}

dht::DHTMeta_spbt_scrape_client::~DHTMeta_spbt_scrape_client() {
  char tmp_cached_bloomfilter[PATH_MAX] = {0};
  char cached_bloomfilter[PATH_MAX] = {0};
  if (xdg_share_dir(cached_bloomfilter)) {
    ::strcat(cached_bloomfilter, "/spdht/scrape_bloomfilter.raw");
    ::strcpy(tmp_cached_bloomfilter, cached_bloomfilter);
    ::strcat(tmp_cached_bloomfilter, ".tmp");
    fd raw_fd = fs::open_trunc(tmp_cached_bloomfilter);
    if (bool(raw_fd)) {
      std::size_t writed = fs::write(raw_fd, this->cache.bitset.raw,
                                     sizeof(this->cache.bitset.raw));
      if (writed != sizeof(this->cache.bitset.raw)) {
        unlink(tmp_cached_bloomfilter);
        assertxs(false, writed, sizeof(this->cache.bitset.raw));
      }
    }
  }
  ::rename(tmp_cached_bloomfilter, cached_bloomfilter);
}

bool
dht::spbt_scrape_client_is_started(dht::DHTMeta_spbt_scrape_client &self) {
  return bool(self.dir_fd) &&
         ::faccessat(int(self.dir_fd), "spbt_scrape.socket", W_OK,
                     AT_EACCESS) == F_OK;
}

bool
dht::spbt_scrape_client_send(dht::DHTMeta_spbt_scrape_client &self,
                             const Key &infohash, const Contact &contact) {
  bool tmp = spbt_scrape_client_is_started(self);
  auto f = stderr;
  fprintf(f, "%s:[%s]", __func__, tmp ? "TRUE" : "FALSE");
  dht::print_hex(f, infohash, sizeof(infohash));
  fprintf(f, "\n");

  if (tmp) {
    dht_scrape_msg msg{};
    msg.magic[0] = 32;
    msg.magic[1] = 47;
    msg.magic[2] = 203;
    msg.magic[3] = 56;
    memcpy(msg.info_hash, infohash, sizeof(infohash));
    msg.l_info_hash = sizeof(infohash);
    assertx(contact.ip.type == IpType::IPV4);
    msg.ipv4 = contact.ip.ipv4;
    msg.port = contact.port;

    if (sendto(int(self.unix_socket_file), &msg, sizeof(msg), 0,
               (sockaddr *)&self.name, sizeof(self.name)) < 0) {
      fprintf(stderr, "%s: sendto %s (%s)\n", __func__, self.name.sun_path,
              strerror(errno));
      // assertx(false);
    }
  }

  return true;
}

bool
dht::spbt_has_infohash(DHTMeta_spbt_scrape_client &self, const Infohash &ih) {
  return test(self.cache, ih.id);
}

bool
dht::spbt_has_infohash(DHTMeta_spbt_scrape_client &self, const Key &ih) {
  return test(self.cache, ih);
}

static int
on_publish_ACCEPT_callback(void *closure, uint32_t events) {
  ssize_t ret;
  auto self = (dht::publish_ACCEPT_callback *)closure;
  auto *dht = (dht::DHT *)self->dht;
  bt_to_dht_msg msg{};
  int flags = 0;
#if 0
  struct msghdr msgh {};

  {
    struct sockaddr_in addr {};

    struct iovec iovecs {};
    iovecs.iov_base = &msg;
    iovecs.iov_len = sizeof(msg);
    msgh.msg_name = &addr;
    msgh.msg_namelen = sizeof(addr);
    msgh.msg_iov = &iovecs;
    msgh.msg_iovlen = 1;

    if ((ret = recvmsg(int(self->publish_fd), &msgh, flags)) < 0) {
      perror("recvmsg()");
      return 0;
    }
  }
#else
  if ((ret = recv(int(self->publish_fd), &msg, sizeof(msg), flags)) < 0) {
    fprintf(stderr, "%s: recv: len(%zd) %s (%d)\n", __func__, ret,
            strerror(errno), errno);
    return 0;
  }
#endif

  if (ret == sizeof(msg)) {
    switch (msg.kind) {
    case BT_TO_DHT_MSG_PUBLISH: {
      bt_to_dht_publish_msg *publish = &msg.publish;
      const unsigned char magic[4] = {205, 7, 44, 216};
      if (memcmp(publish->magic, magic, sizeof(magic)) == 0) {
        dht::Infohash tmp_ih;
        dht::Infohash ih;
        if ((publish->l_info_hash == 0) ||
            (publish->l_info_hash > sizeof(publish->info_hash))) {
          return -1;
        }
        memcpy(ih.id, publish->info_hash,
               std::min(publish->l_info_hash, unsigned(sizeof(ih.id))));
        assertx(memcmp(ih.id, tmp_ih.id, sizeof(tmp_ih.id)) != 0);

        static_assert(sizeof(ih.id) == INFO_HASH_v1);
        bool present = test(dht->db.scrape_client.cache, ih.id);
        logger::spbt::publish(*dht, ih, present);
        bool before = insert(dht->db.scrape_client.cache, ih.id);
        if (!before) {
          ++dht->db.scrape_client.cache_length;
          scrape::publish(*dht, ih);
        }
        assertx(present == before);
      } else {
        assertx(false);
      }
    } break;
    case BT_TO_DHT_MSG_BACKOFF: {
      bt_to_dht_backoff_msg *backoff = &msg.backoff;
      const unsigned char magic[4] = {215, 193, 107, 66};
      if (memcmp(backoff->magic, magic, sizeof(magic)) == 0) {
        if (backoff->backoff) {
          dht->scrape_backoff = true;
        } else {
          dht->scrape_backoff = false;
        }
        fprintf(stdout, "%s:backoff[%s]\n", __func__,
                dht->scrape_backoff ? "TRUE" : "FALSE");
      } else {
        assertx(false);
      }
    } break;
    default:
      assertx(false);
    };
  } else {
    assertx(false);
  }

  return 0;
}

dht::publish_ACCEPT_callback::publish_ACCEPT_callback(void *_dht, fd &_fd)
    : dht{_dht}
    , core_cb{}
    , publish_fd{_fd} {
  core_cb.closure = this;
  core_cb.callback = on_publish_ACCEPT_callback;
}
