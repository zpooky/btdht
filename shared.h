#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>

namespace sp {

using byte = unsigned char;

struct Buffer {
  byte *raw;
  const std::size_t capacity;
  std::size_t length;
  std::size_t pos;

  Buffer(byte *, std::size_t) noexcept;

  template <std::size_t SIZE>
  explicit Buffer(byte (&buffer)[SIZE]) noexcept
      : Buffer(buffer, SIZE) {
  }

  byte &operator[](std::size_t) noexcept;
};

void
flip(Buffer &) noexcept;

void
reset(Buffer &) noexcept;

byte *
offset(Buffer &) noexcept;

std::size_t
remaining(Buffer &) noexcept;

std::size_t
remaining_write(Buffer &) noexcept;

} // namespace sp

#endif
