#include "cache.h"

#include "dht.h"
#include "shared.h"
#include "util.h"

#include <bootstrap.h>
#include <collection/Array.h>
#include <hash/djb2.h>
#include <hash/fnv.h>
#include <io/file.h>
#include <unistd.h>

#include <util/Bloomfilter.h>
#include <util/conversions.h>

#include <buffer/Sink.h>

#include <netdb.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 500

// TODO if no internet connection we will send out request for all nodes in
// bootstrap, then we will read nodes from cache then it will recurse until
// empty cache

// XXX ipv6
namespace sp {
//========================
struct CacheHeader {
  // [magic][version][count][ipv4/ipv6][[4byte:2byte]...]
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  char kind[4];
};

static_assert(sizeof(CacheHeader) == 4 + 4 + 4 + 4);

#define MAX_ENTRIES (256)
using Sink_as =
    typename std::aligned_storage<sizeof(Sink), alignof(Sink)>::type;

struct Cache {
  sp::StaticArray<sp::hasher<Contact>, 2> hashers;
  sp::BloomFilter<Contact, 8 * 1024> seen;

  fs::DirectoryFd dir{};

  size_t read_min_idx{~size_t(0)};
  size_t read_max_idx{};
  size_t contacts{};
  size_t write_idx{};

  // size_t cur_idx{};
  CacheHeader *cur_header{};
  sp::StaticCircularByteBuffer<128> cur_buf{};
  Sink_as cur_sink{};
  fd cur{};

  Cache() noexcept
      : hashers{}
      , seen{hashers} {
    assertx_n(insert(hashers, djb_contact));
    assertx_n(insert(hashers, fnv_contact));
  }

  Cache(const Cache &) = delete;
  Cache(const Cache &&) = delete;

  Cache &
  operator=(const Cache &) = delete;

