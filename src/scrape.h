#ifndef SP_MAINLINE_DHT_SCRAPE_H
#define SP_MAINLINE_DHT_SCRAPE_H

#include "module.h"

namespace interface_setup {
bool
setup(dht::Modules &, bool setup_cb) noexcept;
}

#endif
