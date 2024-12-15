#include "upnp_miniupnp.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <util/conversions.h>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

struct sp_upnp {
  struct UPNPUrls upnp_urls;
  struct IGDdatas upnp_data;
  struct in_addr my;
  struct in_addr wan;
};

struct sp_upnp *
sp_upnp_new(void) {
  struct sp_upnp *self = NULL;
  struct UPNPDev *upnp_devs = NULL;

  int timeout = 2000; // ms
  const char *multicastif = NULL;
  const char *minissdpdsock = NULL;
  int ipv6 = 0; // 0 = IPv4, 1 = IPv6
  const uint8_t ttl = 20;
  // UPNP_LOCAL_PORT_ANY (0), or UPNP_LOCAL_PORT_SAME (1) as an alias for 1900
  const int localport = UPNP_LOCAL_PORT_ANY;
  int error = 0;

  if (!(self = (struct sp_upnp *)calloc(1, sizeof(*self)))) {
    return NULL;
  }

  upnp_devs = upnpDiscover(timeout, multicastif, minissdpdsock, localport, ipv6,
                           ttl, &error);
  if (upnp_devs) {
    int r;
    char lanaddr[64] = "\0";      /* my ip address on the LAN */
    char wanaddr[64] = "0.0.0.0"; /* up address of the IGD on the WAN */

    /*
     * #define UPNP_NO_IGD (0)
     * #define UPNP_CONNECTED_IGD (1)
     * #define UPNP_PRIVATEIP_IGD (2)
     * #define UPNP_DISCONNECTED_IGD (3)
     * #define UPNP_UNKNOWN_DEVICE (4)
     *
     *     0 = NO IGD found (UPNP_NO_IGD)
     *     1 = A valid connected IGD has been found (UPNP_CONNECTED_IGD)
     *     2 = A valid connected IGD has been found but its
     *         IP address is reserved (non routable) (UPNP_PRIVATEIP_IGD)
     *     3 = A valid IGD has been found but it reported as
     *         not connected (UPNP_DISCONNECTED_IGD)
     *     4 = an UPnP device has been found but was not recognized as an IGD
     *         (UPNP_UNKNOWN_DEVICE)
     *
     * In any non zero return case, the urls and data structures passed as
     * parameters are set. Donc forget to call FreeUPNPUrls(urls) to free
     * allocated memory.
     *
     * MINIUPNP_LIBSPEC int
     * UPNP_GetValidIGD(struct UPNPDev * devlist,
     *                  struct UPNPUrls * urls,
     *                  struct IGDdatas * data,
     *                  char * lanaddr, int lanaddrlen,
     *                  char * wanaddr, int wanaddrlen);
     */

#if (MINIUPNPC_API_VERSION >= 18)
    // Retrieve a valid Internet Gateway Device
    r = UPNP_GetValidIGD(upnp_devs, &self->upnp_urls, &self->upnp_data, lanaddr,
                         sizeof(lanaddr), wanaddr, sizeof(wanaddr));
#else
    r = UPNP_GetValidIGD(upnp_devs, &self->upnp_urls, &self->pnp_data, lanaddr,
                         sizeof(lanaddr));
#endif
    if (r == 1 || r == 2) {
      if (!inet_pton(AF_INET, lanaddr, &self->my)) {
        fprintf(stderr, "lan[%s]: %s (%d)\n", lanaddr, strerror(errno), error);
        goto Lerr;
      }
#if (MINIUPNPC_API_VERSION >= 18)
#else
      r = UPNP_GetExternalIPAddress(self->upnp_urls.controlURL,
                                    self->upnp_data.first.servicetype, wanaddr);
      if (r != UPNPCOMMAND_SUCCESS) {
        fprintf(stderr, "GetExternalIPAddress: %s (%d)\n", strerror(errno),
                error);
      }
#endif
      if (!inet_pton(AF_INET, wanaddr, &self->wan)) {
        fprintf(stderr, "wan[%s]: %s (%d)\n", wanaddr, strerror(errno), error);
        goto Lerr;
      }
      freeUPNPDevlist(upnp_devs);
      return self;
    }
  } else {
    fprintf(stderr, "upnpDiscover: error: %s (%d)\n", strupnperror(error),
            error);
  }
Lerr:
  freeUPNPDevlist(upnp_devs);
  sp_upnp_free(&self);
  return NULL;
}

