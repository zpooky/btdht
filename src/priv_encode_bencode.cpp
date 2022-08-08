#include "priv_encode_bencode.h"
#include "encode_bencode.h"
#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>

namespace sp {
namespace bencode {
template <typename Buffer>
bool
priv::e<Buffer>::value(Buffer &b, const dht::NodeId &id) noexcept {
  return bencode::e<Buffer>::value(b, id.id, sizeof(id.id));
}

template <typename Buffer>
bool
priv::e<Buffer>::pair(Buffer &buf, const char *key,
                      const dht::NodeId &value) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::priv::e<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
priv::e<Buffer>::value(Buffer &buf, const dht::Node &node) noexcept {
  return bencode::e<Buffer>::dict(buf, [&node](Buffer &b) { //
    if (!bencode::e<Buffer>::pair(b, "id", node.id.id, sizeof(node.id.id))) {
      return false;
    }

    if (!bencode::priv::e<Buffer>::pair(b, "contact", node.contact)) {
      return false;
    }

    // if (!bencode::e<Buffer>::pair(b, "good", node.good)) {
    //   return false;
    // }
    // if (!bencode::e<Buffer>::pair(b, "outstanding", node.outstanding)) {
    //   return false;
    // }

    return true;
  });
}

//=====================================
template <typename Buffer>
bool
priv::e<Buffer>::value(Buffer &buf, const dht::Bucket &t) noexcept {
  return bencode::e<Buffer>::list(buf, (void *)&t, [](Buffer &b, void *arg) { //
    auto *a = (dht::Bucket *)arg;

    return for_all(*a, [&b](const dht::Node &node) { //
      return value(b, node);
    });
  });
}

//=====================================
template <typename Buffer>
bool
priv::e<Buffer>::value(Buffer &buf, const dht::RoutingTable &t) noexcept {
  // used by dump

  return bencode::e<Buffer>::dict(buf, [&t](Buffer &b) {
    // dht::Infohash id; // TODO
    // if (!pair(b, "id", id.id, sizeof(id.id))) {
    //   return false;
    // }

    if (!bencode::e<Buffer>::value(b, "bucket")) {
      return false;
    }

    if (!bencode::priv::e<Buffer>::value(b, t.bucket)) {
      return false;
    }

    return true;
  });
} // bencode::priv::e::value()

template <typename Buffer>
bool
priv::e<Buffer>::pair(Buffer &buf, const char *key,
                      const dht::RoutingTable *t) noexcept {
  // used by dump
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::list(buf, (void *)&t, [](Buffer &b, void *arg) { //
    const auto *a = (const dht::RoutingTable *)arg;

    return dht::for_all(a, [&b](const dht::RoutingTable &p) { //
      return value(b, p);
    });
  });
} // bencode::priv::e::pair()

//=====================================
template <typename Buffer>
bool
priv::e<Buffer>::value(Buffer &buf, const dht::Peer &t) noexcept {
  // used by dump

  return bencode::e<Buffer>::dict(buf, [&t](Buffer &b) { //
    if (!bencode::e<Buffer>::value(b, "contact")) {
      return false;
    }
    if (!bencode::priv::e<Buffer>::value(b, t.contact)) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "activity", t.activity.value)) {
      return false;
    }

    return true;
  });
} // bencode::priv::e::value()

//=====================================
template <typename Buffer>
bool
priv::e<Buffer>::value(Buffer &buf, const dht::KeyValue &t) noexcept {
  // used by dump
  return bencode::e<Buffer>::dict(buf, [&t](Buffer &b) { //
    if (!bencode::e<Buffer>::pair(b, "id", t.id.id, sizeof(t.id.id))) {
      return false;
    }

    if (!bencode::e<Buffer>::value(b, "list")) {
      return false;
    }

    return bencode::e<Buffer>::list(
        b, (void *)&t, [](Buffer &b2, void *arg) { //
          const auto *a = (const dht::KeyValue *)arg;

          return for_all(a->peers, [&b2](const dht::Peer &p) { //
            return value(b2, p);
          });
        });
  });
} // bencode::priv::e::value()

// template <typename Buffer>
// bool
// priv::e<Buffer>::pair(Buffer &buf, const char *key,
//                       const dht::KeyValue &t) noexcept {
//   // used by dump
//   if (!bencode::e<Buffer>::value(buf, key)) {
//     return false;
//   }
//
//   if (!bencode::e<Buffer>::value(buf, t)) {
//     return false;
//   }
//
//   return true;
// } // bencode::priv::e::pair()

//=====================================
template <typename Buffer>
bool
priv::e<Buffer>::pair(Buffer &buf, const char *key,
                      const dht::StatTrafic &t) noexcept {
  // used by statistics
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::dict(buf, [&t](Buffer &b) {
    if (!bencode::e<Buffer>::pair(b, "ping", t.ping)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "find_node", t.find_node)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "get_peers", t.get_peers)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "announce_peer", t.announce_peer)) {
      return false;
    }
    if (!bencode::e<Buffer>::pair(b, "error", t.error)) {
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
priv::e<Buffer>::pair(Buffer &buf, const char *key,
                      const dht::StatDirection &d) noexcept {
  // used by statistics
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::dict(buf, [&d](Buffer &b) {
    if (!bencode::priv::e<Buffer>::pair(b, "request", d.request)) {
      return false;
    }

    if (!bencode::priv::e<Buffer>::pair(b, "response_timeout",
                                        d.response_timeout)) {
      return false;
    }

    if (!bencode::priv::e<Buffer>::pair(b, "response", d.response)) {
      return false;
    }

    if (!bencode::e<Buffer>::pair(b, "parse_error", d.parse_error)) {
      return false;
    }
    // TODO
    return true;
  });
}

//=====================================
template <typename Buffer>
static bool
value_contact(Buffer &buffer, const Contact &p) noexcept {
  Ipv4 ip = htonl(p.ip.ipv4);
  Port port = htons(p.port);
  if (!bencode::e<Buffer>::value(buffer, "ipv4")) {
    return false;
  }
  if (!bencode::e<Buffer>::value(buffer, ip)) {
    return false;
  }

  if (!bencode::e<Buffer>::value(buffer, "port")) {
    return false;
  }
  if (!bencode::e<Buffer>::value(buffer, port)) {
    return false;
  }

  return true;
}

template <typename Buffer>
bool
priv::e<Buffer>::value(Buffer &b, const Contact &p) noexcept {
  // TODO
  assertx(p.ip.type == IpType::IPV4);

  return bencode::e<Buffer>::dict(b, [&p](Buffer &buffer) {
    //
    return value_contact(buffer, p);
  });
} // bencode::e::value()

template <typename Buffer>
bool
priv::e<Buffer>::pair(Buffer &buf, const char *key, const Contact &p) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return value(buf, p);
} // bencode::e::pair()

} // namespace bencode

template struct bencode::priv::e<sp::CircularByteBuffer>;
template struct bencode::priv::e<sp::BytesView>;
template struct bencode::priv::e<sp::Sink>;
// template struct bencode::priv::e<sp::Sink>;
} // namespace sp
