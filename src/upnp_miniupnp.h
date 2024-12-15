#ifndef SP_MAINLINE_DHT_UPNP_MINIUPNP_H
#define SP_MAINLINE_DHT_UPNP_MINIUPNP_H

// #include <arpa/inet.h>
// #include <netinet/in.h>
// #include <sys/socket.h>
#include <stdint.h>

struct sp_upnp;

struct sp_upnp *
sp_upnp_new(void);

// returns external_port
// leaseDuration in seconds
int
sp_upnp_create_port_mapping(struct sp_upnp *self, struct sockaddr_in local,
                            uint16_t *external_port, const char *proto,
                            uint32_t *leaseDuration, const char *context);

int
sp_upnp_delete_port_mapping(struct sp_upnp *self, uint16_t external_port,
                            const char *proto);

struct in_addr
sp_upnp_external_ip(const struct sp_upnp *);

struct in_addr
sp_upnp_local_ip(const struct sp_upnp *);

int
sp_upnp_free(struct sp_upnp **);

#endif
