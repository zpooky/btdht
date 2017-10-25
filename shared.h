#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>

namespace sp {

using byte = unsigned char;

struct Buffer {
  byte *start;
  const std::size_t length;
  std::size_t pos;

  Buffer(byte *, std::size_t) noexcept;

  template <std::size_t SIZE>
  explicit Buffer(byte (&buffer)[SIZE]) noexcept
      : Buffer(buffer, SIZE) {
  }

  byte &operator[](std::size_t) noexcept;
};

} // namespace sp

#endif
