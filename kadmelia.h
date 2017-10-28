#ifndef SP_MAINLINE_DHT_KADMELIA_H
#define SP_MAINLINE_DHT_KADMELIA_H

#include "shared.h"

namespace kadmelia {
/*Kadmelia*/
using Sha1 = sp::byte[20];
using Key = Sha1;

void
FIND_VALUE(const Key &);

} // namespace kadmelia

#endif
