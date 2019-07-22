#include "bencode_print.h"
#include "decode_bencode.h"
#include <buffer/Thing.h>
#include <cstddef>
#include <cstdio>
#include <string/ascii.h>

static FILE *_f = stdout;

namespace internal {
template <typename Buffer>
static bool
dict_wildcard(Buffer &d, std::size_t tabs) noexcept;

static void
print_tabs(std::size_t tabs) noexcept {
  for (std::size_t i = 0; i < tabs; ++i) {
    fprintf(_f, " ");
  }
}

static void
print_raw(const char *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    fprintf(_f, "%.*s", int(len), val);
  } else {
    fprintf(_f, "hex[");
    for (std::size_t i = 0; i < len; ++i) {
      fprintf(_f, "%hhX", (unsigned char)val[i]);
    }
    fprintf(_f, "]: %zu(", len);
    for (std::size_t i = 0; i < len; ++i) {
      if (ascii::is_printable(val[i])) {
        fprintf(_f, "%c", val[i]);
      } else {
        fprintf(_f, "_");
      }
    }
    fprintf(_f, ")");
  }
}

template <typename Buffer>
static bool
int_wildcard(Buffer &d, std::size_t tabs) noexcept {
  auto m = mark(d);
  std::uint64_t val = 0;
  if (sp::bencode::d<Buffer>::value(d, val)) {
    print_tabs(tabs);
    fprintf(_f, "i%lue\n", val);
    return true;
  }
  m.rollback = true;
  return false;
}

template <typename Buffer>
static bool
string_wildcard(Buffer &d, std::size_t tabs) noexcept {
  auto m = mark(d);
  std::size_t len = 1024;
  char *val = new char[len];
  if (sp::bencode::d<Buffer>::value(d, val, len)) {
    print_tabs(tabs);
    fprintf(_f, "%zu:", len);
    print_raw(val, len);
    fprintf(_f, "\n");
    delete[] val;
    return true;
  }

  m.rollback = true;
  delete[] val;
  return false;
}

template <typename Buffer>
static bool
list_wildcard(Buffer &d, std::size_t tabs) noexcept {
  auto m = mark(d);
  unsigned char cur = 0;

  if (pop_front(d, cur) != 1) {
    m.rollback = true;
    return false;
  }
  if (cur != 'l') {
    m.rollback = true;
    return false;
  }
  print_tabs(tabs);
  fprintf(_f, "l\n");
Lretry:
  if (list_wildcard(d, tabs + 1)) {
    goto Lretry;
  }
  if (int_wildcard(d, tabs + 1)) {
    goto Lretry;
  }
  if (string_wildcard(d, tabs + 1)) {
    goto Lretry;
  }
  if (dict_wildcard(d, tabs + 1)) {
    goto Lretry;
  }

  if (pop_front(d, cur) != 1) {
    m.rollback = true;
    return false;
  }
  if (cur != 'e') {
    m.rollback = true;
    return false;
  }
  print_tabs(tabs);
  fprintf(_f, "e\n");

  return true;
}

template <typename Buffer>
static bool
dict_wildcard(Buffer &d, std::size_t tabs) noexcept {
  auto m = mark(d);
  unsigned char cur = 0;
  if (pop_front(d, cur) != 1) {
    m.rollback = true;
    return false;
  }
  if (cur != 'd') {
    m.rollback = true;
    return false;
  }
  print_tabs(tabs);
  fprintf(_f, "d\n");
Lretry:
  if (list_wildcard(d, tabs + 1)) {
    goto Lretry;
  }
  if (int_wildcard(d, tabs + 1)) {
    goto Lretry;
  }
  if (string_wildcard(d, tabs + 1)) {
    goto Lretry;
  }
  if (dict_wildcard(d, tabs + 1)) {
    goto Lretry;
  }
  if (pop_front(d, cur) != 1) {
    m.rollback = true;
    return false;
  }
  if (cur != 'e') {
    m.rollback = true;
    return false;
  }
  print_tabs(tabs);
  fprintf(_f, "e\n");

  return true;
}
} // namespace internal

void
bencode_print_out(FILE *f) noexcept {
  _f = f;
}

template <>
void
bencode_print(sp::Buffer &d) noexcept {
  internal::dict_wildcard(d, 0);
}

template <>
void
bencode_print(const sp::Buffer &d) noexcept {
  sp::Buffer copy(d.raw, d.capacity);
  copy.length = d.length;

  internal::dict_wildcard(copy, 0);
}

template <>
void
bencode_print(sp::Thing &d) noexcept {
  internal::dict_wildcard(d, 0);
}

template <typename Buffer>
bool
find_entry(Buffer &, const char *key, /*OUT*/ sp::byte *val,
           /*IN/OUT*/ std::size_t &) noexcept {
  // XXX
  return true;
}
