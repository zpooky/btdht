#include "shared.h"
#include <cassert>
#include <utility>

namespace sp {

Buffer::Buffer(byte *s, std::size_t l) noexcept
    : raw(s)
    , capacity(l)
    , length(0)
    , pos(0) {
}

byte &Buffer::operator[](std::size_t idx) noexcept {
  assert(idx < capacity);
  return raw[idx];
}

void
flip(Buffer &b) noexcept {
  std::swap(b.length, b.pos);
}

void
reset(Buffer &b) noexcept {
  b.length = 0;
  b.pos = 0;
}

byte *
offset(Buffer &b) noexcept {
  return b.raw + b.pos;
}

std::size_t
remaining_read(Buffer &b) noexcept {
  return b.length - b.pos;
}

std::size_t
remaining_write(Buffer &b) noexcept {
  return b.capacity - b.pos;
}

} // namespace sp
