#include "encode_bencode.h"
#include <arpa/inet.h>
#include <buffer/CircularByteBuffer.h>
#include <buffer/Sink.h>

namespace sp {
//=====================================
template <typename Buffer, typename T>
static bool
write_raw_numeric(Buffer &buffer, const char *format, T in) noexcept {
  char b[64] = {0};
  int res = std::snprintf(b, sizeof(b), format, in);
  if (res < 0) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write(buffer, b, (std::size_t)res)) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  return true;
} // bencode::write_raw_numeric()

template <typename Buffer, typename T>
static bool
encode_integer(Buffer &buffer, const char *format, T in) noexcept {
  static_assert(std::is_integral<T>::value, "");

  if (!write(buffer, 'i')) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write_raw_numeric(buffer, format, in)) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  if (!write(buffer, 'e')) {
    fprintf(stderr, "%s:2\n", __func__);
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

  if (!write_raw_numeric(buffer, "%zu", length)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write(buffer, ':')) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  if (!write(buffer, str, length)) {
    fprintf(stderr, "%s:2\n", __func__);
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

  if (!write_raw_numeric(buffer, "%zu", raw_length)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write(buffer, ':')) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  for (std::size_t a = 0; a < length; ++a) {
    if (!write(buffer, v[a].id, sizeof(v[a].id))) {
      fprintf(stderr, "%s:2\n", __func__);
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

  if (!write_raw_numeric(b, "%zu", raw_size)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write(b, ':')) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  std::size_t dummy_written = 0;
  bool result = for_all(list, length, [&b, &dummy_written](const auto &value) {
    if (!write(b, value.id.id, sizeof(value.id.id))) {
      fprintf(stderr, "%s:2\n", __func__);
      return false;
    }
    dummy_written += sizeof(value.id.id);

    Ipv4 ip = htonl(value.contact.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      fprintf(stderr, "%s:3\n", __func__);
      return false;
    }
    dummy_written += sizeof(ip);
    Port port = htons(value.contact.port);
    if (!write(b, &port, sizeof(port))) {
      fprintf(stderr, "%s:4\n", __func__);
      return false;
    }
    dummy_written += sizeof(port);

    return true;
  });
  assertx(raw_size == dummy_written);

  return result;
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_id_contact_compact(
    Buffer &b, const sp::list<dht::Node> &list) noexcept {
  std::size_t raw_size = 0;

  for_each(list, [&raw_size](const auto &value) { //
    raw_size += serialize_size(value.contact) + sizeof(value.id.id);
  });

  if (!write_raw_numeric(b, "%zu", raw_size)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write(b, ':')) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  return for_all(list, [&b](const auto &value) {
    if (!write(b, value.id.id, sizeof(value.id.id))) {
      fprintf(stderr, "%s:2\n", __func__);
      return false;
    }

    Ipv4 ip = htonl(value.contact.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      fprintf(stderr, "%s:3\n", __func__);
      return false;
    }
    Port port = htons(value.contact.port);
    if (!write(b, &port, sizeof(port))) {
      fprintf(stderr, "%s:3\n", __func__);
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

  if (!write_raw_numeric(b, "%zu", raw_size)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write(b, ':')) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  fprintf(stderr, "%s:2\n", __func__);
  return for_all(list, [&b](const auto &value) {
    Ipv4 ip = htonl(value.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      fprintf(stderr, "%s:2\n", __func__);
      return false;
    }
    Port port = htons(value.port);
    if (!write(b, &port, sizeof(port))) {
      fprintf(stderr, "%s:3\n", __func__);
      return false;
    }

    return true;
  });
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_string_list_contact(
    Buffer &b, const sp::list<Contact> &values) noexcept {
  if (!write(b, 'l')) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  bool res = for_all(values, [&b](const auto &v) {
    char entry[sizeof(Ipv4) + sizeof(Port) + 1] = {0};
    Ipv4 ip = htonl(v.ip.ipv4);
    Port port = htons(v.port);
    memcpy(entry, &ip, sizeof(ip));
    memcpy(entry + sizeof(ip), &port, sizeof(port));

    if (!bencode::e<Buffer>::value(b, entry)) {
      fprintf(stderr, "%s:1\n", __func__);
      return false;
    }

    return true;
  });

  if (!res) {
    fprintf(stderr, "%s:2\n", __func__);
    return false;
  }

  if (!write(b, 'e')) {
    fprintf(stderr, "%s:3\n", __func__);
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_string_list_contact(
    Buffer &b, const sp::UinArray<Contact> &values) noexcept {
  if (!write(b, 'l')) {
    fprintf(stderr, "%s:0\n", __func__);
    assertx(false);
    return false;
  }

  for (const Contact &v : values) {
    sp::byte entry[sizeof(Ipv4) + sizeof(Port)] = {0};
    Ipv4 ip = htonl(v.ip.ipv4);
    Port port = htons(v.port);
    memcpy(entry, &ip, sizeof(ip));
    memcpy(entry + sizeof(ip), &port, sizeof(port));

    if (!bencode::e<Buffer>::value(b, entry, sizeof(entry))) {
      fprintf(stderr, "%s:1\n", __func__);
      assertx(false);
      return false;
    }
  }

  if (!write(b, 'e')) {
    fprintf(stderr, "%s:2\n", __func__);
    assertx(false);
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::value_compact(Buffer &b,
                                  const sp::list<Contact> &list) noexcept {
  std::size_t raw_size = 0;

  for_each(list, [&raw_size](const auto &value) {
    raw_size += serialize_size(value);
  });

  if (!write_raw_numeric(b, "%zu", raw_size)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!write(b, ':')) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  return for_all(list, [&b](const auto &value) {
    Ipv4 ip = htonl(value.ip.ipv4);
    if (!write(b, &ip, sizeof(ip))) {
      fprintf(stderr, "%s:2\n", __func__);
      return false;
    }
    Port port = htons(value.port);
    if (!write(b, &port, sizeof(port))) {
      fprintf(stderr, "%s:3\n", __func__);
      return false;
    }

    return true;
  });
}

//=====================================
template <typename Buffer>
bool
bencode::e<Buffer>::list(Buffer &b,
                         const sp::UinArray<std::string> &values) noexcept {
  if (!write(b, 'l')) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  for (const std::string &v : values) {
    if (!bencode::e<Buffer>::value(b, v.c_str())) {
      fprintf(stderr, "%s:1\n", __func__);
      return false;
    }
  }

  if (!write(b, 'e')) {
    fprintf(stderr, "%s:2\n", __func__);
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair(Buffer &b, const char *key,
                         const sp::UinArray<std::string> &value) noexcept {
  if (!bencode::e<Buffer>::value(b, key)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!bencode::e<Buffer>::list(b, value)) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  return true;
}

//=====================================
template <typename Buffer, typename V>
static bool
generic_encodePair(Buffer &buffer, const char *key, V val) {

  if (!bencode::e<Buffer>::value(buffer, key)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!bencode::e<Buffer>::value(buffer, val)) {
    fprintf(stderr, "%s:1\n", __func__);
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
    fprintf(stderr, "%s:0\n", __func__);
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
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!bencode::e<Buffer>::value_compact(b, v, l)) {
    fprintf(stderr, "%s:1\n", __func__);
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
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!bencode::e<Buffer>::value_id_contact_compact(b, list, length)) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair_id_contact_compact(
    Buffer &b, const char *key, const sp::list<dht::Node> &list) noexcept {
  if (!bencode::e<Buffer>::value(b, key)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!bencode::e<Buffer>::value_id_contact_compact(b, list)) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  return true;
}

#if 0
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
#endif

template <typename Buffer>
bool
bencode::e<Buffer>::pair_string_list_contact(
    Buffer &b, const char *key, const sp::UinArray<Contact> &list) noexcept {

  if (!bencode::e<Buffer>::value(b, key)) {
    fprintf(stderr, "%s:0\n", __func__);
    assertx(false);
    return false;
  }

  if (!bencode::e<Buffer>::value_string_list_contact(b, list)) {
    fprintf(stderr, "%s:1\n", __func__);
    assertx(false);
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode::e<Buffer>::pair_string_list_contact(
    Buffer &b, const char *key, const sp::list<Contact> &list) noexcept {
  if (!bencode::e<Buffer>::value(b, key)) {
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!bencode::e<Buffer>::value_string_list_contact(b, list)) {
    fprintf(stderr, "%s:1\n", __func__);
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
    fprintf(stderr, "%s:0\n", __func__);
    return false;
  }

  if (!f(buf, closure)) {
    fprintf(stderr, "%s:1\n", __func__);
    return false;
  }

  if (!write(buf, 'e')) {
    fprintf(stderr, "%s:2\n", __func__);
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
//   if (!write_raw_numeric(b, "%zu", length)) {
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
