#define __STDC_FORMAT_MACROS
#include "krpc_parse.h"
#include "Log.h"
#include "bencode_offset.h"
#include "decode_bencode.h"

#include <inttypes.h>
#include <string/ascii.h>

// ========================================
static void
print_raw(FILE *f, const char *val, std::size_t len) noexcept {
  if (ascii::is_printable(val, len)) {
    fprintf(f, "'%.*s': %zu", int(len), val, len);
  } else {
    fprintf(f, "hex[");
    dht::print_hex(f, (const sp::byte *)val, len);
    fprintf(f, "](");
    for (std::size_t i = 0; i < len; ++i) {
      if (ascii::is_printable(val[i])) {
        fprintf(f, "%c", val[i]);
      } else {
        fprintf(f, "_");
      }
    }
    fprintf(f, ")");
  }
}

static bool
bencode_any(sp::Buffer &p, const char *ctx) noexcept {
  FILE *f = stderr;
  /*any str*/ {
    const char *kit = nullptr;
    std::size_t klen = 0;

    const unsigned char *vit = nullptr;
    std::size_t vlen = 0;

    if (bencode::d::pair_ref(p, kit, klen, vit, vlen)) {
      fprintf(f, "%s str[", ctx);
      print_raw(f, kit, klen);
      fprintf(f, ", ");
      print_raw(f, (const char *)vit, vlen);
      fprintf(f, "] \n");
      return true;
    }
  }

  /*any int*/ {
    const char *kit = nullptr;
    std::size_t klen = 0;

    std::uint64_t value = 0;

    if (bencode::d::pair_ref(p, kit, klen, value)) {
      fprintf(f, "%s int[", ctx);
      print_raw(f, kit, klen);
      fprintf(f, ", %" PRIu64 "]\n", value);
      return true;
    }
  }

  /*any list*/ {
    const size_t pos = p.pos;
    const char *kit = nullptr;
    std::size_t klen = 0;

    if (bencode::d::value_ref(p, kit, klen)) {
      bool first = true;

      auto cb = [&](sp::Buffer &p2) { //
        fprintf(f, "%s list[", ctx);
        print_raw(f, kit, klen);
        fprintf(f, "\n");
        first = false;

        while (true) {
          const char *vit = nullptr;
          std::size_t vlen = 0;
          std::uint64_t value = 0;

          if (bencode::d::value_ref(p2, vit, vlen)) {
            fprintf(f, "- ");
            print_raw(f, (const char *)vit, vlen);
            fprintf(f, "\n");
          } else if (bencode::d::value(p2, value)) {
            fprintf(f, "- ");
            fprintf(f, ", %" PRIu64 "]\n", value);
          } else {
            break;
          }
        }

        return true;
      };

      if (bencode::d::list(p, cb)) {
        fprintf(f, "]\n");
        return true;
      }

      fprintf(f, "spooky[error]\n");
      p.pos = pos;
    }
  }

  return false;
}

// ========================================
bool
krpc::parse_ping_request(dht::MessageContext &ctx, krpc::PingRequest &out) {
  // sp::Buffer &d
  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) { //
    bool b_id = false;
    bool b_ip = false;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", out.sender.id)) {
      b_id = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assertx(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    if (bencode_any(p, "ping request")) {
      goto Lstart;
    }

    if (b_id) {
      return true;
    }

    logger::receive::parse::error(ctx.dht, p, "'ping' request missing 'id'");
    return false;
  });
}

bool
krpc::parse_ping_response(dht::MessageContext &ctx, krpc::PingResponse &out) {

  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) {
    bool b_id = false;
    bool b_ip = false;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", out.sender.id)) {
      b_id = true;
      goto Lstart;
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assertx(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      }
    }

    if (bencode_any(p, "ping resp")) {
      goto Lstart;
    }

    if (b_id) {
      return true;
    }

    logger::receive::parse::error(ctx.dht, p, "'ping' response missing 'id'");
    return false;
  });
}

