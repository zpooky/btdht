#include "mainline.h"
#include <stdio.h>

#include <arpa/inet.h>
#include <sys/socket.h> //socket
#include <unistd.h>     //close
#include <cstring>
#include <exception>

void
die(const char *s) {
  perror(s);
  std::terminate();
}

int
main() {
  dht::DHT dht;
  dht::randomize(dht.id);

  const std::uint16_t port = 0;

  int udp = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udp < 0) {
    die("socket()\n");
  }
  sockaddr_in me;
  std::memset(&me, 0, sizeof(me));

  me.sin_family = AF_INET;
  me.sin_port = htons(port);
  me.sin_addr.s_addr = htonl(INADDR_ANY);

  // bind socket to port
  if (bind(udp, (sockaddr *)&me, sizeof(me)) == -1) {
    die("bind");
  }

  ::close(udp);
  return 0;
}