  Cache &
  operator=(const Cache &&) = delete;
};

//========================
static void
on_retire_good(void *, const dht::Node &in) noexcept;

static void
on_topup_bootstrap(dht::DHT &) noexcept;

//========================
template <size_t SIZE>
static bool
cache_dir(char (&buffer)[SIZE]) noexcept {
  if (!xdg_share_dir(buffer)) {
    return false;
  }

  strcat(buffer, "/spdht");
  return true;
}

template <typename T>
static T *
ptr_add(T *base, uintptr_t l) noexcept {
  return (T *)(((uintptr_t)base) + l);
}

template <typename Function>
static bool
cache_for_each(fs::DirectoryFd &dir, const char *fname, Function f) noexcept {
  struct stat stat {};
  fd fd = fs::open_read(dir, fname);
  if (!fd) {
    return false;
  }
  if (fstat(int(fd), &stat) < 0) {
    return false;
  }

  if (stat.st_size < 0) {
    return false;
  }
  if (size_t(stat.st_size) < sizeof(CacheHeader)) {
    return false;
  }

  const size_t entry_size = (sizeof(Ipv4) + sizeof(Port));
  const size_t payload_length = size_t(stat.st_size) - sizeof(CacheHeader);
  if (payload_length % entry_size != 0) {
    return false;
  }

  bool result = false;
  void *const addr =
      ::mmap(nullptr, size_t(stat.st_size), PROT_READ, MAP_SHARED, int(fd), 0);
  if (addr) {
    // XXX what about alignemnt?
    const CacheHeader *const mem = (CacheHeader *)addr;
    const uint32_t entries = ntohl(mem->count);
    const size_t payload_entries = payload_length / entry_size;
    if (entries == payload_entries) {
      void *it = ptr_add(addr, sizeof(CacheHeader));
      void *end = ptr_add(it, (entry_size * entries));
      for (; it != end;) {
        Ipv4 ip;
        Port port;
        memcpy(&ip, it, sizeof(Ipv4));
        it = ptr_add(it, sizeof(Ipv4));
        memcpy(&port, it, sizeof(Port));
        it = ptr_add(it, sizeof(Port));
        Contact con(ntohl(ip), ntohs(port));
        f(con);
      }
      result = true;
    }

    ::munmap(addr, size_t(stat.st_size));
  }

  return result;
}

template <size_t SIZE>
static void
cache_filename(char (&fname)[SIZE], size_t idx) noexcept {
  sprintf(fname, "cache%zu_ipv4.db", idx);
}

static bool
filename_extract(const char *fname, uint32_t &idx) noexcept {
  const char *prefix = "cache";
  size_t prefix_len = strlen(prefix);

  if (strncmp(fname, prefix, prefix_len) == 0) {
    size_t fn_len = strlen(fname);
    const char *suffix = "_ipv4.db";
    size_t suf_len = strlen(suffix);

    if (fn_len > suf_len) {
      size_t tail = fn_len - suf_len;

      assertx(strlen(fname + tail) == strlen(suffix));
      if (strcmp(fname + tail, suffix) == 0) {
        char tmp[64]{0};
        size_t idx_len = (fn_len - prefix_len) - suf_len;

        if (idx_len >= sizeof(tmp)) {
          return false;
        }
        if (idx_len == 0) {
          return false;
        }

        memcpy(tmp, fname + prefix_len, idx_len);
        return parse_int(tmp, tmp + strlen(tmp), idx);
      }
    }
  }

  return false;
}

bool
init_cache(dht::DHT &ctx) noexcept {
  auto self = new Cache;
  ctx.routing_table.cache = self;
  emplace(ctx.routing_table.retire_good, on_retire_good, self);
  ctx.topup_bootstrap = on_topup_bootstrap;

  char root[PATH_MAX];
  if (!cache_dir(root)) {
    return false;
  }

  mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;
  if (!fs::mkdirs(root, mode)) {
    return false;
  }

  swap(self->dir, fs::open_dir(root));
  if (!self->dir) {
    return false;
  }

  auto cb = [](fs::DirectoryFd &parent, const char *fname, void *arg) {
    uint32_t idx = 0;
    if (filename_extract(fname, /*OUT*/ idx)) {
      auto s = (Cache *)arg;
      s->read_min_idx = std::min(s->read_min_idx, (size_t)idx);
      s->read_max_idx = std::max(s->read_max_idx, (size_t)idx);

      cache_for_each(parent, fname, [&](const Contact &good) { //
        insert(s->seen, good);
        s->contacts++;
      });
    }

    return true;
  };

  fs::for_each_files(self->dir, self, cb); // TODO whats the point?
  if (self->read_min_idx == ~size_t(0)) {
    assertxs(self->read_max_idx == 0, self->read_min_idx, self->read_max_idx);
    self->read_min_idx = 0;
  }
  // if (self->read_max_idx > 0) {
  //   self->cur_idx = self->read_max_idx + 1;
  // }
  self->write_idx = self->read_max_idx + 1;

  printf("self->read_min_idx: %zu\n", self->read_min_idx);
  printf("self->read_max_idx: %zu\n", self->read_max_idx);
  // printf("self->cur_idx: %zu\n", self->cur_idx);

  return true;
}

//========================
static bool
cache_finalize(Cache &self) noexcept {
  if (self.cur_header) {
    assertx(self.dir);
    assertxs(bool(self.cur), int(self.cur));

    char from[PATH_MAX]{0};
    char to[PATH_MAX]{0};

    munmap(self.cur_header, sizeof(CacheHeader));
    self.cur_header = nullptr;

    auto &sink = (Sink &)self.cur_sink;
    flush(sink);
    sink.~Sink();

    fd dummy{};
    swap(self.cur, dummy);

    cache_filename(from, self.write_idx);
    strcat(from, ".tmp");
    cache_filename(to, self.write_idx);
    self.write_idx++;

    if (::renameat(int(self.dir), from, int(self.dir), to) < 0) {
      return false;
    }
  }

  return true;
}

static bool
flush(CircularByteBuffer &b, void *arg) noexcept {
  // XXX make generic filesink
  assertx(arg);
  fd *f = (fd *)arg;

  if (!fs::write(*f, b)) {
    return false;
  }

  return true;
}

static bool
cache_write_contact(Cache &self, const Contact &in) noexcept {
  auto &sink = (Sink &)self.cur_sink;
  if (!self.cur_header) {
    assertx(bool(self.dir));
    assertx(!bool(self.cur));

    char fname[FILENAME_MAX]{0};
    cache_filename(fname, self.write_idx);
    strcat(fname, ".tmp");

    swap(self.cur, fs::open_trunc(self.dir, fname));
    if (!self.cur) {
      return false;
    }

    new (&sink) Sink{self.cur_buf, &self.cur, flush};

    CacheHeader header;
    {
      header.count = htonl(0);
      std::memcpy(header.kind, "ipv4", 4);
      header.magic = htonl(0xdead);
      header.version = htonl(1);

      write(sink, &header, sizeof(header));
      flush(sink);
    }

    void *addr = ::mmap(nullptr, sizeof(header), PROT_READ | PROT_WRITE,
                        MAP_SHARED, int(self.cur), 0);
    if (!addr) {
      return false;
    }
    self.cur_header = (CacheHeader *)addr; // XXX what about alignemnt
    assertx(header.count == self.cur_header->count);
    assertx(memcmp(header.kind, self.cur_header->kind, sizeof(header.kind)) ==
            0);
    assertx(header.magic == self.cur_header->magic);
    assertx(header.version == self.cur_header->version);
  }

  assertx(self.cur_header);

  Ipv4 ip = htonl(in.ip.ipv4);
  Port port = htons(in.port);
  uint32_t entries = ntohl(self.cur_header->count);
  write(sink, &ip, sizeof(ip));
  write(sink, &port, sizeof(port));
  ++entries;
  self.cur_header->count = htonl(entries);

  ++self.contacts;

  if (entries == MAX_ENTRIES) {
    cache_finalize(self);
  }

  return true;
}

void
deinit_cache(dht::DHT &ctx) noexcept {

  if (ctx.routing_table.cache) {
    auto self = (Cache *)ctx.routing_table.cache;
    /*drain*/
    auto cb = [](void *ctx2, const dht::DHTMetaRoutingTable &, const auto &,
                 const dht::Node &current) {
      auto s = (Cache *)((dht::DHT *)ctx2)->routing_table.cache;
      cache_write_contact(*s, current.contact);
    };
    debug_for_each(ctx.routing_table, &ctx, cb);

    if (self->contacts < (2 * 1024)) { // XXX configurable
      for_each(ctx.bootstrap, [&](const dht::KContact &boot) {
        cache_write_contact(*self, boot.contact);
      });
    }

    cache_finalize(*self);
    delete self;
    ctx.routing_table.cache = nullptr; // TODO how to handle on_good_callback?
  }
}

//========================
template <size_t SIZE>
static bool
take_next_read_cache(Cache &self, char (&file)[SIZE]) noexcept {
  assertx(self.dir);
  while (self.read_min_idx < self.read_max_idx) {
    cache_filename(file, self.read_max_idx);
    self.read_min_idx--;

    if (::faccessat(int(self.dir), file, R_OK, 0) == 0) {
      return true;
    }
  }

  return false;
}

static void
on_retire_good(void *tmp, const dht::Node &in) noexcept {
  Cache *self = (Cache *)tmp;
  if (self) {
    if (!test(self->seen, in.contact)) {
      insert(self->seen, in.contact);
      cache_write_contact(*self, in.contact);
    }
  }
}

//========================
template <typename T, size_t SIZE, typename F>
static void
for_each(T (&arr)[SIZE], F f) noexcept {
  for (size_t i = 0; i < SIZE; ++i) {
    f(arr[i]);
  }
}

static bool
dns_lookup(dht::DHT &self, const char *hostname, uint16_t port) {
  struct addrinfo *result = nullptr;
  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  if (::getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
    return false;
  }

  for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
    if (rp->ai_family == AF_INET && rp->ai_addr) {
      Contact contact;
      struct sockaddr_in *ai_addr = (struct sockaddr_in *)rp->ai_addr;
      to_contact(ai_addr->sin_addr, port, contact);

      bootstrap_insert(self, contact);
    }
  }

