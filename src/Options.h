#ifndef SP_MAINLINE_DHT_OPTIONS_H
#define SP_MAINLINE_DHT_OPTIONS_H

#include "util.h"
#include <limits.h>

namespace dht {
struct Options {
  Port port;
  sp::list<Contact> bootstrap;
  char dump_file[PATH_MAX];
  char local_socket[PATH_MAX];

  Options();
};

bool
parse(Options &, int, char **) noexcept;

} // namespace dht

#endif
