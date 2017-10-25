#include "app.h"

namespace sp {
int
fun(int arg) {
#ifdef SP_TEST
  return arg;
#else
  return arg + 3;
#endif
}

} // namespace sp
