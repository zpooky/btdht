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
for_if_send(dht::DHT &self, const sp::Seconds &) noexcept {
  char aLanAddr[64] = {};
  char pPort[16] = {};
  bool result = false;
  struct UPNPUrls upnp_urls = {};
  struct IGDdatas upnp_data = {};
  struct UPNPDev *upnp_devs = nullptr;

  {
    Contact current_listen;
    if (!net::local(self.client.udp, current_listen)) {
      assertx(false);
      goto Lerr;
    }

    sprintf(pPort, "%d", current_listen.port);

    /* upnpDiscover() discover UPnP devices on the network.
     * The discovered devices are returned as a chained list.
     * It is up to the caller to free the list with freeUPNPDevlist().
     *
     * delay: (in millisecond) is the maximum time for waiting any device
     * response. If available, device list will be obtained from MiniSSDPd.
     *
     * Default path for minissdpd socket will be used if minissdpdsock argument
     * is NULL.
     *
     * If multicastif is not NULL, it will be used instead of the default
     * multicast interface for sending SSDP discover packets.
     *
     * If localport is set to UPNP_LOCAL_PORT_SAME(1) SSDP packets will be sent
     * from the source port 1900 (same as destination port), if set to
     * UPNP_LOCAL_PORT_ANY(0) system assign a source port, any other value will
     * be attempted as the source port.
     * "searchalltypes" parameter is useful when searching several types,
     *
     * if 0, the discovery will stop with the first type returning results.
     * TTL should default to 2. */
    /*
     * MINIUPNP_LIBSPEC struct UPNPDev *
     * upnpDiscover(int delay, const char * multicastif, const char *
     * minissdpdsock, int localport, int ipv6, unsigned char ttl, int * error);
     */

    int timeout = 2000; // ms
    const char *multicastif = nullptr;
    const char *minissdpdsock = nullptr;
    int ipv6 = 0; // 0 = IPv4, 1 = IPv6
    const uint8_t ttl = 20;
    // UPNP_LOCAL_PORT_ANY (0), or UPNP_LOCAL_PORT_SAME (1) as an alias for 1900
    const int localport = UPNP_LOCAL_PORT_ANY;
    int error = 0;
    // localport, ipv6, ttl, &error
    upnp_devs = upnpDiscover(timeout, multicastif, minissdpdsock, localport,
                             ipv6, ttl, &error);
    if (upnp_devs) {
#if 0
      struct UPNPDev *upnp_dev = upnp_devs;
      printf("upnpDiscover: error: %s (%d)\n", strupnperror(error), error);

      while (upnp_dev) {
        if (strstr(upnp_dev->st, "InternetGatewayDevice")) {
          break;
        }
        upnp_dev = upnp_dev->pNext;
      }
      if (!upnp_dev) {
        upnp_dev = upnp_devs; // default
      }

      {
        struct UPNPUrls *urls = nullptr;
        struct IGDdatas *datas = nullptr;

        int descXMLsize = 0;
        int descXMLstatus = 0;
        char *descXML = (char *)miniwget(upnp_dev->descURL, &descXMLsize,
                                         upnp_dev->scope_id, &descXMLstatus);
        if (descXML) {
          parserootdesc(descXML, descXMLsize, datas);
          free(descXML);
          descXML = 0;
          GetUPNPUrls(urls, datas, upnp_dev->descURL, upnp_dev->scope_id);
        }

        // Get LAN IP address that connects to the router
        char lanaddr[64] = "unset";

        // possible "status" values:
        // -1 = Internal error
        //  0 = NO IGD found
        //  1 = A valid connected IGD has been found
        //  2 = A valid connected IGD has been found but its IP address is
        //  reserved (non routable) 3 = A valid IGD has been found but it
        //  reported as not connected 4 = an UPnP device has been found but was
        //  not recognized as an IGD
      }

      // Retrieve a valid Internet Gateway Device
#if (MINIUPNPC_API_VERSION >= 18)
      int status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, aLanAddr,
                                    sizeof(aLanAddr), nullptr, 0);
#else
      int status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, aLanAddr,
                                    sizeof(aLanAddr));
#endif
      printf("status=%d, lan_addr=%s\n", status, aLanAddr);

      if (status == 1 || status == 2) {
        printf("found valid IGD: %s\n", upnp_urls.controlURL);
        error = UPNP_AddPortMapping(
            upnp_urls.controlURL, upnp_data.first.servicetype,
            pPort, // external port
            pPort, // internal port
            aLanAddr, "spdht", "UDP",
            0,  // remote host
            "0" // lease duration, recommended 0 as some NAT
                // implementations may not support another value
        );

        if (error) {
          printf("failed to map port\n");
          printf("error: %s\n", strupnperror(error));
        } else {
          printf("%s:pPort[%.*s]aLanAddr[%.*s]\n", __func__, (int)16, pPort,
                 (int)64, aLanAddr);
        }
      } else {
        printf("no valid IGD found\n");
      }
#endif
    }

    // TODO delete in dtor
    // error = UPNP_DeletePortMapping(
    //     upnp_urls.controlURL, upnp_data.first.servicetype, pPort, "UDP", 0);
  }

  result = true;
Lerr:
  FreeUPNPUrls(&upnp_urls);
  freeUPNPDevlist(upnp_devs);
  return result;
}

#endif

static Timestamp
scheduled_upnp(dht::DHT &self, sp::Buffer &) noexcept {
  const sp::Seconds lease(sp::Hours(1));
  Timestamp expiry(self.upnp_sent + lease);

  if (self.now >= expiry) {
    for_if_send(self, lease);
    self.upnp_sent = self.now;
    expiry = self.upnp_sent + lease;
  }

  assertx(expiry > self.now);
  return Timestamp(expiry);
}

bool
setup(dht::Modules &modules) noexcept {
  insert(modules.awake.on_awake, scheduled_upnp);
  return true;
}
} // namespace dht_upnp