// ========================================
bool
krpc::parse_find_node_request(dht::MessageContext &ctx,
                              krpc::FindNodeRequest &out) {

  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) { //
    bool b_id = false;
    bool b_t = false;
    bool b_want = false;

    sp::UinStaticArray<std::string, 2> want;
  Lstart:
    if (!b_id && bencode::d::pair(p, "id", out.id.id)) {
      b_id = true;
      goto Lstart;
    }
    if (!b_t && bencode::d::pair(p, "target", out.target.id)) {
      b_t = true;
      goto Lstart;
    }

    if (!b_want && bencode_d<sp::Buffer>::pair(p, "want", want)) {
      b_want = true;
      for (std::string &w : want) {
        if (w == "n4") {
          out.n4 = true;
        } else if (w == "n6") {
          out.n6 = true;
        }
      }
      goto Lstart;
    }

    if (bencode_any(p, "find_node req")) {
      goto Lstart;
    }

    if (!(b_id && b_t)) {
      const char *msg = "'find_node' request missing 'id' or 'target'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    if (!b_want) {
      out.n4 = true;
    }

    return true;
  });
}

bool
krpc::parse_find_node_response(dht::MessageContext &ctx,
                               krpc::FindNodeResponse &out) {
  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) { //
    bool b_id = false;
    bool b_n = false;
    bool b_p = false;
    bool b_ip = false;
    bool b_t = false;

    std::uint64_t p_param = 0; // TODO

  Lstart:
    const std::size_t pos = p.pos;
    if (!b_id && bencode::d::pair(p, "id", out.id.id)) {
      b_id = true;
      goto Lstart;
    } else {
      assertx(p.pos == pos);
    }

    // optional
    if (!b_n) {
      clear(out.nodes);
      // TODO we parse a node which get 0 as port
      // - ipv4 = 1148492139,
      if (bencode::d::nodes(p, "nodes", out.nodes)) {
        b_n = true;
        goto Lstart;
      } else {
        assertx(p.pos == pos);
      }
    }

    if (!b_t && bencode::d::pair(p, "token", out.token)) {
      b_t = true;
      goto Lstart;
    } else {
      assertx(p.pos == pos);
    }

    // optional
    if (!b_p && bencode::d::pair(p, "p", p_param)) {
      b_p = true;
      goto Lstart;
    } else {
      assertx(p.pos == pos);
    }

    {
      Contact ip;
      if (!b_ip && bencode::d::pair(p, "ip", ip)) {
        ctx.ip_vote = ip;
        assertx(bool(ctx.ip_vote));
        b_ip = true;
        goto Lstart;
      } else {
        assertx(p.pos == pos);
      }
    }

    if (bencode_any(p, "find_node resp")) {
      goto Lstart;
    }

    if (!(b_id)) {
      const char *msg = "'find_node' response missing 'id'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    return true;
  });
}

// ========================================
bool
krpc::parse_get_peers_request(dht::MessageContext &ctx,
                              krpc::GetPeersRequest &out) {
  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) {
    bool b_id = false;
    bool b_ih = false;
    bool b_ns = false;
    bool b_sc = false;
    bool b_want = false;

    sp::UinStaticArray<std::string, 2> want;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", out.id.id)) {
      b_id = true;
      goto Lstart;
    }

    if (!b_ih && bencode::d::pair(p, "info_hash", out.infohash.id)) {
      b_ih = true;
      goto Lstart;
    }

    bool tmp_noseed = false;
    if (!b_ns && bencode::d::pair(p, "noseed", tmp_noseed)) {
      b_ns = true;
      out.noseed = tmp_noseed;
      goto Lstart;
    }

    if (!b_sc && bencode::d::pair(p, "scrape", out.scrape)) {
      b_sc = true;
      goto Lstart;
    }

    if (!b_want && bencode_d<sp::Buffer>::pair(p, "want", want)) {
      b_want = true;
      for (std::string &w : want) {
        if (w == "n4") {
          out.n4 = true;
        } else if (w == "n6") {
          out.n6 = true;
        }
      }
      goto Lstart;
    }

    if (bencode_any(p, "get_peers req")) {
      goto Lstart;
    }

    if (!(b_id && b_ih)) {
      const char *msg = "'get_peers' request missing 'id' or 'info_hash'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    if (!b_want) {
      // default:
      out.n4 = true;
    }

    return true;
  });
}

