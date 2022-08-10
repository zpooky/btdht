#include "encode_bencode.h"
#include <arpa/inet.h>
#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>

namespace sp {
//=====================================
template <typename Buffer, typename T>
static bool
raw_numeric(Buffer &buffer, const char *format, T in) noexcept {
  char b[64] = {0};
  int res = std::snprintf(b, sizeof(b), format, in);
  if (res < 0) {
    return false;
  }

  if (!write(buffer, b, (std::size_t)res)) {
    return false;
  }

  return true;
} // bencode::raw_numeric()

template <typename Buffer, typename T>
static bool
encode_integer(Buffer &buffer, const char *format, T in) noexcept {
  static_assert(std::is_integral<T>::value, "");

  if (!write(buffer, 'i')) {
    return false;
  }

  if (!raw_numeric(buffer, format, in)) {
    return false;
  }

  if (!write(buffer, 'e')) {
    return false;
  }

  return true;
} // bencode::encode_integer()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, bool in) noexcept {
  return encode_integer(buffer, "%d", in ? 1 : 0);
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::uint16_t in) noexcept {
  return encode_integer(buffer, "%hu", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::int16_t in) noexcept {
  return encode_integer(buffer, "%h", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::uint32_t in) noexcept {
  return encode_integer(buffer, "%u", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::int32_t in) noexcept {
  return encode_integer(buffer, "%d", in);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::uint64_t in) noexcept {
  return encode_integer(buffer, "%llu", in);
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &buffer, std::int64_t in) noexcept {
  return encode_integer(buffer, "%lld", in);
}

//=====================================
template <typename Buffer, typename T>
static bool
encode_raw(Buffer &buffer, const T *str, std::size_t length) noexcept {
  static_assert(
      std::is_same<T, char>::value || std::is_same<T, sp::byte>::value, "");

  if (!raw_numeric(buffer, "%zu", length)) {
    return false;
  }

  if (!write(buffer, ':')) {
    return false;
  }

  if (!write(buffer, str, length)) {
    return false;
  }

  return true;
} // bencode::e::encode_raw()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &b, const char *str) noexcept {
  return value(b, str, std::strlen(str));
}

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &b, const char *str, std::size_t l) noexcept {
  return encode_raw(b, str, l);
} // bencode::e::value()

template <typename Buffer>
bool
bencode::e<Buffer>::value(Buffer &b, const sp::byte *str,
                          std::size_t l) noexcept {
  return encode_raw(b, str, l);
} // bencode::e::value()

//=====================================
template <typename Buffer>
bool
bencode::e<Buffer>::value_compact(Buffer &buffer, const dht::Infohash *v,
                                  std::size_t length) noexcept {
  const std::size_t raw_length = length * sizeof(v->id);

  if (!raw_numeric(buffer, "%zu", raw_length)) {
    return false;
  }

  if (!write(buffer, ':')) {
    return false;
  }

  for (std::size_t a = 0; a < length; ++a) {
    if (!write(buffer, v[a].id, sizeof(v[a].id))) {
      return false;
    }
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_id_contact_compact(Buffer &b, const dht::Node **list,
                                             std::size_t length) noexcept {
  std::size_t raw_size = 0;

  for_all(list, length, [&raw_size](const auto &value) { //
    raw_size += serialize_size(value.contact) + sizeof(value.id.id);
    return true;
  });

  if (!raw_numeric(b, "%zu", raw_size)) {
    return false;
  }

  if (!write(b, ':')) {
    return false;
  }

  return for_all(list, length, [&b](const auto &value) {
    if (!write(b, value.id.id, sizeof(value.id.id))) {
      return false;
    }

    Ipv4 ip = htonl(value.contact.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      return false;
    }
    Port port = htons(value.contact.port);
    if (!write(b, &port, sizeof(port))) {
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_id_contact_compact(
    Buffer &b, const sp::list<dht::Node> &list) noexcept {
  std::size_t raw_size = 0;

  for_each(list, [&raw_size](const auto &value) { //
    raw_size += serialize_size(value.contact) + sizeof(value.id.id);
  });

  if (!raw_numeric(b, "%zu", raw_size)) {
    return false;
  }

  if (!write(b, ':')) {
    return false;
  }

  return for_all(list, [&b](const auto &value) {
    if (!write(b, value.id.id, sizeof(value.id.id))) {
      return false;
    }

    Ipv4 ip = htonl(value.contact.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      return false;
    }
    Port port = htons(value.contact.port);
    if (!write(b, &port, sizeof(port))) {
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_compact(Buffer &b,
                                  const sp::UinArray<Contact> &list) noexcept {
  std::size_t raw_size = 0;

  for_each(list, [&raw_size](const auto &value) {
    raw_size += serialize_size(value);
  });

  if (!raw_numeric(b, "%zu", raw_size)) {
    return false;
  }

  if (!write(b, ':')) {
    return false;
  }

  return for_all(list, [&b](const auto &value) {
    Ipv4 ip = htonl(value.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      return false;
    }
    Port port = htons(value.port);
    if (!write(b, &port, sizeof(port))) {
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_compact(Buffer &b,
                                  const sp::list<Contact> &list) noexcept {
  std::size_t raw_size = 0;

  for_each(list, [&raw_size](const auto &value) {
    raw_size += serialize_size(value);
  });

  if (!raw_numeric(b, "%zu", raw_size)) {
    return false;
  }

  if (!write(b, ':')) {
    return false;
  }

  return for_all(list, [&b](const auto &value) {
    Ipv4 ip = htonl(value.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      return false;
    }
    Port port = htons(value.port);
    if (!write(b, &port, sizeof(port))) {
      return false;
    }

    return true;
  });
}

//=====================================
template <typename Buffer, typename V>
static bool
generic_encodePair(Buffer &buffer, const char *key, V val) {

  if (!bencode::e<Buffer>::value(buffer, key)) {
    return false;
  }

  if (!bencode::e<Buffer>::value(buffer, val)) {
    return false;
  }

  return true;
} // bencode::e::generic_encodePair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key, bool value) noexcept {
  return generic_encodePair(buffer, key, value);
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::int16_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::uint16_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::int32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::uint32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::uint64_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         std::int64_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

//=====================================
template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buffer, const char *key,
                         const char *value) noexcept {
  return generic_encodePair(buffer, key, value);
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &buf, const char *key, const byte *value,
                         std::size_t l) noexcept {

  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  return bencode::e<Buffer>::value(buf, value, l);
}

//=====================================
template <typename Buffer>
bool
bencode::e<Buffer>::pair_compact(Buffer &b, const char *key,
                                 const dht::Infohash *v,
                                 std::size_t l) noexcept {
  if (!bencode::e<Buffer>::value(b, key)) {
    return false;
  }

  if (!bencode::e<Buffer>::value_compact(b, v, l)) {
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair_id_contact_compact(Buffer &b, const char *key,
                                            const dht::Node **list,
                                            std::size_t length) noexcept {
  if (!bencode::e<Buffer>::value(b, key)) {
    return false;
  }

  if (!bencode::e<Buffer>::value_id_contact_compact(b, list, length)) {
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair_id_contact_compact(
    Buffer &b, const char *key, const sp::list<dht::Node> &list) noexcept {
  if (!bencode::e<Buffer>::value(b, key)) {
    return false;
  }

  if (!bencode::e<Buffer>::value_id_contact_compact(b, list)) {
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
                                 const sp::UinArray<Contact> &list) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  if (!bencode::e<Buffer>::value_compact(buf, list)) {
    return false;
  }

  return true;
} // bencode::e::pair_compact()

template <typename Buffer>
bool
bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
                                 const sp::list<Contact> &list) noexcept {
  if (!bencode::e<Buffer>::value(buf, key)) {
    return false;
  }

  if (!bencode::e<Buffer>::value_compact(buf, list)) {
    return false;
  }

  return true;
}

//=====================================
template <typename Buffer>
bool
bencode::e<Buffer>::list(Buffer &buf, void *closure,
                         bool (*f)(Buffer &, void *)) noexcept {
  if (!write(buf, 'l')) {
    return false;
  }

  if (!f(buf, closure)) {
    return false;
  }

  if (!write(buf, 'e')) {
    return false;
  }

  return true;
}

//=====================================
#if 0
static std::size_t
size(const Contact &p) noexcept {
  // TODO ipv4
  assertx(p.ip.type == IpType::IPV4);
  return sizeof(p.ip.ipv4) + sizeof(p.port);
}

static std::size_t
size(const dht::Node &p) noexcept {
  return sizeof(p.id.id) + size(p.contact);
}

static std::size_t
size(const dht::Node **list, std::size_t length) noexcept {
  std::size_t result = 0;
  for_all(list, length, [&result](const auto &value) { //
    result += size(value);
    return true;
  });
  return result;
}

template <typename T>
static std::size_t
size(const sp::list<T> &list) noexcept {
  std::size_t result = 0;
  for_each(list, [&result](const T &ls) { //
    result += size(ls);
  });
  return result;
}
#endif

//=====================================
// template <typename Buffer>
// bool
// bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
//                                  const sp::UinArray<dht::Peer> &list)
//                                  noexcept {
//   if (!bencode::e<Buffer>::value(buf, key)) {
//     return false;
//   }
//
//   auto cb = [](Buffer &b, void *arg) {
//     auto &l = *((const sp::UinArray<dht::Peer> *)arg);
//
//     return for_all(l, [&b](const auto &ls) {
//       if (!value(b, ls.contact)) {
//         return false;
//       }
//
//       return true;
//     });
//   };
//
//   return bencode::e<Buffer>::value(buf, length(list), (void *)&list, cb);
// } // bencode::e::pair_compact()
//
// template <typename Buffer>
// bool
// bencode::e<Buffer>::value(Buffer &b, std::size_t length, void *closure,
//                           bool (*f)(Buffer &, void *)) noexcept {
//   if (!raw_numeric(b, "%zu", length)) {
//     return false;
//   }
//
//   if (!write(b, ':')) {
//     return false;
//   }
//
//   if (!f(b, closure)) {
//     return false;
//   }
//
//   return true;
// }
//
// //=====================================
// template <typename Buffer>
// static bool
// write(Buffer &b, const Contact &) noexcept {
//   //TODO
// }
//
// template <typename Buffer>
// bool
// bencode::e<Buffer>::value_compact(Buffer &b, const dht::Node &node) noexcept
// {
//   if (!write(b, node.id.id, sizeof(node.id.id))) {
//     return false;
//   }
//
//   if (!write(b, node.contact)) {
//     return false;
//   }
//
//   return true;
// }
//
// template <typename Buffer>
// bool
// bencode::e<Buffer>::value(Buffer &b, const dht::Node &value) noexcept {
//   return bencode::e<Buffer>::dict(b, [&value](Buffer &buffer) {
//     if (!bencode::e<Buffer>::value(buffer, "id")) {
//       return false;
//     }
//     if (!bencode::e<Buffer>::value(buffer, value.id.id, sizeof(value.id.id)))
//     {
//       return false;
//     }
//
//     if (!value_contact(buffer, value.contact)) {
//       return false;
//     }
//
//     return true;
//   });
// }

// template <typename Buffer>
// bool
// bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
//                                  const sp::list<dht::Node> &list) noexcept {
//   if (!bencode::e<Buffer>::value(buf, key)) {
//     return false;
//   }
//   /*
//    * id +contact
//    */
//   std::size_t len = size(list);
//
//   auto f = [](Buffer &b, void *a) {
//     const auto *l = (sp::list<dht::Node> *)a;
//
//     return for_all(*l,
//                    [&b](const auto &value) { //
//                      return value_compact(b, value);
//                    });
//   };
//
//   return bencode::e<Buffer>::value(buf, len, (void *)&list, f);
// } // bencode::e::pair()

//=====================================
// template <typename Buffer>
// bool
// bencode::e<Buffer>::pair_compact(Buffer &buf, const char *key,
//                                  const dht::Node **list,
//                                  std::size_t sz) noexcept {
//
//   if (!bencode::e<Buffer>::value(buf, key)) {
//     return false;
//   }
//
//   std::size_t len = size(list, sz);
//   std::tuple<const dht::Node **, std::size_t> arg(list, sz);
//
//   return bencode::e<Buffer>::value(buf, len, &arg, [](auto &b, void *a) {
//     auto targ = (std::tuple<const dht::Node **, std::size_t> *)a;
//
//     auto f = [&b](const auto &value) {
//       if (!bencode::e<Buffer>::value(b, value)) {
//         return false;
//       }
//       return true;
//     };
//
//     return for_all(std::get<0>(*targ), std::get<1>(*targ), f);
//   });
// } // bencode::e::pair_compact()

//=====================================
template struct bencode::e<sp::BytesView>;
template struct bencode::e<sp::Sink>;
template struct bencode::e<sp::CircularByteBuffer>;
} // namespace sp
