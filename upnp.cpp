#include "tcp.h"
#include "upnp.h"
#include <cstddef>
#include <util/assert.h>

namespace upnp {
upnp::upnp() noexcept
    : protocol("")
    , local(0)
    , external(0)
    , ip(Ipv4(0)) {
}

static std::size_t
format_body(char *buffer, size_t length, const upnp &data) noexcept {
  const char *desc = "spbtdht";
  static const char *const format = //
      "<?xml version=\"1.0\" ?>"
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
      "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
      "  <s:Body>"
      "    <u:AddPortMapping "
      "xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
      "      <NewRemoteHost>"
      "    </NewRemoteHost>"
      "    <NewExternalPort>"
      "      %u"
      "    </NewExternalPort>"
      "    <NewProtocol>"
      "      %s"
      "    </NewProtocol>"
      "    <NewInternalPort>"
      "      %u"
      "    </NewInternalPort>"
      "    <NewEnabled>"
      "      1"
      "    </NewEnabled>"
      "    <NewInternalClient>"
      "      %s"
      "    </NewInternalClient>"
      "    <NewLeaseDuration>"
      "      0"
      "    </NewLeaseDuration>"
      "    <NewPortMappingDescription>"
      "      %s"
      "    </NewPortMappingDescription>"
      "  </u:AddPortMapping>"
      "</s:Body>"
      "</s:Envelope>";

  char ip[32] = {0};
  if (!to_string(data.ip, ip, sizeof(ip))) {
    return 0;
  }

  int res = snprintf(buffer, length, format, data.external, data.protocol,
                     data.local, ip, desc);
  if (res <= 0) {
    return 0;
  }

  if (std::size_t(res + 1) > length) {
    assertxs(false, res);
    return 0;
  }

  return res;
}

bool
http_add_port(fd &fd, const upnp &data) noexcept {
  constexpr std::size_t header_cap = 1024;
  char header[header_cap] = {0};

  constexpr std::size_t content_cap = 1024 * 4;
  char content[content_cap] = {0};

  const char *format = //
      "POST %s HTTP/1.1\r\n"
      "HOST: %s:%u\r\n"
      "CONTENT-LENGTH: %zu\r\n"
      "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
      "SOAPACTION:\"%s#%s\"\r\n"
      "\r\n";

  std::size_t clen = format_body(content, content_cap, data);
  if (clen == 0) {
    return false;
  }

  // Contact local;
  // if (!tcp::local(fd, local)) {
  //   return false;
  // }

  char gateway_ip[32] = {0};
  Port gateway_port = 80;
  if (!to_string(data.ip, gateway_ip)) {
    return false;
  }

  const char *path = "/";

  const char *action = "urn:schemas-upnp-org:service:WANIPConnection:1";
  const char *operation = "AddPortMapping";
  int hlen = snprintf(header, header_cap, format, path, gateway_ip,
                      gateway_port, clen, action, operation);

  printf("%s\n", header);
  sp::BytesView hbuf((unsigned char *)header, std::size_t(hlen));
  hbuf.length = hlen;
  if (tcp::send(fd, hbuf) != 0) {
    return false;
  }
  assertxs(remaining_read(hbuf) == 0, remaining_read(hbuf));

  printf("%s\n", content);
  sp::BytesView cbuf((unsigned char *)content, clen);
  cbuf.length = clen;
  if (tcp::send(fd, cbuf) != 0) {
    return false;
  }
  assertxs(remaining_read(cbuf) == 0, remaining_read(cbuf));

  return true;
}

} // namespace upnp
