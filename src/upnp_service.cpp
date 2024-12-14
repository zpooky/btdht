#include "upnp_service.h"
#include "net_util.h"
#include "tcp.h"
#include "udp.h"
#include "upnp.h"
#include "util.h"
#include <arpa/inet.h>
#include <cstddef>
#include <ifaddrs.h>
#include <inttypes.h>
#include <linux/if_link.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

namespace dht_upnp {
#if 0
static fd
open_upnp(const Ip &ip) {

  {
    Contact gateway{ip, Port(48353)};
    fd tcp = tcp::connect(gateway, tcp::Mode::BLOCKING);
    if (tcp) {
      return tcp;
    }
  }
  {
    Contact gateway{ip, Port(49153)};
    fd tcp = tcp::connect(gateway, tcp::Mode::BLOCKING);
    if (tcp) {
      return tcp;
    }
  }
  return fd{-1};
}

static bool
send(const upnp::upnp &in, const Ip &ip) noexcept {
  fd tcp = open_upnp(ip);
  if (!tcp) {
    printf("failed open: %s\n", to_string(ip));
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
  printf("%s: '%.*s': %zu\n", __func__, int(buf.length), buf.raw, buf.length);
  return true;
}

#endif

#if 0
static bool
guess_default_gateway(const sockaddr_in &ip, const sockaddr_in &netmask,
                      in_addr &result) noexcept {
  ::in_addr network = sp::network_for(ip.sin_addr, netmask.sin_addr);

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

      auto me = (::sockaddr_in *)it->ifa_addr;
      auto netmask = (::sockaddr_in *)it->ifa_netmask;

      if (!sp::is_private_address(me->sin_addr, netmask->sin_addr)) {
        printf("UPNP !is_private_address: me:%s\n", to_string(*me));
        printf("UPNP !is_private_address: mask:%s\n", to_string(*netmask));
        goto Lnext;
      }

      ::in_addr def_gateway{};
      if (!guess_default_gateway(*me, *netmask, def_gateway)) {
        goto Lnext;
      }

      Contact gateway;
      Ip g{ntohl(def_gateway.s_addr)};
      if (!to_contact(def_gateway, Port(48353), gateway)) {
        assertx(false);
        goto Lnext;
      }

      printf("gateway %s\n", to_string(gateway));
      printf("netmask %s\n", to_string(*netmask));
      printf("%s\n", it->ifa_name);

      Contact current_listen;
      if (!net::local(self.client.udp, current_listen)) {
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
      ctx.external = ctx.local + Port(1);
      ctx.ip = local.ip;
      if (!send(ctx, g)) {
        goto Lnext;
      }
    }
  Lnext:
    it = it->ifa_next;
  } // while

  ::freeifaddrs(ifaddr);

  return true;
}
#else
static bool
for_if_send(dht::DHT &self, const sp::Seconds &lease) noexcept {
  bool result = true;
  if (self.upnp) {
    sockaddr_in sock_local{};
    socklen_t slen = sizeof(sock_local);

    int ret =
        ::getsockname(int(self.client.udp), (sockaddr *)&sock_local, &slen);
    if (ret < 0) {
      return false;
    }
    struct in_addr lan = sp_upnp_local_ip(self.upnp);

    const char *local_str = to_string(sock_local);
    fprintf(stderr, "%s:sock_local[%s]\n", __func__, local_str);
    local_str = to_string(lan);
    fprintf(stderr, "%s:upnp lan[%s]\n", __func__, local_str);

    sock_local.sin_addr = lan;
    sock_local.sin_family = AF_INET;

    assertx(lease.value < UINT32_MAX);
    uint32_t ilease = (uint32_t)lease.value;
    int res = sp_upnp_create_port_mapping(self.upnp, sock_local,
                                          &self.upnp_external_port, "UDP",
                                          &ilease, "spdht");
    // assertx(res == 0);
  }

  return result;
}

#endif

static Timestamp
scheduled_upnp(dht::DHT &self, sp::Buffer &) noexcept {

  if (self.now >= self.upnp_expiry) {
    const sp::Seconds lease(sp::Hours(1));
    for_if_send(self, lease);
    self.upnp_expiry = self.now + lease;
  }

  assertx(self.upnp_expiry > self.now);
  return Timestamp(self.upnp_expiry);
}

bool
setup(dht::Modules &modules) noexcept {
  insert(modules.awake.on_awake, scheduled_upnp);
  return true;
}
} // namespace dht_upnp
