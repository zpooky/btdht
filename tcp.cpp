#include "tcp.h"
#include <sys/errno.h>  //errno
#include <sys/socket.h> //socket
#include <unistd.h>     //close

namespace tcp {
//=====================================
bool
local(fd &listen, Contact &out) noexcept {
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  socklen_t slen = sizeof(addr);
  sockaddr *saddr = (sockaddr *)&addr;

  int ret = ::getsockname(int(listen), saddr, &slen);
  if (ret < 0) {
    return false;
  }

  return to_contact(addr, out);
}

//=====================================
} // namespace tcp
