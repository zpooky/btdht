#include "shared.h"
namespace sp {

Buffer::Buffer(byte *s, std::size_t l) noexcept
    : start(s)
    , length(l)
    , pos(nullptr) {
}

} // namespace sp
