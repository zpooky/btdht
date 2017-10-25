#include "shared.h"
#include <cassert>

namespace sp {

Buffer::Buffer(byte *s, std::size_t l) noexcept
    : start(s)
    , length(l)
    , pos(0) {
}

byte &Buffer::operator[](std::size_t idx) noexcept {
  assert(idx < length);
  return start[idx];
}

} // namespace sp
