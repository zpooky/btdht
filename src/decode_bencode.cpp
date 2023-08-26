#include "decode_bencode.h"
#include <buffer/BytesView.h>
#include <buffer/Thing.h>
#include <cstring>
#include <util/conversions.h>

//=====================================
template <typename Buffer, typename T>
static bool
read_numeric(Buffer &b, T &out, char end) noexcept {
  static_assert(std::is_integral<T>::value, "");

  char str[32] = {0};
  std::size_t it = 0;
Lloop : {
  unsigned char cur = 0;
  if (pop_front(b, cur) == 1) {
    if (cur != end) {

      if (cur >= '0' && cur <= '9') {
        if (it < sizeof(str)) {
          str[it++] = cur;
          goto Lloop;
        }
      }

      return false;
    }
  } else {
    assertx(false);
    return false;
  }
}

  if (it == 0) {
    return false;
  }

  return sp::parse_int(str + 0, str + it, out);
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &b, std::uint64_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
    m.rollback = true;
    return false;
  }
  if (out != 'i') {
    m.rollback = true;
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    m.rollback = true;
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode_d<Buffer>::pair(Buffer &buf, const char *key,
                        std::uint64_t &value) noexcept {
  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &b, std::uint32_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
    m.rollback = true;
    return false;
  }
  if (out != 'i') {
    m.rollback = true;
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    m.rollback = true;
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode_d<Buffer>::pair(Buffer &buf, const char *key,
                        std::uint32_t &value) noexcept {
  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &b, std::uint16_t &val) noexcept {
  auto m = mark(b);

  unsigned char out = '\0';
  if (pop_front(b, out) != 1) {
    m.rollback = true;
    return false;
  }
  if (out != 'i') {
    m.rollback = true;
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    m.rollback = true;
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode_d<Buffer>::pair(Buffer &buf, const char *key,
                        std::uint16_t &value) noexcept {
  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_d<Buffer>::value(buf, value);
}

//=====================================

//=====================================
template <typename Buffer, typename T>
static bool
parse_string(Buffer &b, /*OUT*/ T *str, std::size_t N,
             std::size_t &len) noexcept {
  static_assert(sizeof(T) == 1, "");

  if (!read_numeric(b, len, ':')) {
    return false;
  }

  if (len > N) {
    // TODO we sometimes get 32byte "id" instead of the expected 20byte "id"
    return false;
  }

  len = pop_front(b, (unsigned char *)str, len);
  return true;
}

template <typename Buffer, typename T, std::size_t N>
static bool
parse_string(Buffer &b, /*OUT*/ T (&str)[N], std::size_t &len) noexcept {
  return parse_string(b, str, N, len);
} // bencode_d::parse_string()

template <typename Buffer>
bool
bencode_d<Buffer>::is_key(Buffer &buf, const char *key) noexcept {
  const auto key_len = std::strlen(key);

  unsigned char out[128] = {0};
  if (sizeof(out) < key_len) {
    assertx(false);
    return false;
  }
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, out, out_len)) {
    m.rollback = true;
    return false;
  }

  if (out_len == key_len && std::memcmp(key, out, key_len) == 0) {
    return true;
  }

  m.rollback = true;
  return false;
}

template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &buf, sp::byte *value,
                         std::size_t &len) noexcept {
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, value, len, out_len)) {
    m.rollback = true;
    return false;
  }
  len = out_len;

  return true;
}

template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &buf, char *value, std::size_t &len) noexcept {
  auto m = mark(buf);

  std::size_t out_len = 0;
  if (!parse_string(buf, value, len, out_len)) {
    m.rollback = true;
    return false;
  }
  len = out_len;

  return true;
}

template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &buf,
                         sp::UinArray<std::string> &value) noexcept {
  return bencode_d<Buffer>::list(buf, [&value](auto &b) {
    char front;
    while (peek_front(b, front) && front != 'e') {
      std::string str;
      if (!bencode_d<Buffer>::value(b, str)) {
        return false;
      }
      //
      insert(value, str);
    }
    return true;
  });
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &buf, Ip &value) noexcept {
  return bencode_d<Buffer>::value(buf, value.ipv4);
}

template <typename Buffer>
bool
bencode_d<Buffer>::pair(Buffer &buf, const char *key, Ip &value) noexcept {
  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &buf, std::string &value) noexcept {
  std::size_t l_raw = 0;

  if (!read_numeric(buf, l_raw, ':')) {
    return false;
  }
  value.clear();
  for (std::size_t i = 0; i < l_raw; ++i) {
    char c;
    size_t len = pop_front(buf, &c, sizeof(c));
    assertx(len == sizeof(c));
    value.append(&c, 1);
    // XXX
  }

  return true;
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::pair(Buffer &buf, const char *key, sp::byte *value,
                        std::size_t &l) noexcept {

  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_d<Buffer>::value(buf, value, l);
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::value(Buffer &b, dht::NodeId &id) noexcept {
  auto len = sizeof(id.id);
  if (!value(b, id.id, len)) {
    return false;
  }

  if (len != 20) {
    return false;
  }

  return true;
}

template <typename Buffer>
bool
bencode_d<Buffer>::pair(Buffer &buf, const char *key,
                        dht::NodeId &value) noexcept {
  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::pair(Buffer &buf, const char *key,
                        sp::UinArray<std::string> &value) noexcept {
  if (!bencode_d<Buffer>::is_key(buf, key)) {
    return false;
  }

  return bencode_d<Buffer>::value(buf, value);
}

//=====================================
template <typename Buffer>
bool
bencode_d<Buffer>::value_compact(Buffer &buf,
                                 sp::UinArray<dht::Infohash> &out) noexcept {
  std::size_t l_raw = 0;

  if (!read_numeric(buf, l_raw, ':')) {
    return false;
  }
  for (std::size_t i = 0; i < l_raw; i += sizeof(dht::Infohash::id)) {
    dht::Infohash ih{};
    size_t len = pop_front(buf, ih.id, sizeof(ih.id));
    if (len != sizeof(ih.id)) {
      return false;
    }
    insert(out, std::move(ih));
  }
  return true;
}

template <typename Buffer>
bool
bencode_d<Buffer>::value_compact(
    Buffer &buf, sp::UinArray<std::tuple<dht::NodeId, Contact>> &out) noexcept {
  std::size_t l_raw = 0;

  if (!read_numeric(buf, l_raw, ':')) {
    return false;
  }

  for (std::size_t i = 0; i < l_raw;
       i +=
       sizeof(dht::Key) + sizeof(Contact::ip.ipv4) + sizeof(Contact::port)) {
    dht::NodeId id{};
    size_t len = pop_front(buf, id.id, sizeof(id.id));
    if (len != sizeof(id.id)) {
      return false;
    }
    Contact c{};
    len = pop_front(buf, &c.ip.ipv4, sizeof(c.ip.ipv4));
    c.ip.ipv4 = ntohl(c.ip.ipv4);
    if (len != sizeof(c.ip.ipv4)) {
      return false;
    }
    len = pop_front(buf, &c.port, sizeof(c.port));
    c.port = ntohs(c.port);
    if (len != sizeof(c.port)) {
      return false;
    }

    insert(out, std::tuple<dht::NodeId, Contact>(std::move(id), std::move(c)));
  }
  return true;
}

//=====================================
// template struct bencode_d<sp::CircularByteBuffer>;
template struct bencode_d<sp::Thing>;
template struct bencode_d<sp::BytesView>;