bool
krpc::parse_get_peers_response(dht::MessageContext &ctx,
                               krpc::GetPeersResponse &out) {
  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) { //
    bool b_id = false;
    bool b_t = false;
    bool b_n = false;
    bool b_v = false;
    // bool b_ip = false;

  Lstart:
    const std::size_t pos = p.pos;
    if (!b_id && bencode::d::pair(p, "id", out.id.id)) {
      b_id = true;
      goto Lstart;
    } else {
      assertx(pos == p.pos);
    }

    if (!b_t && bencode::d::pair(p, "token", out.token)) {
      b_t = true;
      goto Lstart;
    } else {
      assertx(pos == p.pos);
    }

    // XXX
    // {
    //   Contact ip;
    //   if (!b_ip && bencode::d::pair(p, "ip", ip)) {
    //     ctx.ip_vote = ip;
    //     assertx(bool(ctx.ip_vote));
    //     b_ip = true;
    //     goto Lstart;
    //   } else {
    //     assertx(pos == p.pos);
    //   }
    // }

    /*closes K nodes*/
    if (!b_n) {
      clear(out.nodes);
      if (bencode::d::nodes(p, "nodes", out.nodes)) {
        b_n = true;
        goto Lstart;
      } else {
        assertx(pos == p.pos);
      }
    }

    if (!b_v) {
      clear(out.values);
      if (bencode::d::peers(p, "values", out.values)) {
        b_v = true;
        goto Lstart;
      } else {
        assertx(pos == p.pos);
      }
    }

    if (bencode_any(p, "get_peers resp")) {
      goto Lstart;
    }

    if (b_id && b_t && (b_n || b_v)) {
      return true;
    }

    const char *msg = "'get_peers' response missing 'id' and 'token' or "
                      "('nodes' or 'values')";
    logger::receive::parse::error(ctx.dht, p, msg);
    return false;
  });
}

// ========================================
bool
krpc::parse_announce_peer_request(dht::MessageContext &ctx,
                                  krpc::AnnouncePeerRequest &out) {
  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) {
    bool b_id = false;
    bool b_ip = false;
    bool b_ih = false;
    bool b_p = false;
    bool b_t = false;
    bool b_s = false;
    bool b_n = false;

  Lstart:
    if (!b_id && bencode::d::pair(p, "id", out.id.id)) {
      b_id = true;
      goto Lstart;
    }
    // optional
    if (!b_ip && bencode::d::pair(p, "implied_port", out.implied_port)) {
      b_ip = true;
      goto Lstart;
    }
    if (!b_ih && bencode::d::pair(p, "info_hash", out.infohash.id)) {
      b_ih = true;
      goto Lstart;
    }
    if (!b_p && bencode::d::pair(p, "port", out.port)) {
      b_p = true;
      goto Lstart;
    }
    if (!b_t && bencode::d::pair(p, "token", out.token)) {
      b_t = true;
      goto Lstart;
    }
    if (!b_s && bencode::d::pair(p, "seed", out.seed)) {
      b_s = true;
      goto Lstart;
    }

    if (!b_n && bencode::d::pair_value_ref(p, "name", out.name, out.name_len)) {
      b_n = true;
      goto Lstart;
    }

    if (bencode_any(p, "announce_peer req")) {
      goto Lstart;
    }

    if (!(b_id && b_ih && b_t)) {
      const char *msg =
          "'announce_peer' request missing 'id' or 'info_hash' or 'token'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    return true;
  });
}

bool
krpc::parse_announce_peer_response(dht::MessageContext &ctx,
                                   krpc::AnnouncePeerResponse &out) {
  return bencode::d::dict(ctx.in, [&ctx, &out](auto &p) { //
    bool b_id = false;

  Lstart:
    if (bencode::d::pair(p, "id", out.id.id)) {
      b_id = true;
      goto Lstart;
    }

    if (bencode_any(p, "announce_peer resp")) {
      goto Lstart;
    }

    if (!(b_id)) {
      const char *msg = "'announce_peer' response missing 'id'";
      logger::receive::parse::error(ctx.dht, p, msg);
      return false;
    }

    return true;
  });
}

// ========================================
