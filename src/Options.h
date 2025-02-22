#ifndef SP_MAINLINE_DHT_OPTIONS_H
#define SP_MAINLINE_DHT_OPTIONS_H

#include "util.h"
#include <limits.h>

namespace dht {
struct Options {
  Port port;
  sp::LinkedList<Contact> bootstrap;
  char dump_file[PATH_MAX];
  char local_socket[PATH_MAX];
  char publish_socket[PATH_MAX];
  char db_path[PATH_MAX];
  char scrape_socket_path[PATH_MAX];
  bool systemd;

  Options();
};

bool
parse(Options &, int, char **) noexcept;

// TODO rename to argumetns
} // namespace dht

#endif
