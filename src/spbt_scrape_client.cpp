#include "spbt_scrape_client.h"

#include <algorithm>
#include <climits>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <sqlite3.h>

#include <hash/djb2.h>
#include <hash/fnv.h>

#include "Log.h"

extern "C" {
struct dht_scrape_msg {
  unsigned int ipv4;
  unsigned short port;
  unsigned char info_hash[20];
  unsigned char magic[4];
};

#define PUBLISH_HAVE 1

struct dht_publish_msg {
  unsigned char info_hash[20];
  int flag;
  unsigned char magic[4];
};
}

#define SHA_HASH_SIZE 20

static bool
__db_cache_insert(dht::DHTMeta_spbt_scrape_client &self, dht::Infohash ih) {
  return insert(self.cache, ih.id);
}

static bool
__db_seed_cache(dht::DHTMeta_spbt_scrape_client &self, sqlite3 *db) {
  bool res = false;
  const char *dml = "SELECT info_hash FROM torrent";
  sqlite3_stmt *stmt = NULL;

  {
    int r;
    if ((r = sqlite3_prepare(db, dml, -1, /*OUT*/ &stmt, NULL)) != SQLITE_OK) {
      fprintf(stderr, "%s: sqlite3_prepare (%d)\n", __func__, r);
      goto Lout;
    }

    while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
      const int column = 0;
      int info_hash_len =
          std::min(sqlite3_column_bytes(stmt, column), int(SHA_HASH_SIZE));
      const void *info_hash = sqlite3_column_blob(stmt, column);
      if (info_hash_len == SHA_HASH_SIZE) {
        dht::Infohash ih;
        memcpy(ih.id, info_hash, SHA_HASH_SIZE);
        __db_cache_insert(self, ih);
      } else {
        assertx(false);
      }
    }
    // fprintf(stderr, "%s: sqlite3_step (%d) %s\n", __func__, r,
    //         sqlite3_errmsg(db));
  }

  res = true;
Lout:
  if (stmt) {
    sqlite3_finalize(stmt);
  }
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
    fprintf(stderr, "scrape_socket_path[%.*s]\n", (int)PATH_MAX,
            scrape_socket_path2);
  }

  if (strlen(db_path) > 0) {
    sqlite3 *db = nullptr;
    int flags = SQLITE_OPEN_READONLY;
    int r;

    if ((r = sqlite3_open_v2(db_path, &db, flags, NULL)) != SQLITE_OK) {
      fprintf(stderr, "ERROR: opening database '%s': %s (%d)\n", db_path,
              sqlite3_errmsg(db), r);
    } else {
      __db_seed_cache(*this, db);
      sqlite3_close_v2(db);
    }
  }
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

  if (spbt_has_infohash(self, infohash)) {
    return true;
  }

  if (spbt_scrape_client_is_started(self)) {
    dht_scrape_msg msg{};
    msg.magic[0] = 32;
    msg.magic[1] = 47;
    msg.magic[2] = 203;
    msg.magic[3] = 56;
    memcpy(msg.info_hash, infohash, sizeof(msg.info_hash));
    assertx(contact.ip.type == IpType::IPV4);
    msg.ipv4 = contact.ip.ipv4;
    msg.port = contact.port;

    if (sendto(int(self.unix_socket_file), &msg, sizeof(msg), 0,
               (sockaddr *)&self.name, sizeof(self.name)) < 0) {
      fprintf(stderr, "%s: sendto %s (%s)", __func__, self.name.sun_path,
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
  dht_publish_msg msg{};
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
    perror("recvmsg()");
    return 0;
  }
#endif

  if (ret == sizeof(msg)) {
    const unsigned char magic[4] = {205, 7, 44, 216};
    if (memcmp(msg.magic, magic, sizeof(magic)) == 0) {
      dht::Infohash ih;
      memcpy(ih.id, msg.info_hash, sizeof(msg.info_hash));
      if (__db_cache_insert(self->self, ih)) {
        logger::spbt::publish(self->self.now, ih);
      }
    } else {
      assertx(false);
    }
  } else {
    assertx(false);
  }

  return 0;
}

dht::publish_ACCEPT_callback::publish_ACCEPT_callback(
    DHTMeta_spbt_scrape_client &_self, fd &_fd)
    : core_cb{}
    , self{_self}
    , publish_fd{_fd} {
  core_cb.closure = this;
  core_cb.callback = on_publish_ACCEPT_callback;
}
