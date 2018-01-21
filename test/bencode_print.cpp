#include "bencode_print.h"
#include <cstdio>

static bool
dict_wildcard(bencode::d::Decoder &d, std::size_t tabs) noexcept;

static void
print_tabs(std::size_t tabs) noexcept {
  for (std::size_t i = 0; i < tabs; ++i) {
    printf(" ");
  }
}

static bool
is_printable(char c) noexcept {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') || c == '\'' || c == '`' || c == ' ';
}

static bool
is_only_printable(const char *val, std::size_t len) noexcept {
  for (std::size_t i = 0; i < len; ++i) {
    if (!is_printable(val[i])) {
      return false;
    }
  }
  return true;
}

static void
print_len(const char *val, std::size_t len) noexcept {
  if (is_only_printable(val, len)) {
    for (std::size_t i = 0; i < len; ++i) {
      printf("%c", val[i]);
    }
  } else {
    printf("hex[");
    for (std::size_t i = 0; i < len; ++i) {
      printf("%hhX", val[i]);
    }
    printf("]");
  }
}

static bool
int_wildcard(bencode::d::Decoder &d, std::size_t tabs) noexcept {
  std::uint64_t val = 0;
  if (bencode::d::value(d, val)) {
    print_tabs(tabs);
    printf("i%lue\n", val);
    return true;
  }
  return false;
}

static bool
string_wildcard(bencode::d::Decoder &d, std::size_t tabs) noexcept {
  const char *val = nullptr;
  std::size_t len = 0;
  if (bencode::d::value_ref(d, val, len)) {
    print_tabs(tabs);
    printf("%zu:", len);
    print_len(val, len);
    printf("\n");
    return true;
  }
  return false;
}

static bool
list_wildcard(bencode::d::Decoder &d, std::size_t tabs) noexcept {
  const std::size_t pos = d.buf.pos;
  if (d.buf.raw[d.buf.pos++] != 'l') {
    d.buf.pos = pos;
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
  if (d.buf.raw[d.buf.pos++] != 'e') {
    d.buf.pos = pos;
    return false;
  }
  print_tabs(tabs);
  printf("e\n");

  return true;
}

static bool
dict_wildcard(bencode::d::Decoder &d, std::size_t tabs) noexcept {
  const std::size_t pos = d.buf.pos;
  if (d.buf.raw[d.buf.pos++] != 'd') {
    d.buf.pos = pos;
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
  if (d.buf.raw[d.buf.pos++] != 'e') {
    d.buf.pos = pos;
    return false;
  }
  print_tabs(tabs);
  printf("e\n");

  return true;
}

void
bencode_print(bencode::d::Decoder &d) noexcept {
  dict_wildcard(d, 0);
}
