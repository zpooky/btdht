#ifndef SP_MAINLINE_DHT_KRPC_H
#define SP_MAINLINE_DHT_KRPC_H

#include "shared.h"

namespace krpc {
enum class MessageType : char { query = 'q', response = 'r', error = 'e' };
enum class Query { ping, find_node, get_peers, announce_peer };
enum class Error {
  generic_error = 201,
  server_error = 202,
  protocol_error = 203,
  method_unknown = 204
};

namespace request {
bool
ping(sp::Buffer &, const dht::NodeId &sender) noexcept;

bool
find_node(sp::Buffer &, const dht::NodeId &self,
          const dht::NodeId &search) noexcept;

bool
get_peers(sp::Buffer &, const dht::NodeId &id, const char *infohash) noexcept;

bool
announce_peer(sp::Buffer &, const dht::NodeId &id, bool implied_port,
              const char *infohash, std::uint16_t port,
              const char *token) noexcept;
} // namespace request

namespace response {
bool
ping(sp::Buffer &, const dht::NodeId &receiver) noexcept;

bool
find_node(sp::Buffer &, const dht::NodeId &id, const char *target) noexcept;

bool
announce_peer(sp::Buffer &, const dht::NodeId &) noexcept;
} // namespace response

} // namespace krpc

#endif