  ::freeaddrinfo(result);
  return true;
}

static bool
setup_static_bootstrap(dht::DHT &self) noexcept {
  dns_lookup(self, "dht.libtorrent.org", 25401);
  dns_lookup(self, "router.bittorrent.com", 6881);
  dns_lookup(self, "router.utorrent.com", 6881);
  dns_lookup(self, "dht.transmissionbt.com", 6881);
  dns_lookup(self, "dht.aelitis.com", 6881);
#if 0
  const char *bss[] = {"0.0.0.0:12456"};

  for_each(bss, [&self](const char *ip) {
    Contact bs;
    assertx_n(to_contact(ip, bs));

    assertx(bs.ip.ipv4 > 0);
    assertx(bs.port > 0);

    bootstrap_insert(self, dht::IdContact(dht::NodeId{},Contact(Ipv4(0), 22288)));
  });
#else
  (void)self;
#endif

  return true;
}

static void
on_topup_bootstrap(dht::DHT &ctx) noexcept {
  char fname[FILENAME_MAX]{0};
  bool cont = true;
  if (!ctx.routing_table.cache) {
    return;
  }
  Cache &self = *((Cache *)ctx.routing_table.cache);

  assertx(!is_full(ctx.bootstrap));

  // printf("-------------------\n");
  while (cont && take_next_read_cache(self, /*OUT*/ fname)) {
    // printf("- fname[%s]", fname);
    size_t bi = 0;
    size_t cw = 0;
    cache_for_each(self.dir, fname, [&](const Contact &cur) {
      assertxs(self.contacts > 0, self.contacts);
      --self.contacts;

      if (!is_full(ctx.bootstrap)) {
        ++bi;
        bootstrap_insert(ctx, cur);
      } else {
        ++cw;
        cont = false;
        cache_write_contact(self, cur);
      }

      if (is_full(ctx.bootstrap)) {
        cont = false;
      }
    });
    // printf("bs_insert[%zu]cache_write[%zu], bootstrap_lengt[%zu],
    // is_full:%s\n",
    //        bi, cw, length(ctx.bootstrap),
    //        is_full(ctx.bootstrap) ? "TRUE" : "FALSE");

    if (::unlinkat(int(self.dir), fname, 0) < 0) {
      printf("unlinkat(%s): %s\n", fname, strerror(errno));
      exit(1);
    }
  }
  if (cont) {
    /* bootstrap want more but there is no more cache files */
    clear(self.seen);
  }

  if (is_empty(ctx.bootstrap)) {
    setup_static_bootstrap(ctx);
  }
}

//========================

size_t
cache_read_min_idx(const dht::DHT &ctx) noexcept {
  auto self = (Cache *)ctx.routing_table.cache;
  return self ? self->read_min_idx : 0;
}

size_t
cache_read_max_idx(const dht::DHT &ctx) noexcept {
  auto self = (Cache *)ctx.routing_table.cache;
  return self ? self->read_max_idx : 0;
}

size_t
cache_contacts(const dht::DHT &ctx) noexcept {
  auto self = (Cache *)ctx.routing_table.cache;
  return self ? self->contacts : 0;
}

size_t
cache_write_idx(const dht::DHT &ctx) noexcept {
  auto self = (Cache *)ctx.routing_table.cache;
  return self ? self->write_idx : 0;
}

} // namespace sp
