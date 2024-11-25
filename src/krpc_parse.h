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
  // "id" : "<querying nodes id>",
  dht::NodeId sender;
  // "target" : "<id of target node>"
  dht::NodeId target;

  bool n4 = false;
  bool n6 = false;
};

struct FindNodeResponse {
  // "id" : "<queried nodes id>",
  dht::NodeId id;
  dht::Token token; // TODO?
  // "nodes" : "<compact node info>"
  sp::UinStaticArray<dht::IdContact, 256> nodes;
};

bool
parse_find_node_request(dht::MessageContext &ctx, FindNodeRequest &out);

bool
parse_find_node_response(dht::MessageContext &ctx, FindNodeResponse &out);

// ========================================
struct GetPeersRequest {
  // "id" : "<querying nodes id>",
  dht::NodeId sender;
  // "info_hash" : "<20-byte infohash of target torrent>"
  dht::Infohash infohash;
  sp::maybe<bool> noseed{};
  bool scrape = false;
  bool bootstrap = false; // TODO do something with it.

  bool n4 = false;
  bool n6 = false;
};

struct GetPeersResponse {
  // "id" : "<queried nodes id>"
  dht::NodeId id;
  // The token value is a required argument for a future announce_peer query.
  // "token" :"<opaque write token>"
  dht::Token token;

  // peers for the queried infohash
  //  - "compact" peer information
  // "values" : ["<peer 1 info string>", "<peer 2 info string>"]
  sp::UinStaticArray<Contact, 256> values;
  // If the queried node has no peers for the infohash
  // - "nodes" is returned containing nodes from its routing table which is
  // closest to the queried infohash "nodes" : "<compact node info>"
  sp::UinStaticArray<dht::IdContact, 256> nodes;
};

bool
parse_get_peers_request(dht::MessageContext &ctx, GetPeersRequest &out);

bool
parse_get_peers_response(dht::MessageContext &ctx, GetPeersResponse &out);

// ========================================
struct AnnouncePeerRequest {
  dht::NodeId sender;
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
  dht::Key target;

  bool n4 = false;
  bool n6 = false;
};

struct SampleInfohashesResponse {
  dht::NodeId id;
  std::uint32_t interval = 0;
  // "nodes": <nodes close to 'target'>,
  sp::UinStaticArray<std::tuple<dht::NodeId, Contact>, 128> nodes;
  std::uint32_t num = 0;
  // "samples" : <subset of stored infohashes, N × 20 bytes(string)>
  sp::UinStaticArray<dht::Infohash, 128> samples;
  std::uint32_t p = 0;
};

bool
parse_sample_infohashes_request(sp::Buffer &in, SampleInfohashesRequest &out);

bool
parse_sample_infohashes_response(sp::Buffer &in, SampleInfohashesResponse &out);

// ========================================

} // namespace krpc

#endif
