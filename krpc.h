#ifndef SP_MAINLINE_DHT_KRPC_H
#define SP_MAINLINE_DHT_KRPC_H

#include "shared.h"
namespace bencode {
namespace e {
bool
value(sp::Buffer &buffer, const dht::Peer &p) noexcept;

bool
value(sp::Buffer &buffer, const dht::Node &node) noexcept;

bool
pair(sp::Buffer &, const char *, const dht::Peer *list) noexcept;

bool
pair(sp::Buffer &, const char *, const sp::list<dht::Node> &list) noexcept;

} // namespace e
} // namespace bencode

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
ping(sp::Buffer &, const Transaction &, //
     const dht::NodeId &sender) noexcept;

bool
find_node(sp::Buffer &, const Transaction &, //
          const dht::NodeId &self, const dht::NodeId &search) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, //
          const dht::NodeId &id, const dht::Infohash &infohash) noexcept;

bool
announce_peer(sp::Buffer &, const Transaction &, //
              const dht::NodeId &id, bool implied_port,
              const dht::Infohash &infohash, std::uint16_t port,
              const char *token) noexcept;
} // namespace request

namespace response {
bool
ping(sp::Buffer &, const Transaction &, //
     const dht::NodeId &receiver) noexcept;

bool
find_node(sp::Buffer &, const Transaction &, //
          const dht::NodeId &, const sp::list<dht::Node> &) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, //
          const dht::NodeId &id, const dht::Token &,
          const sp::list<dht::Node> &) noexcept;

bool
get_peers(sp::Buffer &, const Transaction &, //
          const dht::NodeId &id, const dht::Token &,
          const dht::Peer *) noexcept;

bool
announce_peer(sp::Buffer &, const Transaction &, //
              const dht::NodeId &) noexcept;
} // namespace response

namespace d {
template <typename F>
bool
krpc(bencode::d::Decoder &d, Transaction &tx, char (&msg_type)[16],
     char (&query)[16], F f) {
  return bencode::d::dict(d, [&tx, &msg_type, &query, f](auto &p) { //
    bool t = false;
    bool y = false;
    bool q = false;

  start:
    if (!t && bencode::d::pair(p, "t", tx.id)) {
      t = true;
      goto start;
    }
    if (!y && bencode::d::pair(p, "y", msg_type)) {
      y = true;
      goto start;
    }
    if (!q && bencode::d::pair(p, "q", query)) {
      q = true;
      goto start;
    }

    if (!(t && y && q)) {
      return false;
    }

    return f(p, tx, msg_type, query);
  });
}
} // namespace d

} // namespace krpc

#endif
