#ifndef SP_MAINLINE_DHT_KRPC_PARSE_H
#define SP_MAINLINE_DHT_KRPC_PARSE_H

#include "shared.h"
#include "util.h"

namespace krpc {

// ========================================
struct PingRequest {
  dht::NodeId sender;
};

struct PingResponse {
  dht::NodeId sender;
};

bool
parse_ping_request(dht::MessageContext &ctx, PingRequest &out);

bool
parse_ping_response(dht::MessageContext &ctx, PingResponse &out);

// ========================================
struct FindNodeRequest {
  dht::NodeId id;
  dht::NodeId target;

  bool n4 = false;
  bool n6 = false;
};

struct FindNodeResponse {
  dht::NodeId id;
  dht::Token token;
  sp::UinStaticArray<dht::IdContact, 256> nodes;
};

bool
parse_find_node_request(dht::MessageContext &ctx, FindNodeRequest &out);

bool
parse_find_node_response(dht::MessageContext &ctx, FindNodeResponse &out);

// ========================================

} // namespace krpc

#endif
