#include "cache.h"

#include "shared.h"
#include <bootstrap.h>
#include <collection/Array.h>
#include <hash/djb2.h>
#include <hash/fnv.h>
#include <io/file.h>
#include <unistd.h>
#include <util/Bloomfilter.h>

// TODO ipv4 & ipv6
namespace sp {
//========================
static void
on_retire_good(dht::DHT &ctx, Contact in) noexcept;

static void
on_topup_bootstrap(dht::DHT &) noexcept;

//========================
struct CacheHeader {
  uint32_t magic;
  char kind[4];
  uint32_t version;
  uint32_t count;
};

struct Cache {
  sp::StaticArray<sp::hasher<Contact>, 2> hashers;
  sp::BloomFilter<Contact, 8 * 1024> seen; // TODO how to reset bloomfilter?

  size_t read_min_idx{};
  size_t read_max_idx{};

  size_t write_cur_idx;
  CacheHeader* cur_header;
  fd cur{};
  // TODO [magic][ipv4/ipv6][version][count][[4byte:2byte]...]
  // TODO mmap file when writing so to be able to update count

  Cache() noexcept
      : hashers{}
      , seen{hashers} {
    assertx_n(insert(hashers, djb_contact));
    assertx_n(insert(hashers, fnv_contact));
  }
};

//========================
template <size_t SIZE>
static bool
xdg_cache_dir(char (&buffer)[SIZE]) noexcept {
  // read env $XDG_DATA_HOME default to $HOME/.local/share
  // $XDG_CACHE_HOME default equal to $HOME/.cache
  const char *data = getenv("XDG_DATA_HOME");
  if (data == NULL || strcmp(data, "") == 0) {
    const char *home = getenv("HOME");
    assertx(home);
    sprintf(buffer, "%s/.local/share", home);
    return true;
  }

  sprintf(buffer, "%s", data);
  return true;
}

template <size_t SIZE>
static bool
cache_dir(char (&buffer)[SIZE]) noexcept {
  if (!xdg_cache_dir(buffer)) {
    return false;
  }

  strcat(buffer, "/spdht");
  return true;
}

template <typename Function>
static void
cache_for_each(const char *file, Function f) noexcept {
  // TODO
}

bool
init_cache(dht::DHT &ctx) noexcept {
  auto cache = new Cache;
  ctx.cache = cache;
  ctx.retire_good = on_retire_good;
  ctx.topup_bootstrap = on_topup_bootstrap;

  char root[PATH_MAX];
  if (!cache_dir(root)) {
    return false;
  }

  mode_t mode = 0; // TODO
  if (!fs::mkdirs(root, mode)) {
    return false;
  }

  fs::for_each_files_cb_t cb = [](const char *file, void *self) {
    // TODO store the highest index file and the lowest index file
    //   - highest to know what the next cache index should be named
    //   - lowest to know which file to read when we seed bootstrap nodes
    cache_for_each(file, [&](const Contact &good) { //
      insert(((Cache *)self)->seen, good);
    });
    return true;
  };

  fs::for_each_files(root, cache, cb);

  return true;
}

//========================
void
deinit_cache(dht::DHT &ctx) noexcept {
  auto self = (Cache *)ctx.cache;
  if (self) {
    delete self;
    ctx.cache = nullptr;
  }
}

//========================
template <size_t SIZE>
static bool
take_next_read_cache(Cache &self, char (&file)[SIZE]) {
  while (self.read_min_idx < self.read_max_idx) {
    char filename[FILENAME_MAX]{0};
    if (!cache_dir(file)) {
      return false;
    }
    sprintf(filename, "cache%zu_ipv4.db", self.read_min_idx);
    strcat(file, filename);

    self.read_min_idx++;
    if (access(file, R_OK) == 0) {
      return true;
    }
  }

  return false;
}

static bool
write_contact(Cache &self, const Contact &in) noexcept {
  // TODO use .tmp for wip cache files
  return true;
}

static void
on_retire_good(dht::DHT &ctx, Contact in) noexcept {
  Cache *self = (Cache *)ctx.cache;
  if (!test(self->seen, in)) {
    insert(self->seen, in);
    write_contact(*self, in);
  }
}

//========================
template <typename T, std::size_t SIZE, typename F>
static void
for_each(T (&arr)[SIZE], F f) noexcept {
  for (std::size_t i = 0; i < SIZE; ++i) {
    f(arr[i]);
  }
}

static bool
setup_static_bootstrap(dht::DHT &self) noexcept {
  /*boostrap*/
  const char *bss[] = {
      // "192.168.1.47:13596",
      // "127.0.0.1:13596",
      // "213.65.130.80:13596",
      "192.168.1.49:51413",    //
      "192.168.2.14:51413",    //
      "109.228.170.47:51413",  //
      "192.168.0.15:13596",    //
      "127.0.0.1:13596",       //
      "192.168.0.113:13596",   //
      "192.168.1.49:13596",    //
      "213.174.1.219:13680",   //
      "90.243.184.9:6881",     //
      "105.99.128.147:40189",  //
      "79.160.16.63:6881",     //
      "24.34.3.237:6881",      //
      "2.154.168.153:6881",    //
      "47.198.79.139:50321",   //
      "94.181.155.240:47661",  //
      "213.93.18.70:24896",    //
      "173.92.231.220:6881",   //
      "93.80.248.65:8999",     //
      "95.219.168.133:50321",  //
      "85.247.221.231:19743",  //
      "62.14.189.18:57539",    //
      "195.191.186.170:30337", //
      "192.168.0.112:13596",   //
      "51.68.37.227:28011",    //
      "24.146.6.127:6881",     //
      "109.107.81.218:38119",  //
                               // "127.0.0.1:51413", "213.65.130.80:51413",
  };

  for_each(bss, [&self](const char *ip) {
    Contact bs;
    assertx_n(to_contact(ip, bs));

    assertx(bs.ip.ipv4 > 0);
    assertx(bs.port > 0);

    bootstrap_insert(self, dht::KContact(0, bs));
  });

  return true;
}

static void
on_topup_bootstrap(dht::DHT &ctx) noexcept {
  char file[PATH_MAX]{0};
  if (take_next_read_cache(*((Cache *)ctx.cache), /*OUT*/ file)) {
    cache_for_each(file, [&](const Contact &cur) {
      if (is_full(ctx.bootstrap)) {
        bootstrap_insert(ctx, dht::KContact(0, cur));
      } else {
        write_contact(*((Cache *)ctx.cache), cur);
      }
    });
  }

  if (!is_full(ctx.bootstrap)) {
    setup_static_bootstrap(ctx);
  }
}

//========================

} // namespace sp
