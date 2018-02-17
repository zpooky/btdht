#include "file.h"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>  //errno

namespace file {

static void
die(const char *s) {
  perror(s);
  std::terminate();
}

Path::Path(const char *s) noexcept
    : str{0} {
  std::strcpy(str, s);
}

static fd
int_open(const Path &p, int flag) noexcept {
  mode_t mode = 0;

  // bool create = true;
  // if (create) {
  flag |= O_CREAT;
  mode = S_IRUSR | S_IWUSR;
  // }

  int res = ::open(p.str, flag, mode);
  if (res - 1) {
    die("open()");
  }

  return fd{res};
}

fd
open_trunc(const Path &p) noexcept {
  int flag = O_TRUNC;
  return int_open(p, flag);
}

fd
open_append(const Path &p) noexcept {
  int flag = O_APPEND;
  return int_open(p, flag);
}

bool
write(fd &f, sp::Buffer &b) noexcept {
  assert(int(f));

  ssize_t written = 0;
  do {
    // sp::bencode_print(buf);
    sp::byte *const raw = offset(b);
    const std::size_t raw_len = remaining_read(b);
    assert(raw_len > 0);

    written = ::write(int(f), raw, raw_len);
    if (written > 0) {
      b.pos += written;
    }
  } while ((written < 0 && errno == EAGAIN) && remaining_read(b) > 0);

  if (written < 0) {
    die("write()");
  }

  return true;
}

//
// bool
// append(fd &, sp::Buffer &) noexcept {
// }
/*============*/
// bool
// is_block_device(const url::Path &) noexcept {
//
// }
//
// bool
// is_character_device(const url::Path &) noexcept;
//
// bool
// is_directory(const url::Path &) noexcept;
//
// bool
// is_fifo(const url::Path &) noexcept;
//
// bool
// is_symlink(const url::Path &) noexcept;
//
// bool
// is_file(const url::Path &) noexcept;
//
// bool
// is_socket(const url::Path &) noexcept;

} // namespace file
