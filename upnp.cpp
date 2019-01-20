#include "upnp.h"
#include "util.h"
#include <cstddef>

struct upnp {
  const char *protocol;
  Port local;
  Port external;
  Ip ip;
};

static std::size_t
add_port(char *buffer, size_t length, const upnp &data) noexcept {
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
  if ((res + 1) > length) {
    assertxs(false, res);
  }

  return res;
}

static bool
http_add_port(const upnp &data, fd &) {
  constexpr std::size_t header_cap = 1024;
  char header[header_cap] = {0};

  constexpr std::size_t content_cap = 1024 * 4;
  char content[content_cap] = {0};

  const char *format = //
      "POST %s HTTP/1.1\r\n"
      "HOST: %s:%d\r\n"
      "CONTENT-LENGTH: %zu\r\n"
      "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
      "SOAPACTION:\"%s#%s\"\r\n"
      "\r\n";

  std::size_t content_length = add_port(content, content_cap, data);
  if (content_length == 0) {
    return false;
  }

  const char *path = "";
  Port gateway = 0;
  const char *gateway_port = "";
  const char *action = "urn:schemas-upnp-org:service:WANIPConnection:1";
  const char *operation = "AddPortMapping";
  snprintf(header, header_cap, format, path, gateway, gateway_port,
           content_length, action, operation);

  return true;
}
