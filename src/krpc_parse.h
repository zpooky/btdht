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
struct GetPeersRequest {
  dht::NodeId id;
  dht::Infohash infohash;
  sp::maybe<bool> noseed{};
  bool scrape = false;

  bool n4 = false;
  bool n6 = false;
};

struct GetPeersResponse {
  dht::NodeId id;
  dht::Token token;

  sp::UinStaticArray<Contact, 256> values;
  sp::UinStaticArray<dht::IdContact, 256> nodes;
};

bool
parse_get_peers_request(dht::MessageContext &ctx, GetPeersRequest &out);

bool
parse_get_peers_response(dht::MessageContext &ctx, GetPeersResponse &out);

// ========================================
struct AnnouncePeerRequest {
  dht::NodeId id;
  bool implied_port = false;
  dht::Infohash infohash;
  Port port = 0;
  dht::Token token;
  // According to BEP-33 if "seed" is omitted we assume it is a peer not a
  // seed
  bool seed = false;

  const char *name = nullptr;
  size_t name_len = 0;
};

struct AnnouncePeerResponse {
  dht::NodeId id;
};

bool
parse_announce_peer_request(dht::MessageContext &ctx, AnnouncePeerRequest &out);

bool
parse_announce_peer_response(dht::MessageContext &ctx,
                             AnnouncePeerResponse &out);

// ========================================
struct SampleInfohashesRequest {
  dht::NodeId sender;
  dht::Infohash ih;
};

struct SampleInfohashesResponse {};

bool
parse_sample_infohashes_request(dht::MessageContext &ctx,
                                SampleInfohashesRequest &out);

bool
parse_sample_infohashes_response(dht::MessageContext &ctx,
                                 SampleInfohashesResponse &out);

// ========================================

} // namespace krpc

#endif
