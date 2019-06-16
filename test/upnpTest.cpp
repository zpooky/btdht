#include "util.h"
#include <gtest/gtest.h>
#include <net_util.h>

TEST(upnpTest, test) {

  Contact a_netmask;
  ::sockaddr_in a_netmask_addr;

  Contact a_network;
  ::sockaddr_in a_network_addr;

  ASSERT_TRUE(to_contact("255.255.0.0:0", a_netmask));
  ASSERT_TRUE(to_sockaddr(a_netmask, a_netmask_addr));

  ASSERT_TRUE(to_contact("192.168.0.116:0", a_network));
  ASSERT_TRUE(to_sockaddr(a_network, a_network_addr));

  ASSERT_TRUE(
      sp::is_private_address(a_network_addr.sin_addr, a_netmask_addr.sin_addr));
}
