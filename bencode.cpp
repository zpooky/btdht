#include "bencode.h"
#include <arpa/inet.h>
#include <cassert>
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
} // bencode::encode_numeric()

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
} // bencode::encode_integer()

namespace e {
bool
value(sp::Buffer &buffer, std::uint16_t in) noexcept {
  return encode_integer(buffer, "%hu", in);
} // bencode::e::value()

bool
value(sp::Buffer &buffer, std::int16_t in) noexcept {
  return encode_integer(buffer, "%h", in);
} // bencode::e::value()

bool
value(sp::Buffer &buffer, std::uint32_t in) noexcept {
  return encode_integer(buffer, "%u", in);
} // bencode::e::value()

bool
value(sp::Buffer &buffer, std::int32_t in) noexcept {
  return encode_integer(buffer, "%d", in);
} // bencode::e::value()

bool
value(sp::Buffer &buffer, std::uint64_t in) noexcept {
  return encode_integer(buffer, "%llu", in);
}

bool
value(sp::Buffer &buffer, std::int64_t in) noexcept {
  return encode_integer(buffer, "%lld", in);
}
//-----------------------------
template <typename T>
static bool
encode_raw(sp::Buffer &buffer, const T *str, std::size_t length) noexcept {
  static_assert(
      std::is_same<T, char>::value || std::is_same<T, sp::byte>::value, "");

  const std::size_t before = buffer.pos;
  if (!encode_numeric(buffer, "%zu", length)) {
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
} // bencode::e::encode_raw()

bool
value(sp::Buffer &buffer, const char *str) noexcept {
  return value(buffer, str, std::strlen(str));
} // bencode::e::value()

bool
value(sp::Buffer &buffer, const char *str, std::size_t length) noexcept {
  return encode_raw(buffer, str, length);
} // bencode::e::value()

bool
value(sp::Buffer &b, const sp::byte *str, std::size_t length) noexcept {
  return encode_raw(b, str, length);
} // bencode::e::value()

bool
value(sp::Buffer &b, std::size_t length, void *closure,
      bool (*f)(sp::Buffer &, void *)) noexcept {
  const std::size_t pos = b.pos;
  if (!encode_numeric(b, "%zu", length)) {
    b.pos = pos;
    return false;
  }

  if (sp::remaining_read(b) < length + 1) {
    b.pos = pos;
    return false;
  }
  b.raw[b.pos++] = ':';

  const std::size_t before = b.pos;
  if (!f(b, closure)) {
    b.pos = pos;
    return false;
  }
  assert(before + length == b.pos);

  return true;
}

//-----------------------------
bool
list(sp::Buffer &buffer, void *capture,
     bool (*f)(sp::Buffer &, void *)) noexcept {
  const std::size_t before = buffer.pos;
  std::size_t &i = buffer.pos;
  if (buffer.pos + 1 > buffer.capacity) {
    return false;
  }
  buffer.raw[i++] = 'l';

  if (!f(buffer, capture)) {
    buffer.pos = before;
    return false;
  }

  if (buffer.pos + 1 > buffer.capacity) {
    buffer.pos = before;
    return false;
  }
  buffer.raw[i++] = 'e';

  return true;
} // bencode::e::list()

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
} // bencode::e::generic_encodePair()

bool
pair(sp::Buffer &buffer, const char *key, const char *value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

bool
pair(sp::Buffer &buffer, const char *key, std::int16_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

bool
pair(sp::Buffer &buffer, const char *key, std::uint16_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

bool
pair(sp::Buffer &buffer, const char *key, std::int32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

bool
pair(sp::Buffer &buffer, const char *key, std::uint32_t value) noexcept {
  return generic_encodePair(buffer, key, value);
} // bencode::e::pair()

bool
pair(sp::Buffer &buffer, const char *key, std::uint64_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
pair(sp::Buffer &buffer, const char *key, std::int64_t value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
pair(sp::Buffer &buffer, const char *key, bool value) noexcept {
  return generic_encodePair(buffer, key, value);
}

bool
pair(sp::Buffer &b, const char *k, const sp::byte *v, std::size_t l) noexcept {
  const std::size_t before = b.pos;

  if (!value(b, k)) {
    b.pos = before;
    return false;
  }

  if (!value(b, v, l)) {
    b.pos = before;
    return false;
  }

  return true;
} // bencode::e::pair()

} // namespace e

//=DECODE=========================================
namespace d {
template <typename T>
static bool
peek_numeric(const sp::Buffer &b, T &out, char end,
             std::size_t &read) noexcept {
  static_assert(std::is_integral<T>::value, "");
  std::size_t p = b.pos;

  char str[32] = {0};
  std::size_t it = 0;
Lloop:
  if (sp::remaining_read(b) > 0) {
    if (b[p] != end) {

      if (b[p] >= '0' && b[p] <= '9') {
        if (it < sizeof(str)) {
          str[it++] = b[p++];
          goto Lloop;
        }
      }
      return false;
    }
    ++p;
  } else {
    return false;
  }

  if (it == 0) {
    return false;
  }

  read = p - b.pos;
  out = std::atoll(str);
  return true;
} // bencode::d::read_numeric()
template <typename T>
static bool
read_numeric(sp::Buffer &b, T &out, char end) noexcept {
  std::size_t read = 0;

  if (!peek_numeric(b, out, end, read)) {
    return false;
  }

  b.pos += read;
  return true;
} // bencode::d::read_numeric()

template <typename T>
static bool
peek_string(const sp::Buffer &b, /*OUT*/ const T *&str,
            std::size_t &len) noexcept {
  static_assert(sizeof(T) == 1, "");

  std::size_t num_read = 0;
  if (!peek_numeric(b, len, ':', num_read)) {
    return false;
  }

  if (len > sp::remaining_read(b)) {
    return false;
  }
  str = (T *)b.raw + b.pos + num_read;

  return true;
} // bencode::d::peek_string()

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
                /*IN&OUT*/ std::size_t &len) noexcept {
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
  len = val_len;

  return true;
} // bencode::d::parse_key_value()

static bool
parse_valuex(sp::Buffer &b, std::uint64_t &val) noexcept {
  const std::size_t p = b.pos;
  if (sp::remaining_read(b) == 0 || b.raw[b.pos++] != 'i') {
    b.pos = p;
    return false;
  }

  if (!read_numeric(b, val, 'e')) {
    b.pos = p;
    return false;
  }

  return true;
}

static bool
parse_key_valuex(sp::Buffer &b, const char *key, std::uint64_t &val) noexcept {
  const std::size_t p = b.pos;
  if (!parse_key(b, key)) {
    b.pos = p;
    return false;
  }

  if (!parse_valuex(b, val)) {
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
pair_x(Decoder &d, const char *key, char *val,
       /*IN&OUT*/ std::size_t &len) noexcept {
  return parse_key_value(d.buf, key, val, len);
} // bencode::d::pair()

bool
pair_x(Decoder &d, const char *key, sp::byte *val,
       /*IN&OUT*/ std::size_t &len) noexcept {
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

  if (sp::remaining_read(b) == 0) {
    return false;
  }

  if (b[b.pos++] != 'l') {
    b.pos = p;
    return false;
  }

  sp::node<T> *node = list.root;
  list.size = 0;

  while (sp::remaining_read(b) > 0 && b[b.pos] != 'e') {
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
value_to_peer(const char *str, Contact &peer) noexcept {
  // TODO ipv4
  std::memcpy(&peer.ipv4, str, sizeof(peer.ipv4));
  str += sizeof(peer.ipv4);
  peer.ipv4 = ntohl(peer.ipv4);

  std::memcpy(&peer.port, str, sizeof(peer.port));
  peer.port = ntohs(peer.port);
} // bencode::d::value_to_peer()

static bool
value(Decoder &d, dht::Node &value) noexcept {
  Contact &contact = value.contact;
  sp::Buffer &buf = d.buf;

  const std::size_t pos = buf.pos;

  const char *str = nullptr;
  std::size_t len = 0;
  if (!parse_string(d.buf, str, len)) {
    buf.pos = pos;
    return false;
  }

  // TODO ipv4
  constexpr std::size_t cmp =
      (sizeof(value.id.id) + sizeof(contact.ipv4) + sizeof(contact.port));
  if (len != cmp) {
    buf.pos = pos;
    return false;
  }

  std::memcpy(value.id.id, str, sizeof(value.id.id));
  str += sizeof(value.id.id);

  value_to_peer(str, contact);

  return true;
} // bencode::d::value()

static bool
value(Decoder &d, Contact &peer) noexcept {
  sp::Buffer &buf = d.buf;
  const std::size_t pos = buf.pos;

  const char *str = nullptr;
  std::size_t len = 0;
  if (!parse_string(d.buf, str, len)) {
    buf.pos = pos;
    return false;
  }

  // TODO ipv4
  if (len != (sizeof(peer.ipv4) + sizeof(peer.port))) {
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

  if (!parse_key(d.buf, key)) {
    b.pos = p;
    return false;
  }

  auto f = [](Decoder &decoder, /*OUT*/ T &out, void *) { //
    return value(decoder, out);
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
pair(Decoder &d, const char *key, sp::list<Contact> &l) noexcept {
  return decode_list_pair(d, key, l);
} // bencode::d:pair()

bool
value(Decoder &d, const char *key) noexcept {
  return parse_key(d.buf, key);
} // bencode::d::value()

bool
value_ref(Decoder &d, const char *&key, std::size_t &key_len) noexcept {
  const std::size_t p = d.buf.pos;
  if (!parse_string(d.buf, key, key_len)) {
    d.buf.pos = p;
    return false;
  }

  return true;
}

bool
value_ref(Decoder &d, const sp::byte *&key, std::size_t &key_len) noexcept {
  const std::size_t p = d.buf.pos;
  if (!parse_string(d.buf, key, key_len)) {
    d.buf.pos = p;
    return false;
  }

  return true;
}

bool
value(Decoder &d, std::uint64_t &val) noexcept {
  return parse_valuex(d.buf, val);
}

bool
peek(const Decoder &d, const char *key) noexcept {

  const char *parse_key = nullptr;
  std::size_t parse_key_len = 0;
  if (!peek_string(d.buf, parse_key, parse_key_len)) {
    return false;
  }

  std::size_t key_len = std::strlen(key);
  if (key_len != parse_key_len || std::memcmp(key, parse_key, key_len) != 0) {
    return false;
  }
  return true;
}

bool
pair_any(Decoder &d, char *key, std::size_t klen, sp::byte *val,
         std::size_t vlen) noexcept {
  sp::Buffer &b = d.buf;
  const std::size_t pos = b.pos;

  {
    const char *pkey = nullptr;
    std::size_t pkeylen = 0;
    if (!parse_string(b, pkey, pkeylen)) {
      b.pos = pos;
      return false;
    }

    if ((pkeylen + 1) > klen) {
      b.pos = pos;
      return false;
    }
    std::memset(key, 0, klen);
    std::memcpy(key, pkey, pkeylen);
  }
  {
    const sp::byte *pval = nullptr;
    std::size_t plen = 0;
    if (!parse_string(b, pval, plen)) {
      b.pos = pos;
      return false;
    }

    if (plen + 1 > vlen) {
      b.pos = pos;
      return false;
    }

    std::memset(val, 0, vlen);
    std::memcpy(val, pval, plen);
  }

  return true;
}

} // namespace d

} // namespace bencode
