#include "shared.h"
#include <cassert>
#include <utility>

namespace sp {

Buffer::Buffer(byte *s, std::size_t l) noexcept
    : start(s)
    , capacity(l)
    , length(0)
    , pos(0) {
}

byte &Buffer::operator[](std::size_t idx) noexcept {
  assert(idx < capacity);
  return start[idx];
}

void
flip(Buffer &b) noexcept {
  std::swap(b.length, b.pos);
}

} // namespace sp
