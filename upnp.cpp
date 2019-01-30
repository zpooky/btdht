#include "tcp.h"
#include "upnp.h"
#include <cstddef>
#include <util/assert.h>

namespace upnp {
upnp::upnp(sp::Seconds t) noexcept
    : protocol("")
    , local(0)
    , external(0)
    , ip(Ipv4(0))
    , timeout(t) {
}

static std::size_t
format_body(char *buffer, size_t length, const char *action, const upnp &data,
            sp::Seconds tout) noexcept {

#if 0
	tpl := `<?xml version="1.0" ?>
	<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
	<s:Body>%s</s:Body>
	</s:Envelope>
`
#endif

  const char *desc = "spbtdht";
  static const char *const format = //
      "<?xml version=\"1.0\" ?>"
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
      "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
      "  <s:Body>"
      "    <u:AddPortMapping xmlns:u=\"%s\">"
      // "      <NewRemoteHost></NewRemoteHost>"
      "    <NewExternalPort>%u</NewExternalPort>"
      "    <NewProtocol>%s</NewProtocol>"
      "    <NewInternalPort>%u</NewInternalPort>"
      "    <NewInternalClient>%s</NewInternalClient>"
      "    <NewEnabled>1</NewEnabled>"
      "    <NewPortMappingDescription>%s</NewPortMappingDescription>"
      "    <NewLeaseDuration>%llu</NewLeaseDuration>"
      "  </u:AddPortMapping>"
      "</s:Body>"
      "</s:Envelope>";

  char ip[32] = {0};
  if (!to_string(data.ip, ip, sizeof(ip))) {
    return 0;
  }

  int res = snprintf(buffer, length, format, action, data.external,
                     data.protocol, data.local, ip, desc, tout.value);
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
  constexpr std::size_t cheader = 1024;
  char header[cheader] = {0};

  constexpr std::size_t cconten = 1024 * 4;
  char content[cconten] = {0};

  const char *action = "urn:schemas-upnp-org:service:WANIPConnection:1";
  std::size_t clen = format_body(content, cconten, action, data, data.timeout);
  if (clen == 0) {
    return false;
  }

  Contact gateway;
  if (!tcp::remote(fd, gateway)) {
    return false;
  }

  char gateway_ip[32] = {0};
  if (!to_string(gateway.ip, gateway_ip)) {
    return false;
  }

  const char *path = "/ctl/IPConn";

  // http://192.168.2.1:49152/upnp/control/WANIPConn1

  const char *format = //
      "POST %s HTTP/1.1\r\n"
      "host: %s:%u\r\n"
      "Content-Length: %zu\r\n"
      "Content-Type: text/xml; charset=\"utf-8\"\r\n"
      "Connection: Close\r\n"
      "Cache-Control: no-cache\r\n"
      "Pragma: no-cache\r\n"
      "SOAPAction: %s#%s\r\n"
      "\r\n";

  const char *operation = "AddPortMapping";
  int hlen = snprintf(header, cheader, format, path, gateway_ip, gateway.port,
                      clen, action, operation);

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
