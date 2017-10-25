#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>

namespace sp {

using byte = unsigned char;

struct Buffer {
  byte *start;
  std::size_t length;
  byte *pos;

  Buffer(byte *, std::size_t) noexcept;
  template <std::size_t SIZE>
  explicit Buffer(byte (&buff)[SIZE]) noexcept
      : Buffer(buff, SIZE) {
  }
};

} // namespace shared

#endif
