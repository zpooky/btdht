-------------------------------------------------------------------------------
# scrape
TODO sample_infohashes

```
while 1:
  needle = rand_info_hash(2^160)
  best = new_heap()
  best_in_order = new_heap()
  seed(local_routing_table, ih, /*out*/best)
  seed(local_routing_table, ih, /*out*/best_in_order)
  while b = heap_dequeue(best_in_order) and it < ???:
    nodes = get_peers(b, needle)?

    for node : nodes:
      if not dupe(best, node)
        heap_enque_eager(best, nodes)

      if not dupe(best_in_order, node) and not already_asked(best_in_order, node, needle) and last_get_peers:ed() < 24h
        heap_enque_eager(best_in_order, nodes)

  while b = heap_dequeue(best):
    ih:s = sample_infohashes(best)
    for ih : ihs:
      TODO probably get_nodes
      if cache::insert(ih):
        enqueue(info_hash_lookup_queue,ih)

```

TODO what is the rules for get_peers again?
```

TODO we need better matches not just the nodes we happen to have
- maintain multiple routing tables cuncurrently one main and multiple (rand_info_hash(2^160) that we cycle through)
while 1:
  needle = rand_info_hash(2^160)
  heap = new_heap(closest=needle)
  seed(local_routing_table, ih, last_sample_infohashes=<24h, /*out*/heap)
  while cur = heap_dequeue(heap):
    ih:s = sample_infohashes(cur)
    for ih : ihs:
      peers = get_peers(cur, ih)
      enqueue(result, (ih, peers))
```

-------------------------------------------------------------------------------
TODO
- save routing table to disk
- restore routing table from disk
- if we regenerate new self node id we reinsert to the routing table otherwise
  we just restore it as is from disk

-------------------------------------------------------------------------------
TODO
RoutingTableNode[length, Array[...]]
- no linear scans of RoutingTableNode
- if peer is remove from a RoutingTable Node the last peer in thn
  RoutingTableNode gets its place

TODO timeout queue verify the timestamp/date thing
TODO spare routing_Table
  - multiple closest
TODO routing_table heap node compare (heap entry needs to be updated when new nodes are added/removed)
TOOD do not store options in struct so that we can reduce memory

- scrape main routing_table as well

- db populate based on 1M latest
  - size of bloomfilter ...
  - query planner

-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
standard test

-------------------------------------------------------------------------------
statistics on version which returns response from sample_infohash

---------------------------------------------------------------------
retire good vector
- routing_table_will_accept(node)
- dht::insert(best_match->routing_table, node);

-------------------------------------------------------------------------------
assertion failed: (b.length >= b.pos)
../external/sputil/src/buffer/BytesView.cpp: 100

Stack trace (most recent call last):
#0    unsigned long backward::details::unwind<backward::StackTraceImpl<backward::system_tag::linux_tag>::callback>(backward::StackTraceImpl<backward::system_tag::linux_tag>::callback, unsigned long) backward.hpp:851
#1    backward::StackTraceImpl<backward::system_tag::linux_tag>::load_here(unsigned long, void*, void*) backward.hpp:869
#2    sp::impl::print_backtrace(_IO_FILE*) assert.cpp:124
#3    sp::impl::assert_func(char const*, int, char const*, char const*) assert.cpp:161
#4    sp::remaining_read(sp::IBytesView<unsigned char> const&) BytesView.cpp:100
#5    bool bencode::d::list_contact<sp::UinStaticArray<Contact, 256ul> >(sp::IBytesView<unsigned char>&, sp::UinStaticArray<Contact, 256ul>&) bencode_offset.cpp:307
#6    bencode::d::peers(sp::IBytesView<unsigned char>&, char const*, sp::UinStaticArray<Contact, 256ul>&) bencode_offset.cpp:390
#7    auto krpc::parse_get_peers_response(dht::MessageContext&, krpc::GetPeersResponse&)::{lambda(auto:1&)#1}::operator()<sp::IBytesView<unsigned char> >(sp::IBytesView<unsigned char>&) const krpc_parse.cpp:448
#8    bool bencode::d::dict<krpc::parse_get_peers_response(dht::MessageContext&, krpc::GetPeersResponse&)::{lambda(auto:1&)#1}>(sp::IBytesView<unsigned char>&, krpc::parse_get_peers_response(dht::MessageContext&, krpc::GetPeersResponse&)::{lambda(auto:1&)#1}) bencode.h:115
#9    krpc::parse_get_peers_response(dht::MessageContext&, krpc::GetPeersResponse&) krpc_parse.cpp:399
#10   get_peers::on_response(dht::MessageContext&, void*) dht_interface.cpp:768
#11   tx::TxContext::handle(dht::MessageContext&) shared.cpp:58
#12   parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1}::operator()(krpc::ParseContext&) const dht-server.cpp:155
#13   auto krpc::d::krpc<parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1}>(krpc::ParseContext&, parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1})::{lambda(auto:1&)#1}::operator()<sp::IBytesView<unsigned char> >(sp::IBytesView<unsigned char>&) const krpc.h:259
#14   bool bencode::d::dict<krpc::d::krpc<parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1}>(krpc::ParseContext&, parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1})::{lambda(auto:1&)#1}>(sp::IBytesView<unsigned char>&, krpc::d::krpc<parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1}>(krpc::ParseContext&, parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1})::{lambda(auto:1&)#1}) bencode.h:115
#15   bool krpc::d::krpc<parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1}>(krpc::ParseContext&, parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&)::{lambda(krpc::ParseContext&)#1}) krpc.h:104
#16   parse(dht::Domain, dht::DHT&, dht::Modules&, Contact const&, sp::IBytesView<unsigned char>&, sp::IBytesView<unsigned char>&) dht-server.cpp:183
#17   on_dht_protocol_handle(void*, unsigned int) dht-server.cpp:209
#18   sp::tick(epoll_event&) core.cpp:34
#19   sp::core_tick(sp::core&, sp::Milliseconds) core.cpp:66
#20   int main_loop<main::{lambda(sp::IBytesView<unsigned char>&)#1}>(dht::DHT&, main::{lambda(sp::IBytesView<unsigned char>&)#1}) dht-server.cpp:446
#21   main dht-server.cpp:588
#22   __libc_init_first
#23   __libc_start_main
#24   _start
#25

-------------------------------------------------------------------------------
