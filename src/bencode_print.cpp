#include "bencode_print.h"
#include "decode_bencode.h"
#include "util.h"
#include <buffer/Thing.h>
#include <cstddef>
#include <cstdio>
#include <string/ascii.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    dht::print_hex((const sp::byte *)val, len);
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
  char val[1024*4];
  std::size_t len = sizeof(val);
  if (sp::bencode::d<Buffer>::value(d, val, len)) {
    print_tabs(tabs);
    fprintf(_f, "%zu:", len);
    print_raw(val, len);
    fprintf(_f, "\n");
    return true;
  }

  m.rollback = true;
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

//=====================================
void
bencode_print_out(FILE *f) noexcept {
  _f = f;
}

//=====================================
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

//=====================================
void
bencode_print_file(const char *file) noexcept {
  fd fd{open(file, O_RDONLY)};
  if (!fd) {
    assertx(false);
  }

  struct stat st;
  memset(&st, 0, sizeof(st));
  if (fstat(int(fd), &st) < 0) {
    assertx(false);
  }

  size_t file_size = (size_t)st.st_size;

  if (!S_ISREG(st.st_mode)) {
    assertx(false);
  }

  int prot = PROT_READ;
  int flags = MAP_SHARED;

  void *raw;
  if ((raw = mmap(nullptr, file_size, prot, flags, int(fd), 0)) == MAP_FAILED) {
    assertx(false);
  }
  sp::Buffer buf((sp::byte *)raw, file_size);
  buf.length = file_size;
  bencode_print(buf);

  munmap(raw, file_size);
}

//=====================================
template <typename Buffer>
bool
find_entry(Buffer &, const char *key, /*OUT*/ sp::byte *val,
           /*IN/OUT*/ std::size_t &) noexcept {
  // XXX
  return true;
}

//=====================================
