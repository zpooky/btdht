#include "bencode.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

namespace bencode {

template <typename T>
static bool
encode_numeric(sp::Buffer &buffer, const char *format, T in) noexcept {
  std::size_t &i = buffer.pos;
  const std::size_t remaining = buffer.capacity - i;

  char *b = reinterpret_cast<char *>(buffer.raw + i);
  int res = std::snprintf(b, remaining, format, in);
  if (res < 0) {
    return false;
  }
  if (buffer.pos + res > buffer.capacity) {
    return false;
  }
  i += res;
  return true;
}

template <typename T>
static bool
encode_integer(sp::Buffer &buffer, const char *format, T in) noexcept {
  static_assert(std::is_integral<T>::value, "");

  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer[i++] = 'i';

  if (!encode_numeric(buffer, format, in)) {
    buffer.pos = before;
    return false;
  }

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer[i++] = 'e';
  return true;
}
namespace e {
bool
value(sp::Buffer &buffer, std::uint32_t in) noexcept {
  return encode_integer(buffer, "%u", in);
}

bool
value(sp::Buffer &buffer, std::int32_t in) noexcept {
  return encode_integer(buffer, "%d", in);
}
//-----------------------------
template <typename T>
bool
encode_raw(sp::Buffer &buffer, const T *str, std::size_t length) noexcept {
  const std::size_t before = buffer.pos;
  if (!encode_numeric(buffer, "%u", length)) {
    buffer.pos = before;
    return false;
  }
  if ((buffer.pos + length + 1) > buffer.capacity) {
    buffer.pos = before;
    return false;
  }

  std::size_t &i = buffer.pos;
  buffer[i++] = ':';
  std::memcpy(buffer.raw + i, str, length);
  i += length;

  return true;
}

bool
value(sp::Buffer &buffer, const char *str) noexcept {
  return value(buffer, str, std::strlen(str));
}

bool
value(sp::Buffer &buffer, const char *str, std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
}

bool
value(sp::Buffer &buffer, const sp::byte *str, std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
}

//-----------------------------
bool
list(sp::Buffer &buffer, void *arg, bool (*f)(sp::Buffer &, void *)) noexcept {
  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;
  if (buffer.pos + 1 > buffer.capacity) {
    return false;
  }
  buffer.raw[i++] = 'l';

  if (!f(buffer, arg)) {
    buffer.pos = before;
    return false;
  }

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer.raw[i++] = 'e';

  return true;
}

//-----------------------------

template <typename V>
static bool
generic_encodePair(sp::Buffer &buffer, const char *key, V val) {
  const std::size_t before = buffer.pos;
  if (!value(buffer, key)) {
    buffer.pos = before;
    return false;
  }
  if (!value(buffer, val)) {
    buffer.pos = before;
    return false;
  }
  return true;
}

bool
pair(sp::Buffer &buffer, const char *key, const char *value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
pair(sp::Buffer &buffer, const char *key, std::uint32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
pair(sp::Buffer &buffer, const char *key, const sp::byte *val,
     std::size_t length) noexcept {
  const std::size_t before = buffer.pos;
  if (!value(buffer, key)) {
    buffer.pos = before;
    return false;
  }

  if (!value(buffer, val, length)) {
    buffer.pos = before;
    return false;
  }
  return true;
}
} // namespace e
//-----------------------------

namespace d {
template <typename T>
static bool
read_numeric(sp::Buffer &b, T &out, char end) noexcept {
  static_assert(std::is_integral<T>::value, "");
  const std::size_t p = b.pos;

  char str[32] = {0};
  std::size_t it = 0;
Lloop:
  if (sp::remaining_read(b) > 0) {
    if (b[b.pos] != end) {

      if (b[b.pos] >= '0' && b[b.pos] <= '9') {
        if (it < sizeof(str)) {
          str[it++] = b[b.pos++];
          goto Lloop;
        }
      }
      b.pos = p;
      return false;
    }
    ++b.pos;
  } else {
    b.pos = p;
    return false;
  }

  if (it == 0) {
    b.pos = p;
    return false;
  }

  out = std::atoll(str);
  return true;
} // bencode::d::read_numeric()

template <typename T>
static bool
parse_string(sp::Buffer &b, /*OUT*/ const T *&str, std::size_t &len) noexcept {
  static_assert(sizeof(T) == 1, "");
  const std::size_t p = b.pos;

  if (!read_numeric(b, len, ':')) {
    b.pos = p;
    return false;
  }
  if (len > sp::remaining_read(b)) {
    b.pos = p;
    return false;
  }
  str = (T *)b.raw + b.pos;
  b.pos += len;

  return true;
} // bencode::d::parse_string()

static bool
parse_key(sp::Buffer &b, const char *cmp_key) noexcept {
  const std::size_t key_len = std::strlen(cmp_key);
  const std::size_t p = b.pos;

  const char *parse_key = nullptr;
  std::size_t parse_key_len = 0;
  if (!parse_string(b, parse_key, parse_key_len)) {
    b.pos = p;
    return false;
  }

  if (key_len != parse_key_len ||
      std::memcmp(cmp_key, parse_key, key_len) != 0) {
    b.pos = p;
    return false;
  }

  return true;
} // bencode::d::parse_key()

template <typename T>
static bool
parse_key_value(sp::Buffer &b, const char *key, T *val,
                std::size_t len) noexcept {
  const std::size_t p = b.pos;
  if (!parse_key(b, key)) {
    b.pos = p;
    return false;
  }

  const sp::byte *val_ref = nullptr;
  std::size_t val_len = 0;
  if (!parse_string(b, val_ref, val_len)) {
    b.pos = p;
    return false;
  }
  if (val_len > len) {
    b.pos = p;
    return false;
  }

  std::memcpy(val, val_ref, val_len);
  // TODO howd to indicate length of parsed val?

  return true;
} // bencode::d::parse_key_value()

static bool
parse_key_valuex(sp::Buffer &b, const char *key, std::uint64_t &val) noexcept {
  const std::size_t p = b.pos;
  if (!parse_key(b, key)) {
    b.pos = p;
    return false;
  }

  if (sp::remaining_read(b) == 0 || b.raw[b.pos++] != 'i') {
    b.pos = p;
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    b.pos = p;
    return false;
  }

  return true;
} // bencode::d::parse_key_valuex()

/*Decoder*/
Decoder::Decoder(sp::Buffer &b)
    : buf(b) {
}

namespace internal {
bool
is(sp::Buffer &buf, const char *exact, std::size_t length) noexcept {
  if (sp::remaining_read(buf) < length) {
    return false;
  }
  if (std::memcmp(exact, buf.raw + buf.pos, length) != 0) {
    return false;
  }
  buf.pos += length;
  return true;
}
} // namespace internal

bool
pair(Decoder &d, const char *key, char *val, std::size_t len) noexcept {
  return parse_key_value(d.buf, key, val, len);
} // bencode::d::pair()

bool
pair(Decoder &d, const char *key, sp::byte *val, std::size_t len) noexcept {
  return parse_key_value(d.buf, key, val, len);
}

bool
pair(Decoder &d, const char *key, bool &v) noexcept {
  std::uint64_t t = 0;
  if (!parse_key_valuex(d.buf, key, t)) {
    return false;
  }

  v = t == 1;

  return true;
} // bencode::d::pair()

bool
pair(Decoder &d, const char *key, std::uint32_t &v) noexcept {
  const std::size_t p = d.buf.pos;
  std::uint64_t t = 0;
  if (!parse_key_valuex(d.buf, key, t)) {
    d.buf.pos = p;
    return false;
  }

  if (t > std::uint64_t(~std::uint32_t(0))) {
    d.buf.pos = p;
    return false;
  }

  v = std::uint32_t(t);

  return true;
} // bencode::d::pair()

bool
pair(Decoder &d, const char *key, std::uint16_t &v) noexcept {
  const std::size_t p = d.buf.pos;
  std::uint64_t t = 0;
  if (!parse_key_valuex(d.buf, key, t)) {
    d.buf.pos = p;
    return false;
  }

  if (t > std::uint64_t(~std::uint16_t(0))) {
    d.buf.pos = p;
    return false;
  }

  v = std::uint16_t(t);

  return true;
} // bencode::d::pair()

template <typename T, typename F>
static bool
list(Decoder &d, sp::list<T> &list, void *arg, F f) noexcept {
  sp::Buffer &b = d.buf;
  const std::size_t p = b.pos;

  sp::node<T> *node = list.root;
  list.size = 0;

  while (sp::remaining_read(b) && b[b.pos] != 'e') {
    if (!node) {
      return false;
    }

    if (!f(d, node->value, arg)) {
      return false;
    }
    node = node->next;
    ++list.size;
  }

  if (sp::remaining_read(b) == 0) {
    b.pos = p;
    return false;
  }

  if (b[b.pos++] != 'e') {
    b.pos = p;
    return false;
  }

  return true;
} // bencode::d::list()

static void
value_to_peer(const char *str, dht::Peer &peer) noexcept {
  std::memcpy(&peer.ip, str, sizeof(peer.ip));
  str += sizeof(peer.ip);
  peer.ip = ntohl(peer.ip);

  std::memcpy(&peer.port, str, sizeof(peer.port));
  peer.port = ntohl(peer.port);
} // bencode::d::value_to_peer()

static bool
value(Decoder &d, dht::Node &value) noexcept {
  dht::Peer &peer = value.peer;
  sp::Buffer &buf = d.buf;

  const std::size_t pos = buf.pos;

  const char *str = nullptr;
  std::size_t len = 0;
  if (!parse_string(d.buf, str, len)) {
    buf.pos = pos;
    return false;
  }

  if (len != (sizeof(value.id.id), sizeof(peer.ip) + sizeof(peer.port))) {
    buf.pos = pos;
    return false;
  }

  std::memcpy(value.id.id, str, sizeof(value.id.id));
  str += sizeof(value.id.id);

  value_to_peer(str, peer);

  return true;
} // bencode::d::value()

static bool
value(Decoder &d, dht::Peer &peer) noexcept {
  sp::Buffer &buf = d.buf;
  const std::size_t pos = buf.pos;

  const char *str = nullptr;
  std::size_t len = 0;
  if (!parse_string(d.buf, str, len)) {
    buf.pos = pos;
    return false;
  }

  if (len != (sizeof(peer.ip) + sizeof(peer.port))) {
    buf.pos = pos;
    return false;
  }

  value_to_peer(str, peer);

  return true;
} // bencode::d::value()

template <typename T>
static bool
decode_list_pair(Decoder &d, const char *key, sp::list<T> &list) noexcept {
  sp::Buffer &b = d.buf;
  const std::size_t p = b.pos;

  std::uint64_t t = 0;
  if (!parse_key_valuex(d.buf, key, t)) {
    b.pos = p;
    return false;
  }

  auto f = [](Decoder &d, /*OUT*/ T &out, void *) { //
    return value(d, out);
  };

  if (!bencode::d::list(d, list, nullptr, f)) {
    b.pos = p;
    return false;
  }

  return true;
} // bencode::d::decode_list_pair()

bool
pair(Decoder &d, const char *key, sp::list<dht::Node> &l) noexcept {
  return decode_list_pair(d, key, l);
} // bencode::d::pair()

bool
pair(Decoder &d, const char *key, sp::list<dht::Peer> &l) noexcept {
  return decode_list_pair(d, key, l);
} // bencode::d:pair()

bool
value(Decoder &d, const char *key) noexcept {
  return parse_key(d.buf, key);
}//bencode::d::value()

} // namespace d

} // namespace bencode
