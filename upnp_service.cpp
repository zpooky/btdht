#include "tcp.h"
#include "udp.h"
#include "upnp.h"
#include "upnp_service.h"
#include <arpa/inet.h>
#include <cstddef>
#include <ifaddrs.h>
#include <inttypes.h>
#include <linux/if_link.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

namespace dht_upnp {

static bool
send(const upnp::upnp &in, const Contact &gateway) noexcept {
  fd tcp = tcp::connect(gateway, tcp::Mode::BLOCKING);
  if (!tcp) {
    printf("connect[%s]\n", to_string(gateway));
    return false;
  }

  if (!upnp::http_add_port(tcp, in)) {
    printf("http_add_port\n");
    return false;
  }

  sp::StaticBytesView<1024 * 2> buf;
  if (tcp::read(tcp, buf) != 0) {
    printf("read\n");
    return false;
  }

  flip(buf);
  printf("'%.*s': %zu\n", int(buf.length), buf.raw, buf.length);
  return true;
}

#if 0
struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};
struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET, AF_INET6
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // see struct in_addr, below
    char             sin_zero[8];  // zero this if you want to
};
struct in_addr {
    unsigned long s_addr;          // load with inet_pton()
};
struct sockaddr_in6 {
    u_int16_t       sin6_family;   // address family, AF_INET6
    u_int16_t       sin6_port;     // port number, Network Byte Order
    u_int32_t       sin6_flowinfo; // IPv6 flow information
    struct in6_addr sin6_addr;     // IPv6 address
    u_int32_t       sin6_scope_id; // Scope ID
};
#endif

static bool
guess_default_gateway(const sockaddr_in &ip, const sockaddr_in &netmask,
                      in_addr &result) noexcept {
  ::in_addr network{};
  network.s_addr = ip.sin_addr.s_addr & netmask.sin_addr.s_addr;

  ::in_addr first{};
  first.s_addr = 1 << (3 * 8);

  result.s_addr = network.s_addr | first.s_addr;
  return true;
}

static bool
for_if_send(dht::DHT &self, const sp::Seconds &lease) noexcept {
  // only makes sense for 0.0.0.0
  ::ifaddrs *ifaddr = nullptr;
  int res = ::getifaddrs(&ifaddr);
  if (res < 0) {
    return false;
  }

  auto it = ifaddr;
  while (it) {
    {
      auto addr = it->ifa_addr;
      if (!addr) {
        goto Lnext;
      }
      if (addr->sa_family != AF_INET) {
        goto Lnext;
      }
      if (strcmp(it->ifa_name, "lo") == 0) {
        goto Lnext;
      }

      auto me = (sockaddr_in *)it->ifa_addr;
      auto netmask = (sockaddr_in *)it->ifa_netmask;

      ::in_addr def_gateway{};
      if (!guess_default_gateway(*me, *netmask, def_gateway)) {
        goto Lnext;
      }

      Contact gateway;
      Port port = 48353;
      if (!to_contact(def_gateway, port, gateway)) {
        assertx(false);
        goto Lnext;
      }

      printf("gateway %s\n", to_string(gateway));
      printf("netmask %s\n", to_string(*netmask));
      printf("%s\n", it->ifa_name);

      Contact current_listen;
      if (!udp::local(self.client.udp, current_listen)) {
        assertx(false);
        goto Lnext;
      }

      Contact local;
      if (!to_contact(*me, local)) {
        assertx(false);
        goto Lnext;
      }
      local.port = current_listen.port;

      upnp::upnp ctx{lease};
      ctx.protocol = "udp";
      ctx.local = local.port;
      ctx.external = ctx.local + 1;
      ctx.ip = local.ip;
      if (!send(ctx, gateway)) {
        goto Lnext;
      }
    }
  Lnext:
    it = it->ifa_next;
  } // while

  ::freeifaddrs(ifaddr);

  return true;
} // namespace dht_upnp

static Timeout
scheduled_upnp(dht::DHT &self, sp::Buffer &) noexcept {
  const sp::Seconds lease(sp::Hours(1));
  Timestamp expiry(self.upnp_sent + lease);

  if (self.now >= expiry) {
    for_if_send(self, lease);
    self.upnp_sent = self.now;
    expiry = self.upnp_sent + lease;
  }

  return expiry - self.now;
}

bool
setup(dht::Modules &modules) noexcept {
  insert(modules.on_awake, scheduled_upnp);
  return true;
}
} // namespace dht_upnp
