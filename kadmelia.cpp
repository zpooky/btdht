#include "kadmelia.h"
#include <cstdint>

namespace kadmelia {
void
FIND_VALUE(const Key &) {
}

static void
distance(const Key &a, const Key &b, Key &result) {
  // distance(A,B) = |A xor B| Smaller values are closer.
  for (std::size_t i = 0; i < sizeof(Key); ++i) {
    result[i] = a[i] ^ b[i];
  }
}

} // namespace kadmelia
