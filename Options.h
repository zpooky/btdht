#ifndef SP_MAINLINE_DHT_OPTIONS_H
#define SP_MAINLINE_DHT_OPTIONS_H

#include "util.h"

namespace dht {
struct Options {
  Port port;
  sp::list<Contact> bootstrap;
  char dump_file[256];

  Options();
};

bool
parse(Options &, int, char **) noexcept;

} // namespace dht

#endif
