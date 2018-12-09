#include "bencode_print.h"
#include <cstdio>
#include <string/ascii.h>

namespace internal {

static bool
dict_wildcard(sp::Buffer &d, std::size_t tabs) noexcept;

static void
print_tabs(std::size_t tabs) noexcept {
  for (std::size_t i = 0; i < tabs; ++i) {
    printf(" ");
  }
}

static void
print_raw(const char *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    printf("%.*s", int(len), val);
  } else {
    printf("hex[");
    for (std::size_t i = 0; i < len; ++i) {
      printf("%hhX", (unsigned char)val[i]);
    }
    printf("](");
    for (std::size_t i = 0; i < len; ++i) {
      if (ascii::is_printable(val[i])) {
        printf("%c", val[i]);
      } else {
        printf("_");
      }
    }
    printf(")");
  }
}

static bool
int_wildcard(sp::Buffer &d, std::size_t tabs) noexcept {
  std::uint64_t val = 0;
  if (bencode::d::value(d, val)) {
    print_tabs(tabs);
    printf("i%lue\n", val);
    return true;
  }
  return false;
}

static bool
string_wildcard(sp::Buffer &d, std::size_t tabs) noexcept {
  const char *val = nullptr;
  std::size_t len = 0;
  if (bencode::d::value_ref(d, val, len)) {
    print_tabs(tabs);
    printf("%zu:", len);
    print_raw(val, len);
    printf("\n");
    return true;
  }
  return false;
}

static bool
list_wildcard(sp::Buffer &d, std::size_t tabs) noexcept {
  const std::size_t pos = d.pos;
  if (d.raw[d.pos++] != 'l') {
    d.pos = pos;
    return false;
  }
  print_tabs(tabs);
  printf("l\n");
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
  if (d.raw[d.pos++] != 'e') {
    d.pos = pos;
    return false;
  }
  print_tabs(tabs);
  printf("e\n");

  return true;
}

static bool
dict_wildcard(sp::Buffer &d, std::size_t tabs) noexcept {
  const std::size_t pos = d.pos;
  if (d.raw[d.pos++] != 'd') {
    d.pos = pos;
    return false;
  }
  print_tabs(tabs);
  printf("d\n");
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
  if (d.raw[d.pos++] != 'e') {
    d.pos = pos;
    return false;
  }
  print_tabs(tabs);
  printf("e\n");

  return true;
}
} // namespace internal

namespace sp {
void
bencode_print(sp::Buffer &d) noexcept {
  internal::dict_wildcard(d, 0);
}

bool
find_entry(Buffer &, const char *key, /*OUT*/ byte *val,
           /*IN/OUT*/ std::size_t &) noexcept {
  // TODO
  return true;
}
} // namespace sp
