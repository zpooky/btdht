#ifndef SP_MAINLINE_DHT_SHARED_H
#define SP_MAINLINE_DHT_SHARED_H

#include <cstddef>
#include <cstdint>

class fd {
private:
  int m_fd;

public:
  explicit fd(int p_fd);

  fd(const fd &) = delete;
  fd(fd &&o);
  fd &
  operator=(const fd &) = delete;
  fd &
  operator=(const fd &&) = delete;

  ~fd();

  explicit operator int() noexcept;
};

using Port = std::uint16_t;
using Ip = std::uint32_t;

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
remaining_read(Buffer &) noexcept;

std::size_t
remaining_write(Buffer &) noexcept;

} // namespace sp

namespace dht {
/*mainline dht*/

using Key = sp::byte[20];

struct Infohash {
  Key id;
};

struct NodeId {
  Key id;
};

} // namespace dht

#endif
