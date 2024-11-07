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

extern "C" {
struct dht_scrape_msg {
  unsigned int ipv4;
  unsigned short port;
  unsigned char info_hash[20];
  unsigned char magic[4];
};
}

#define SHA_HASH_SIZE 20

static uint64_t
__db_count_torrents(sqlite3 *db) {
  sqlite3_stmt *stmt = NULL;
  const char *dml = "SELECT COUNT(*) FROM torrent";
  uint64_t result = 0;
  int r;

  if ((r = sqlite3_prepare(db, dml, -1, /*OUT*/ &stmt, NULL)) != SQLITE_OK) {
    fprintf(stderr, "%s: sqlite3_prepare (%d)\n", __func__, r);
    goto Lout;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    result = sqlite3_column_int64(stmt, 0);
  }

Lout:
  if (stmt) {
    sqlite3_finalize(stmt);
  }
  return result;
}

static bool
__db_seed_cache(dht::DHTMeta_spbt_scrape_client &self, sqlite3 *db) {
  bool res = false;
  const char *dml = "SELECT info_hash FROM torrent";
  sqlite3_stmt *stmt = NULL;
  uint64_t n_info_hashes;

  n_info_hashes = std::max(__db_count_torrents(db) * 2, uint64_t(100000000));

  {
    int r;
    if ((r = sqlite3_prepare(db, dml, -1, /*OUT*/ &stmt, NULL)) != SQLITE_OK) {
      fprintf(stderr, "%s: sqlite3_prepare (%d)\n", __func__, r);
      goto Lout;
    }
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const void *info_hash = sqlite3_column_blob(stmt, 0);
    size_t info_hash_len =
        std::min(sqlite3_column_bytes(stmt, 0), int(SHA_HASH_SIZE));
    assertx(info_hash_len == SHA_HASH_SIZE);
  }

  res = true;
Lout:
  if (stmt) {
    sqlite3_finalize(stmt);
  }
  return res;
}

static bool
__sp_bt_DB_has_maybe_false_positive(dht::DHTMeta_spbt_scrape_client &self,
                                    const uint8_t info_hash[SHA_HASH_SIZE]) {
  if (self.cache) {
  }
  return true;
}

dht::DHTMeta_spbt_scrape_client::DHTMeta_spbt_scrape_client()
    : unix_socket_file{socket(AF_UNIX, SOCK_DGRAM, 0)}
    , cache{nullptr}
    , dir_fd{} {
  {
    char scrape_socket_path[PATH_MAX]{};
    xdg_runtime_dir(scrape_socket_path);
    sp::fd tmp(open(scrape_socket_path, O_PATH));
    swap(tmp, dir_fd);
    strcat(scrape_socket_path, "/spbt_scrape.socket");
    name.sun_family = AF_UNIX;
    strcpy(name.sun_path, scrape_socket_path);
  }

  {
    char db_path[PATH_MAX]{};
    // TODO db_path
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

int
dht::spbt_scrape_client_send(dht::DHTMeta_spbt_scrape_client &self,
                             const Key &infohash, const Contact &contact) {

  if (bool(self.dir_fd) && ::faccessat(int(self.dir_fd), "spbt_scrape.socket",
                                       W_OK, AT_EACCESS) == F_OK) {
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
  return __sp_bt_DB_has_maybe_false_positive(self, ih.id);
}
