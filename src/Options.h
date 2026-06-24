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
static inline const char *
sp_debug_Options(const struct Options *in) {
  static char buf[4096] = {'\0'};
  snprintf(buf, sizeof(buf),
           "dump_file[%.*s]local_socket[%.*s]publish_socket[%.*s]db_path[%.*s]"
           "scrape_socket_path[%.*s]systemd[%s]",
           (int)PATH_MAX, in->dump_file, (int)PATH_MAX, in->local_socket,
           (int)PATH_MAX, in->publish_socket, (int)PATH_MAX, in->db_path,
           (int)PATH_MAX, in->scrape_socket_path,
           in->systemd ? "TRUE" : "FALSE");
  return buf;
}

bool
parse(Options &, int, char **) noexcept;

// TODO rename to argumetns
} // namespace dht

#endif
