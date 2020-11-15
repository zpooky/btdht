#include "net_util.h"

#include "util.h"
#include <util/assert.h>

namespace sp {
//=====================================
::in_addr
network_for(::in_addr address, ::in_addr mask) noexcept {
  ::in_addr result{};

  result.s_addr = address.s_addr & mask.s_addr;
  return result;
}

//=====================================
static bool
is_class_a(::in_addr ip, ::in_addr mask) noexcept {

  Contact a_netmask;
  assertx_n(to_contact("255.0.0.0:0", a_netmask));
  ::sockaddr_in a_netmask_addr;
  assertx_n(to_sockaddr(a_netmask, a_netmask_addr));

  ::in_addr network = network_for(ip, a_netmask_addr.sin_addr);

  Contact a_network;
  assertx_n(to_contact("10.0.0.0:0", a_network));
  ::sockaddr_in a_network_addr;
  assertx_n(to_sockaddr(a_network, a_network_addr));

  if ((a_netmask_addr.sin_addr.s_addr & mask.s_addr) ==
      a_netmask_addr.sin_addr.s_addr) {
    if (a_network_addr.sin_addr.s_addr == network.s_addr) {
      return true;
    }
  }

  return false;
}

//=====================================
static bool
is_class_b(::in_addr, ::in_addr) {
  // TODO
  return false;
}

//=====================================
static bool
is_class_c(::in_addr ip, ::in_addr mask) {
  Contact c_netmask;
  assertx_n(to_contact("255.255.0.0:0", c_netmask));
  ::sockaddr_in c_netmask_addr;
  assertx_n(to_sockaddr(c_netmask, c_netmask_addr));

  ::in_addr network = network_for(ip, c_netmask_addr.sin_addr);

  Contact a_network;
  assertx_n(to_contact("192.168.0.0:0", a_network));
  ::sockaddr_in a_network_addr;
  assertx_n(to_sockaddr(a_network, a_network_addr));

  if ((c_netmask_addr.sin_addr.s_addr & mask.s_addr) ==
      c_netmask_addr.sin_addr.s_addr) {
    if (a_network_addr.sin_addr.s_addr == network.s_addr) {
      return true;
    }
  }

  return false;
}

//=====================================
bool
is_private_address(::in_addr ip, ::in_addr mask) noexcept {
  return is_class_a(ip, mask) || is_class_b(ip, mask) || is_class_c(ip, mask);
}

//=====================================
} // namespace sp
