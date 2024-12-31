#include "priv_krpc.h"
// #include "bencode.h"
#include "cache.h"
#include "encode_bencode.h"
#include "krpc_shared.h"
#include "util.h"
#include <tree/bst_extra.h>
#include <udp.h>
#include <unistd.h>

namespace krpc {
namespace priv {

//=====================================
bool
request::dump(sp::Buffer &b, const krpc::Transaction &t) noexcept {
  return req(b, t, "sp_dump", [](sp::Buffer &) { //
    return true;
  });
} // request::dump()

//=====================================
bool
request::dump_scrape(sp::Buffer &b, const krpc::Transaction &t) noexcept {
  return req(b, t, "sp_dump_scrape", [](sp::Buffer &) { //
    return true;
  });
} // request::scrape()

//=====================================
bool
request::dump_db(sp::Buffer &b, const krpc::Transaction &t) noexcept {
  return req(b, t, "sp_dump_db", [](sp::Buffer &) { //
    return true;
  });
} // request::db()

//=====================================
bool
request::statistics(sp::Buffer &b, const krpc::Transaction &t) noexcept {
  return req(b, t, "sp_statistics", [](sp::Buffer &) { //
    return true;
  });
}

//=====================================
bool
request::search(sp::Buffer &b, const krpc::Transaction &t,
                const dht::Infohash &search, std::size_t timeout) noexcept {
  return req(b, t, "sp_search", [&search, &timeout](sp::Buffer &buffer) {
    /**/
    if (!bencode::e::pair(buffer, "search", search.id)) {
      return false;
    }
    if (!bencode::e::pair(buffer, "timeout", timeout)) {
      return false;
    }
    return true;
  });
}

bool
request::stop_search(sp::Buffer &b, const krpc::Transaction &t,
                     const dht::Infohash &search) noexcept {
  return req(b, t, "sp_search_stop", [&search](sp::Buffer &buffer) {
    /**/
    if (!bencode::e::pair(buffer, "search", search.id)) {
      return false;
    }
    return true;
  });
}

//=====================================
bool
response::dump(sp::Buffer &buf, const Transaction &t,
               const dht::DHT &dht) noexcept {
  return resp(buf, t, [&dht](auto &b) {
    if (!bencode::e::pair(b, "id", dht.id.id, sizeof(dht.id.id))) {
      fprintf(stdout, "%s: 1\n", __func__);
      return false;
    }

    uint64_t pid = (uint64_t)getpid();
    if (!bencode::e::pair(b, "pid", pid)) {
      fprintf(stdout, "%s: 2\n", __func__);
      return false;
    }
    Contact bind;
    net::local(dht.client.udp, bind);
    if (!bencode::e::pair(b, "bind_port", bind.port)) {
      fprintf(stdout, "%s: 3\n", __func__);
      return false;
    }

    if (!bencode::e::value(b, "cache")) {
      fprintf(stdout, "%s: 4\n", __func__);
      return false;
    }

    bool res = bencode::e::dict(b, [&dht](auto &b2) {
      if (!bencode::e::pair(b2, "min_read_idx", sp::cache_read_min_idx(dht))) {
        fprintf(stdout, "%s: 4\n", __func__);
        return false;
      }

      if (!bencode::e::pair(b2, "max_read_idx", sp::cache_read_max_idx(dht))) {
        fprintf(stdout, "%s: 5\n", __func__);
        return false;
      }

      if (!bencode::e::pair(b2, "contacts", sp::cache_contacts(dht))) {
        fprintf(stdout, "%s: 6\n", __func__);
        return false;
      }

      if (!bencode::e::pair(b2, "write_idx", sp::cache_write_idx(dht))) {
        fprintf(stdout, "%s: 7\n", __func__);
        return false;
      }

      return true;
    });
    if (!res) {
      return false;
    }

    if (!bencode::e::value(b, "bootstrap")) {
      fprintf(stdout, "%s: 8\n", __func__);
      return false;
    }

    res = bencode::e::dict(b, [&dht](auto &b2) {
      if (!bencode::e::pair(
              b2, "unique_inserts",
              dht.bootstrap_meta.bootstrap_filter.unique_inserts)) {
        fprintf(stdout, "%s: 9\n", __func__);
        return false;
      }
      if (!bencode::e::pair(b2, "candidates", length(dht.bootstrap))) {
        fprintf(stdout, "%s: 10\n", __func__);
        return false;
      }

      return true;
    });
    if (!res) {
      return false;
    }

    if (!bencode::e::value(b, "spbt")) {
      return false;
    }
    res = bencode::e::dict(b, [&dht](auto &b2) {
      if (!bencode::e::pair(b2, "stored",
                            dht.db.scrape_client.cache.unique_inserts)) {
        fprintf(stdout, "%s: 28\n", __func__);
        return false;
      }
      return true;
    });

    if (!res) {
      return false;
    }

    if (!bencode::e::value(b, "ip_election")) {
      fprintf(stdout, "%s: 29\n", __func__);
      return false;
    }

    res = bencode::e::dict(b, [&dht](auto &b2) {
      return for_all(dht.election.table, [&b2](const auto &e) {
        Contact c = std::get<0>(e);
        std::size_t votes = std::get<1>(e);
        char str[64] = {0};
        if (!to_string(c, str)) {
          assertx(false);
        }

        return bencode::e::pair(b2, str, votes);
      });
    });

    if (!res) {
      fprintf(stdout, "%s: 29\n", __func__);
      return false;
    }

    if (!bencode::e::pair(b, "root", dht.routing_table.root)) {
      fprintf(stdout, "%s: 30\n", __func__);
      return false;
    }
    std::uint64_t la(dht.last_activity);
    if (!bencode::e::pair(b, "last_activity", la)) {
      fprintf(stdout, "%s: 31\n", __func__);
      return false;
    }
    if (!bencode::e::pair(b, "routing_table_nodes",
                          dht.routing_table.total_nodes)) {
      fprintf(stdout, "%s: 32\n", __func__);
      return false;
    }
    if (!bencode::e::pair(b, "bad_nodes", dht.routing_table.bad_nodes)) {
      fprintf(stdout, "%s: 33\n", __func__);
      return false;
    }
    return true;
  });
} // response::dump()

bool
response::dump_scrape(sp::Buffer &buf, const Transaction &t,
                 const dht::DHT &dht) noexcept {
  return resp(buf, t, [&dht](auto &b) {
    bool res = true;
    if (!bencode::e::value(b, "scrape")) {
      fprintf(stdout, "%s: 11\n", __func__);
      return false;
    }

    res = bencode::e::dict(b, [&dht](auto &b2) {
      if (!bencode::e::pair(b2, "active_scrapes", length(dht.active_scrapes))) {
        fprintf(stdout, "%s: 12\n", __func__);
        return false;
      }

      if (!bencode::e::pair(b2, "active_sample_infohashes_requests",
                            dht.scrape_active_sample_infhohash)) {
        fprintf(stdout, "%s: 13\n", __func__);
        return false;
      }

      if (!bencode::e::pair(b2, "queue_get_peers_ih",
                            length(dht.scrape_get_peers_ih))) {
        fprintf(stdout, "%s: 14\n", __func__);
        return false;
      }

      std::size_t i = 0;
      for (const auto scrape : dht.active_scrapes) {
        char key[64] = {0};
        sprintf(key, "info_hash%zu", i);
        if (!bencode::e::pair(b2, key, scrape->routing_table.id.id)) {
          fprintf(stdout, "%s: 15 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        sprintf(key, "routing_table_nodes%zu", i);
        if (!bencode::e::pair(b2, key, scrape->routing_table.total_nodes)) {
          fprintf(stdout, "%s: 16 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        sprintf(key, "candidates%zu", i);
        if (!bencode::e::pair(b2, key, length(scrape->bootstrap))) {
          fprintf(stdout, "%s: 17 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        sprintf(key, "stat.publish%zu", i);
        if (!bencode::e::pair(b2, key, scrape->stat.publish)) {
          fprintf(stdout, "%s: 18 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        sprintf(key, "stat.sample_ih%zu", i);
        if (!bencode::e::pair(b2, key, scrape->stat.sent_sample_infohash)) {
          fprintf(stdout, "%s: 19 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        sprintf(key, "stat.response-get_peers%zu", i);
        if (!bencode::e::pair(b2, key, scrape->stat.get_peer_responses)) {
          fprintf(stdout, "%s: 20 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        sprintf(key, "stat.response-new-get_peers%zu", i);
        if (!bencode::e::pair(b2, key, scrape->stat.new_get_peer)) {
          fprintf(stdout, "%s: 21 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        sprintf(key, "approx-upcoming_sample_ih%zu", i);
        if (!bencode::e::pair(b2, key, scrape->upcoming_sample_infohashes)) {
          fprintf(stdout, "%s: 21 (%zu) rw(%zu)\n", __func__, i,
                  remaining_write(b2));
          return false;
        }
        ++i;
      }
      if (!bencode::e::pair(
              b2, "bootstrap_unique_inserts",
              dht.scrape_bootstrap_filter.bootstrap_filter.unique_inserts)) {
        fprintf(stdout, "%s: 22\n", __func__);
        return false;
      }

      if (!bencode::e::pair(b2, "scrape_hour_current_idx",
                            dht.scrape_hour_idx)) {
        fprintf(stdout, "%s: 23\n", __func__);
        return false;
      }

      i = 0;
      for (const auto &sh : dht.scrape_hour) {
        char key[64] = {0};
        sprintf(key, "scrape_hour%zu", i);
        if (!bencode::e::pair(b2, key, sh.unique_inserts)) {
          fprintf(stdout, "%s: 24\n", __func__);
          return false;
        }
        ++i;
      }

      return true;
    });
    return res;
  });
}

bool
response::dump_db(sp::Buffer &buf, const Transaction &t,
             const dht::DHT &dht) noexcept {
  return resp(buf, t, [&dht](auto &b) {
    bool res = true;
    if (!bencode::e::value(b, "db")) {
      fprintf(stdout, "%s: 25\n", __func__);
      return false;
    }

    res = bencode::e::dict(b, [&dht](auto &b2) {
      binary::rec::inorder(dht.db.lookup_table,
                           [&b2, &dht](dht::KeyValue &e) -> bool {
                             return bencode::e::dict(b2, [&e, &dht](auto &b3) {
                               char buffer[64]{0};
                               assertx_n(to_string(e.id, buffer));

                               if (!bencode::e::pair(b3, "infohash", buffer)) {
                                 fprintf(stdout, "%s: 26\n", __func__);
                                 return false;
                               }
#if 0
              if (!bencode::e::pair(b3, "rank",
                                    dht::rank(dht.id.id, e.id.id))) {
                fprintf(stdout, "%s: 27\n", __func__);
                return false;
              }
#endif

                               std::uint64_t l(sp::n::length(e.peers));
                               if (!bencode::e::pair(b3, "entries", l)) {
                                 fprintf(stdout, "%s: 28\n", __func__);
                                 return false;
                               }

                               if (e.name) {
                                 if (!bencode::e::pair(b3, "name", e.name)) {
                                   fprintf(stdout, "%s: 29\n", __func__);
                                   return false;
                                 }
                               }

                               return true;
                             });
                           });
      return true;
    });
    return res;
  });
}

//=====================================
static bool
value(sp::Buffer &buf, const Contact &c) noexcept {
  return bencode::e::dict(buf, [&c](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "ipv4", c.ip.ipv4)) {
      return false;
    }
    if (!bencode::e::pair(b, "port", c.port)) {
      return false;
    }

    return true;
  });
}

static bool
value(sp::Buffer &buf, const dht::Node &node) noexcept {
  return bencode::e::dict(buf, [&node](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", node.id.id, sizeof(node.id.id))) {
      return false;
    }

    if (!bencode::e::pair(b, "good", node.properties.is_good)) {
      return false;
    }

    if (!bencode::e::pair(b, "outstanding", node.outstanding)) {
      return false;
    }

    return true;
  });
}

static bool
value(sp::Buffer &buf, const dht::Bucket &t) noexcept {
  return bencode::e::list(buf, (void *)&t, [](sp::Buffer &b, void *arg) { //
    auto *a = (dht::Bucket *)arg;

    return for_all(*a, [&b](const dht::Node &node) { //
      return value(b, node);
    });
  });
}

static bool
value(sp::Buffer &buf, const dht::RoutingTable &t) noexcept {
  // used by dump

  return bencode::e::dict(buf, [&t](sp::Buffer &b) {
    // dht::Infohash id; // TODO
    // if (!pair(b, "id", id.id, sizeof(id.id))) {
    //   return false;
    // }

    if (!bencode::e::value(b, "bucket")) {
      return false;
    }

    if (!value(b, t.bucket)) {
      return false;
    }

    return true;
  });
} // bencode::e::value()

#if 0
static bool
pair(sp::Buffer &buf, const char *key, const dht::RoutingTable *t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return bencode::e::list(buf, (void *)&t, [](sp::Buffer &b, void *arg) { //
    const auto *a = (const dht::RoutingTable *)arg;

    return dht::for_all(a, [&b](const dht::RoutingTable &p) { //
      return value(b, p);
    });
  });
} // bencode::e::pair()


static bool
value(sp::Buffer &buf, const dht::Peer &t) noexcept {
  // used by dump

  return bencode::e::dict(buf, [&t](sp::Buffer &b) { //
    if (!bencode::e::value(b, "contact")) {
      return false;
    }
    if (!bencode::e::value(b, t.contact)) {
      return false;
    }

    if (!bencode::e::pair(b, "activity", std::uint64_t(t.activity))) {
      return false;
    }

    return true;
  });
} // bencode::e::value()


static bool
value(sp::Buffer &buf, const dht::KeyValue &t) noexcept {
  // used by dump
  return bencode::e::dict(buf, [&t](sp::Buffer &b) { //
    if (!bencode::e::pair(b, "id", t.id.id, sizeof(t.id.id))) {
      return false;
    }

    if (!bencode::e::value(b, "list")) {
      return false;
    }

    return bencode::e::list(b, (void *)&t, [](sp::Buffer &b2, void *arg) { //
      const auto *a = (const dht::KeyValue *)arg;

      return for_all(a->peers, [&b2](const dht::Peer &p) { //
        return value(b2, p);
      });
    });
  });
} // bencode::e::value()

static bool
pair(sp::Buffer &buf, const char *key, const dht::KeyValue *t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return bencode::e::list(buf, (void *)t, [](sp::Buffer &b, void *arg) {
    const auto *a = (const dht::KeyValue *)arg;
    return for_all(a, [&b](const dht::KeyValue &it) { //
      return value(b, it);
    });
  });
} // bencode::e::pair()

#endif

static bool
serialize(sp::Buffer &b, const Contact &p) noexcept {
  // TODO ipv4
  assertx(p.ip.type == IpType::IPV4);
  const std::size_t pos = b.pos;

  Ipv4 ip = htonl(p.ip.ipv4);
  if (sp::remaining_write(b) < sizeof(ip)) {
    b.pos = pos;
    return false;
  }

  std::memcpy(b.raw + b.pos, &ip, sizeof(ip));
  b.pos += sizeof(ip);

  Port port = htons(p.port);
  if (sp::remaining_write(b) < sizeof(port)) {
    b.pos = pos;
    return false;
  }
  std::memcpy(b.raw + b.pos, &port, sizeof(port));
  b.pos += sizeof(port);

  return true;
} // bencode::e::serialize()

static bool
serialize(sp::Buffer &b, const dht::KContact &p) noexcept {
  return serialize(b, p.contact);
}

static bool
serialize(sp::Buffer &b, const dht::Node &node) noexcept {
  const std::size_t pos = b.pos;

  if (sp::remaining_write(b) < sizeof(node.id.id)) {
    b.pos = pos;
    return false;
  }
  std::memcpy(b.raw + b.pos, node.id.id, sizeof(node.id.id));
  b.pos += sizeof(node.id.id);

  if (!serialize(b, node.contact)) {
    return false;
  }

  return true;
} // bencode::e::serialize()

static std::size_t
serialize_size(const dht::KContact &p) noexcept {
  return serialize_size(p.contact);
}

static std::size_t
serialize_size(const dht::Node &p) noexcept {
  return sizeof(p.id.id) + serialize_size(p.contact);
}

static std::size_t
serialize_size(const dht::Node **list, std::size_t length) noexcept {
  std::size_t result = 0;
  for_all(list, length, [&result](const auto &value) { //
    result += serialize_size(value);
    return true;
  });
  return result;
}

template <typename T>
static std::size_t
serialize_size(const sp::list<T> &list) noexcept {
  std::size_t result = 0;
  sp::for_each(list, [&result](const T &ls) { //
    result += serialize_size(ls);
  });
  return result;
}

template <typename T>
static std::size_t
serialize_size(const sp::dstack<T> &list) noexcept {
  std::size_t result = 0;
  sp::for_each(list, [&result](const T &ls) { //
    result += serialize_size(ls);
  });
  return result;
}

template <typename T>
static std::size_t
serialize_size(const heap::MaxBinary<T> &list) noexcept {
  std::size_t result = 0;
  for_each(list, [&result](const auto &ls) {
    //
    result += serialize_size(ls);
  });
  return result;
}

template <typename List>
static bool
sp_list(sp::Buffer &buf, const List &list) noexcept {
  std::size_t sz = serialize_size(list);

  return bencode::e::value(buf, sz, (void *)&list, [](sp::Buffer &b, void *a) {
    const List *l = (List *)a;
    assertx(l);
    return for_all(*l, [&b](const auto &value) {
      //
      return serialize(b, value);
    });
  });
}

template <typename T>
static bool
internal_pair(sp::Buffer &buf, const char *key,
              const sp::list<T> &list) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return sp_list(buf, list);
} // bencode::e::internal_pair()

static bool
pair_compact(sp::Buffer &buf, const char *key,
             const sp::list<dht::Node> &list) noexcept {
  return internal_pair(buf, key, list);
} // bencode::e::pair()

static bool
pair_compact(sp::Buffer &buf, const char *key, const dht::Node **list,
             std::size_t sz) noexcept {
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  std::size_t len = serialize_size(list, sz);
  std::tuple<const dht::Node **, std::size_t> arg(list, sz);

  return bencode::e::value(buf, len, &arg, [](auto &b, void *a) {
    auto targ = (std::tuple<const dht::Node **, std::size_t> *)a;

    auto cb = [&b](const auto &value) {
      if (!serialize(b, value)) {
        return false;
      }

      return true;
    };

    return for_all(std::get<0>(*targ), std::get<1>(*targ), cb);
  });
}

static bool
value(sp::Buffer &buf, const sp::list<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

static bool
value(sp::Buffer &buf, const sp::dstack<Contact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

static bool
value(sp::Buffer &buf, const heap::MaxBinary<dht::KContact> &t) noexcept {
  // used by dump
  return sp_list(buf, t);
} // bencode::e::value()

static bool
pair(sp::Buffer &buf, const char *key, const sp::list<Contact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
} // bencode::e::pair()

static bool
pair(sp::Buffer &buf, const char *key, const sp::dstack<Contact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
}

static bool
pair(sp::Buffer &buf, const char *key,
     const heap::MaxBinary<dht::KContact> &t) noexcept {
  // used by dump
  if (!bencode::e::value(buf, key)) {
    return false;
  }

  return value(buf, t);
}

static bool
pair(sp::Buffer &buf, const char *key, const dht::StatTrafic &t) noexcept {
  // used by statistics
  // if (!bencode::e::value(buf, key)) {
  //   return false;
  // }
  // return bencode::e::dict(buf, [&t](sp::Buffer &b) {
  char skey[64] = {0};
  sprintf(skey, "%s-ping", key);
  if (!bencode::e::pair(buf, skey, t.ping)) {
    return false;
  }
  sprintf(skey, "%s-find_node", key);
  if (!bencode::e::pair(buf, skey, t.find_node)) {
    return false;
  }
  sprintf(skey, "%s-get_peers", key);
  if (!bencode::e::pair(buf, skey, t.get_peers)) {
    return false;
  }
  sprintf(skey, "%s-announce_peer", key);
  if (!bencode::e::pair(buf, skey, t.announce_peer)) {
    return false;
  }
  sprintf(skey, "%s-sample_infohashes", key);
  if (!bencode::e::pair(buf, skey, t.sample_infohashes)) {
    return false;
  }
  sprintf(skey, "%s-error", key);
  if (!bencode::e::pair(buf, skey, t.error)) {
    return false;
  }

  return true;
  // });
}

static bool
pair(sp::Buffer &buf, const char *key, const dht::StatDirection &d,
     bool sent) noexcept {
  // used by statistics
  // if (!bencode::e::value(buf, key)) {
  //   return false;
  // }
  //
  // return bencode::e::dict(buf, [&d](sp::Buffer &b) {

  char skey[64] = {0};
  sprintf(skey, "%s-request", key);
  if (!pair(buf, skey, d.request)) {
    return false;
  }

  if (!sent) {
    sprintf(skey, "%s-response_timeout", key);
    if (!pair(buf, skey, d.response_timeout)) {
      return false;
    }
  }

  sprintf(skey, "%s-response", key);
  if (!pair(buf, skey, d.response)) {
    return false;
  }

  sprintf(skey, "%s-parse_error", key);
  if (!bencode::e::pair(buf, skey, d.parse_error)) {
    return false;
  }

  return true;
  // });
}

bool
response::statistics(sp::Buffer &buf, const Transaction &t,
                     const dht::Stat &stat) noexcept {
  return resp(buf, t, [&stat](auto &b) { //
    if (!pair(b, "transmit", stat.transmit, true)) {
      return false;
    }
    if (!pair(b, "received", stat.received, false)) {
      return false;
    }
    if (!bencode::e::pair(b, "known_tx", stat.known_tx)) {
      return false;
    }
    if (!bencode::e::pair(b, "unknown_tx", stat.unknown_tx)) {
      return false;
    }
    if (!bencode::e::pair(b, "scrape_swapped_ih", stat.scrape_swapped_ih)) {
      return false;
    }
    return true;
  });
}

//=====================================
bool
response::search(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

bool
response::search_stop(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

bool
response::announce_this(sp::Buffer &b, const Transaction &t) noexcept {
  return resp(b, t, [](auto &) { //
    return true;
  });
}

//=====================================
namespace event {
template <typename F>
static bool
event(sp::Buffer &buf, F f) noexcept {
  Transaction t;
  t.length = 4;
  std::memcpy(t.id, "12ab", t.length);

  // return message(buf, t, "q", "found", [&f](auto &b) { //
  //   if (!bencode::e::value(b, "e")) {
  //     return false;
  //   }
  //
  //   return bencode::e::dict(b, [&f](auto &b2) { //
  //     return f(b2);
  //   });
  // });

  return message(buf, t, "q", "sp_found", [&f](auto &b) { //
    if (!bencode::e::value(b, "a")) {
      return false;
    }

    return bencode::e::dict(b, [&f](auto &b2) { //
      return f(b2);
    });
  });
} // krpc::resp()

template <typename Contacts>
bool
found(sp::Buffer &buf, const dht::Infohash &search,
      const Contacts &contacts) noexcept {
  assertx(!is_empty(contacts));

  return event(buf, [&contacts, &search](auto &b) {
    if (!bencode::e::pair(b, "id", search.id, sizeof(search.id))) {
      return false;
    }

    if (!bencode::e::value(b, "contacts")) {
      return false;
    }

    auto cb = [](sp::Buffer &b2, void *a) {
      Contacts *cx = (Contacts *)a;

      return for_all(*cx, [&](auto &c) {
        //
        return value(b2, c);
      });
    };

    return bencode::e::list(b, (void *)&contacts, cb);
  });
}

template bool
found<sp::LinkedList<Contact>>(sp::Buffer &, const dht::Infohash &,
                               const sp::LinkedList<Contact> &) noexcept;
} // namespace event

} // namespace priv
} // namespace krpc
