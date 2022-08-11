#ifndef SP_MAINLINE_DHT_SHAED_KRPC_H
#define SP_MAINLINE_DHT_SHAED_KRPC_H

#include "shared.h"
#include "util.h"

namespace krpc {
template <typename F>
static bool
message(sp::Buffer &buf, const Transaction &t, const char *mt,
        const char *query, F f) noexcept {
  auto cb = [&t, &mt, query, &f](sp::Buffer &b) {
    if (!f(b)) {
      return false;
    }

    if (query) {
      if (!bencode::e::pair(b, "q", query)) {
        return false;
      }
    }

    // transaction: t
    assertx(t.length > 0);
    if (!bencode::e::pair(b, "t", t.id, t.length)) {
      return false;
    }

    sp::byte version[4] = {0};
    {
      version[0] = 's';
      version[1] = 'p';
      version[2] = '0';
      version[3] = '1';
    }
    if (!bencode::e::pair(b, "v", version, sizeof(version))) {
      return false;
    }

    // message_type[reply: r, query: q]
    if (!bencode::e::pair(b, "y", mt)) {
      return false;
    }

    return true;
  };

  return bencode::e::dict(buf, cb);
} // krpc::message()

template <typename F>
static bool
resp(sp::Buffer &buf, const Transaction &t, F f) noexcept {
  return message(buf, t, "r", nullptr, [&f](auto &b) { //
    if (!bencode::e::value(b, "r")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b2) { //
      return f(b2);
    });
  });
} // krpc::resp()

template <typename F>
static bool
req(sp::Buffer &buf, const Transaction &t, const char *query, F f) noexcept {
  return message(buf, t, "q", query, [&f](auto &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b2) { //
      return f(b2);
    });
  });
} // krpc::req()

template <typename F>
static bool
err(sp::Buffer &buf, const Transaction &t, F f) noexcept {
  const char *query = nullptr;
  return message(buf, t, "e", query, [&f](auto &b) { //
    if (!bencode::e::value(b, "e")) {
      return false;
    }

    return f(b);
  });
} // krpc::err()

} // namespace krpc

#endif