int
sp_upnp_create_port_mapping(struct sp_upnp *self, //
                            struct sockaddr_in local, uint16_t *external_port,
                            const char *proto, uint32_t *leaseDuration,
                            const char *context) {
  char comment[128]{};
  char eport_in[16]{};
  char eport_out[16]{};
  char duration_in[16]{};
  char duration_out[16]{};
  int r;

  const char *eport = eport_in;
  char iaddr[64]{};
  char iport[16]{};

  const char *remoteHost = NULL; // ???

  sprintf(duration_in, "%u",
          *leaseDuration); // recommended 0 as some NAT implementations may not
                           // support another value

  assert(strcmp(proto, "TCP") == 0 || strcmp(proto, "UDP") == 0);
  assert(local.sin_family == AF_INET);
  if (!inet_ntop(AF_INET, &local.sin_addr, iaddr, (socklen_t)sizeof(iaddr))) {
    return -1;
  }
  sprintf(iport, "%d", ntohs(local.sin_port));
  printf("%s:%s\n", iaddr, iport);

  if (*external_port == 0) {
    // Note: AddAnyPortMapping does not seem to work
    *external_port =
        (uint16_t)(1024 + ((random() & 0x7ffffffL) % (0xffff - 1024)));
  }
  sprintf(eport_in, "%u", *external_port);
  sprintf(comment, "%s-%u", context, *external_port);

  if (*external_port == 0) {
    r = UPNP_AddAnyPortMapping(
        self->upnp_urls.controlURL, self->upnp_data.first.servicetype, eport,
        iport, iaddr, comment, proto, remoteHost, duration_in, eport_out);
    if (r == UPNPCOMMAND_SUCCESS) {
      eport = eport_out;
    } else {
      fprintf(stderr, "AddAnyPortMapping(%s, %s, %s): %s (%d)\n", eport, iport,
              iaddr, strupnperror(r), r);
      return -2;
    }
  } else {
    /* if desc is NULL, it will be defaulted to "libminiupnpc"
     * remoteHost is usually NULL because IGD don't support it.
     *
     * Return values :
     * 0 : SUCCESS
     * NON ZERO : ERROR. Either an UPnP error code or an unknown error.
     *
     * List of possible UPnP errors for AddPortMapping :
     * errorCode errorDescription (short) - Description (long)
     * 402 Invalid Args - See UPnP Device Architecture section on Control.
     * 501 Action Failed - See UPnP Device Architecture section on Control.
     * 606 Action not authorized - The action requested REQUIRES authorization
     * and the sender was not authorized. 715 WildCardNotPermittedInSrcIP - The
     * source IP address cannot be wild-carded 716 WildCardNotPermittedInExtPort
     * - The external port cannot be wild-carded 718 ConflictInMappingEntry -
     * The port mapping entry specified conflicts with a mapping assigned
     * previously to another client 724 SamePortValuesRequired - Internal and
     * External port values must be the same 725 OnlyPermanentLeasesSupported -
     * The NAT implementation only supports permanent lease times on port
     * mappings 726 RemoteHostOnlySupportsWildcard - RemoteHost must be a
     * wildcard and cannot be a specific IP address or DNS name 727
     * ExternalPortOnlySupportsWildcard - ExternalPort must be a wildcard and
     *                                        cannot be a specific port value
     * 728 NoPortMapsAvailable - There are not enough free ports available to
     *                           complete port mapping.
     * 729 ConflictWithOtherMechanisms - Attempted port mapping is not allowed
     *                                   due to conflict with other mechanisms.
     * 732 WildCardNotPermittedInIntPort - The internal port cannot be
     * wild-carded
     */
    r = UPNP_AddPortMapping(self->upnp_urls.controlURL,
                            self->upnp_data.first.servicetype, eport, iport,
                            iaddr, comment, proto, remoteHost, duration_in);
    if (r != UPNPCOMMAND_SUCCESS) {
      fprintf(stderr, "AddPortMapping(%s, %s, %s): %s (%d)\n", eport, iport,
              iaddr, strupnperror(r), r);
      return -2;
    }
  }

  {
    char intClient[40]{}; // mapping local ip
    char intPort[6]{};    // mapping local port
    /* retrieves an existing port mapping
     * params :
     *  in   controlURL,
     *  in   servicetype
     *  in   extPort
     *  in   proto
     *  in   remoteHost
     *  out  intClient (16 bytes)
     *  out  intPort (6 bytes)
     *  out  desc (80 bytes)
     *  out  enabled (4 bytes)
     *  out  leaseDuration (16 bytes)
     *
     * return value :
     * UPNPCOMMAND_SUCCESS, UPNPCOMMAND_INVALID_ARGS, UPNPCOMMAND_UNKNOWN_ERROR
     * or a UPnP Error Code.
     *
     * List of possible UPnP errors for _GetSpecificPortMappingEntry :
     * 402 Invalid Args - See UPnP Device Architecture section on Control.
     * 501 Action Failed - See UPnP Device Architecture section on Control.
     * 606 Action not authorized - The action requested REQUIRES authorization
     *                             and the sender was not authorized.
     * 714 NoSuchEntryInArray - The specified value does not exist in the array.
     */
    r = UPNP_GetSpecificPortMappingEntry(
        self->upnp_urls.controlURL, self->upnp_data.first.servicetype, eport,
        proto, remoteHost, intClient, intPort, NULL /*desc*/, NULL /*enabled*/,
        duration_out);
    strcpy(eport_out, eport);
    if (r != UPNPCOMMAND_SUCCESS) {
      fprintf(stderr, "GetSpecificPortMappingEntry(): %s (%d)\n",
              strupnperror(r), r);
      return -3;
    }
    printf("InternalIP:Port = %s:%s\n", intClient, intPort);
    printf("external %s is redirected to internal %s:%s (duration=%s)\n", proto,
           intClient, intPort, duration_out);
  }

  sp::parse_int(duration_out, duration_out + strlen(duration_out),
                *leaseDuration);
  sp::parse_int(eport_out, eport_out + strlen(eport_out), *external_port);

  return 0;
}

int
sp_upnp_delete_port_mapping(struct sp_upnp *self, uint16_t external_port,
                            const char *proto) {
  int r;
  char eport[16]{};
  const char *remoteHost = NULL;

  assert(self);
  assert(proto && (strcmp(proto, "TCP") == 0 || strcmp(proto, "UDP") == 0));

  sprintf(eport, "%d", external_port);

  r = UPNP_DeletePortMapping(self->upnp_urls.controlURL,
                             self->upnp_data.first.servicetype, eport, proto,
                             remoteHost);
  if (r != UPNPCOMMAND_SUCCESS) {
    fprintf(stderr, "UPNP_DeletePortMapping(): %s (%d)\n", strupnperror(r), r);
    return -2;
  }

  return 0;
}

struct in_addr
sp_upnp_external_ip(const struct sp_upnp *self) {
  return self->wan;
}

struct in_addr
sp_upnp_local_ip(const struct sp_upnp *self) {
  return self->my;
}

int
sp_upnp_free(struct sp_upnp **pself) {
  if (pself && *pself) {
    struct sp_upnp *self = *pself;

    FreeUPNPUrls(&self->upnp_urls);
    free(self);
    *pself = NULL;
  }

  return 0;
}
