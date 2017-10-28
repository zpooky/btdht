#include "kadmelia.h"
#include <cstdint>

namespace kadmelia {
void
FIND_VALUE(const Key &) {
}

static std::uint32_t
distance(std::uint32_t a, std::uint32_t b) {
  // distance(A,B) = |A xor B| Smaller values are closer.
  return a ^ b;
}

} // namespace kadmelia
