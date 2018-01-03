#ifndef SP_MAINLINE_DHT_OPTIONS_H
#define SP_MAINLINE_DHT_OPTIONS_H

#include "util.h"

namespace dht {
struct Options {
  Contact *bind_ip;
  sp::list<Contact> bootstrap;

  Options();
};

bool
parse(int, char **, Options &) noexcept;

} // namespace dht

#endif
