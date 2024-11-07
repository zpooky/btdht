#ifndef SP_MAINLINE_DHT_SPBT_SCRAPE_CLIENT_H
#define SP_MAINLINE_DHT_SPBT_SCRAPE_CLIENT_H

#include "util.h"
#include <io/fd.h>
#include <sys/un.h>

namespace dht {
//=====================================
struct DHTMeta_spbt_scrape_client {
  sp::fd unix_socket_file;
  void *cache;
  sp::fd dir_fd;
  sockaddr_un name{};

  DHTMeta_spbt_scrape_client();
  ~DHTMeta_spbt_scrape_client() {
  }
};

int
spbt_scrape_client_send(DHTMeta_spbt_scrape_client &, const Key &,
                        const Contact &);

bool
spbt_has_infohash(DHTMeta_spbt_scrape_client &, const Infohash &ih);
} // namespace dht

#endif
