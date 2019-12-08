#include "cache.h"

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
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// 9FFEE58A4F6BB683FB7DBA9A7D553EBAD31EDFB59F56D6AD079EC180C17549F064CE6107956FFB274E52F0C8C8EA9F643444069CBA7B2D2E145DCC60B97B86769E3CA3ACA4CBC8D59F4B497256A049D0B67878F3D0A66744FE590BDF054F4D2BB44E9F42BD223EFE16BF1A3782BD6B7CE1C8DC9ECB3DB92DC3C06D949F7EE676D0953249FDBFBF3E4257EE7E4342CB1DB0096536C8D59F189E6B80EB338C9D87EE84DC44496E055DA1175E822F83C65D9F6A279F1F4E7132F3CAF30E78F8F1C157EFC66AB01C0C30C8D59F431DF8508C53F38191AA64B7DD5BE296CCB83C3ED2D32FD78Dgood[102],
// total[102], b

// XXX ipv6
namespace sp {
//========================
static void
on_retire_good(dht::DHT &ctx, Contact in) noexcept;

static void
on_topup_bootstrap(dht::DHT &) noexcept;

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

  size_t read_min_idx{};
  size_t read_max_idx{};

  size_t cur_idx{};
  CacheHeader *cur_header{};
  sp::StaticCircularByteBuffer<4096> cur_buf{};
  Sink_as cur_sink{};
  fd cur{};

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

  if (stat.st_size < sizeof(CacheHeader)) {
    return false;
  }

  const size_t entry_size = (sizeof(Ipv4) + sizeof(Port));
  const uint32_t payload_length = stat.st_size - sizeof(CacheHeader);
  if (payload_length % entry_size != 0) {
    return false;
  }

  bool result = false;
  void *addr = ::mmap(nullptr, stat.st_size, PROT_READ, MAP_SHARED, int(fd), 0);
  if (addr) {
    const CacheHeader *const mem = (CacheHeader *)addr;
    const uint32_t entries = ntohl(mem->count);
    const uint32_t payload_entries = payload_length / entry_size;
    if (entries == payload_entries) {
      void *it = addr + sizeof(CacheHeader);
      void *end = it + (entry_size * entries);
      for (; it != end;) {
        Ipv4 ip;
        Port port;
        memcpy(&ip, it, sizeof(Ipv4));
        it += sizeof(Ipv4);
        memcpy(&port, it, sizeof(Port));
        it += sizeof(Port);
        Contact con(ntohl(ip), ntohs(port));
        f(con);
      }
      result = true;
    }

    ::munmap(addr, stat.st_size);
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
        char tmp[64];
        size_t idx_len = (fn_len - prefix_len) - suf_len;

        assertxs(idx_len < sizeof(tmp) && idx_len > 0, idx_len);
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
  ctx.cache = self;
  ctx.retire_good = on_retire_good;
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
      auto self = (Cache *)arg;
      self->read_min_idx = std::min(self->read_min_idx, (size_t)idx);
      self->read_max_idx = std::max(self->read_max_idx, (size_t)idx);

      cache_for_each(parent, fname, [&](const Contact &good) { //
        insert(self->seen, good);
      });
    }
    return true;
  };

  fs::for_each_files(self->dir, self, cb);

  return true;
}

//========================
static bool
cache_finalize(Cache &self) noexcept {
  if (self.cur_header) {
    assertx(self.dir);

    char from[PATH_MAX]{0};
    char to[PATH_MAX]{0};

    munmap(self.cur_header, sizeof(CacheHeader));
    self.cur_header = nullptr;

    auto sink = (Sink &)self.cur_sink;
    flush(sink);
    sink.~Sink();

    fd dummy{};
    swap(self.cur, dummy);

    cache_filename(from, self.cur_idx);
    strcat(from, ".tmp");
    cache_filename(to, self.cur_idx);
    self.cur_idx++;

    if (::renameat(int(self.dir), from, int(self.dir), to) < 0) {
      return false;
    }
  }

  return true;
}

void
deinit_cache(dht::DHT &ctx) noexcept {
  auto self = (Cache *)ctx.cache;
  if (self) {
    cache_finalize(*self);
    delete self;
    ctx.cache = nullptr;
  }
}

//========================
template <size_t SIZE>
static bool
take_next_read_cache(Cache &self, char (&file)[SIZE]) noexcept {
  assertx(self.dir);
  while (self.read_min_idx < self.read_max_idx) {
    cache_filename(file, self.read_min_idx);
    self.read_min_idx++;

    if (::faccessat(int(self.dir), file, R_OK, 0) == 0) {
      return true;
    }
  }

  return false;
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
  auto& sink = (Sink &)self.cur_sink;
  if (!self.cur_header) {
    assertx(bool(self.dir));
    assertx(!bool(self.cur));

    char fname[FILENAME_MAX]{0};
    cache_filename(fname, self.cur_idx++);
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
    self.cur_header = (CacheHeader *)addr;
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

  if (entries == MAX_ENTRIES) {
    cache_finalize(self);
  }

  return true;
}

static void
on_retire_good(dht::DHT &ctx, Contact in) noexcept {
  Cache *self = (Cache *)ctx.cache;
  if (!test(self->seen, in)) {
    insert(self->seen, in);
    cache_write_contact(*self, in);
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
  Cache &self = *((Cache *)ctx.cache);
  char fname[FILENAME_MAX]{0};

  if (take_next_read_cache(self, /*OUT*/ fname)) {
    cache_for_each(self.dir, fname, [&](const Contact &cur) {
      if (is_full(ctx.bootstrap)) {
        bootstrap_insert(ctx, dht::KContact(0, cur));
      } else {
        cache_write_contact(self, cur);
      }
    });
  } else {
    clear(self.seen);
  }

  if (!is_full(ctx.bootstrap)) {
    setup_static_bootstrap(ctx);
  }
}

//========================

} // namespace sp
