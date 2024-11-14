#ifndef SP_MAINLINE_DHT_SPBT_SCRAPE_CLIENT_H
#define SP_MAINLINE_DHT_SPBT_SCRAPE_CLIENT_H

#include "util.h"
#include <core.h>
#include <io/fd.h>
#include <sys/un.h>

#include <util/Bloomfilter.h>

namespace dht {
//=====================================
struct DHTMeta_spbt_scrape_client {
  sp::StaticArray<sp::hasher<dht::Infohash>, 2> hashers;
  sp::BloomFilter<dht::Infohash, 1 * 1024 * 1024> cache;
  sp::fd unix_socket_file;
  sp::fd dir_fd;
  sockaddr_un name{};
  Timestamp &now;

  DHTMeta_spbt_scrape_client(Timestamp &now, const char *scrape_socket_path,
                             const char *db_path);
  ~DHTMeta_spbt_scrape_client() {
  }
};

struct publish_ACCEPT_callback {
  sp::core_callback core_cb;
  DHTMeta_spbt_scrape_client &self;
  fd &publish_fd;
  publish_ACCEPT_callback(DHTMeta_spbt_scrape_client &, fd &_fd);
};

bool
spbt_scrape_client_send(DHTMeta_spbt_scrape_client &, const Key &,
                        const Contact &);

bool
spbt_has_infohash(DHTMeta_spbt_scrape_client &, const Infohash &ih);

} // namespace dht

#endif
