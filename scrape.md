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
standard test

-------------------------------------------------------------------------------
statistics on version which returns response from sample_infohash

---------------------------------------------------------------------
retire good vector
- routing_table_will_accept(node)
- dht::insert(best_match->routing_table, node);

-------------------------------------------------------------------------------
find_node resp str[[len:7]hex[6E6F6465730076](nodes_v),[len:208] hex[9986C328B638DC64A1E0F04CC3294306BD06904025AA46F258E59986CA892320658F92FF1D2A7A9ABB1D6968772DC9FD82878C2D9986D0C8B79B754CD31EBA0CCD38ECFE7E8063675B785C7A558B9986D17B3E63574069B337749517901DBF6B5B8E051D163A556099876A2C538EFDBC058B09CB815C0AB543DC77E07DE7194F3F47998417782FEA4229FE17033555B6FB4D1C96FC0601B01F32FD4F9985BF84887205CE9DA073D5CD7FCD240B83C4733AB048755E0C99806C80B96AEC8A4F56C19756355B18A46225A3BBF97C2295C7](___(_8_d___L_)C____@%_F_X_____# e____*z___ihw-_____-______uL_____8__~_cg[x\zU____{>cW@i_7t_____k[____:U`__j,S________\__C_w_}__O?G___x/_B)___5U__M_______2_O_____r____s____$___s:_Hu^___l__j__OV__V5[__b%___|"__)]

-------------------------------------------------------------------------------
2024-12-09 17:30:07|parse error|Invalid krpc 2|
d
 1:r
 d
  2:id
  20:hex[BF34AEC24E86FBA66E9564A4FF34D7AD74C3F861]: 20(_4__N___n_d__4__t__a)
 e
 1:t
 4:hex[6A625019]: 4(jbP_)
 1:v
 4:TK00
 1:y
 1:r
 2:ip
 6:hex[BC97E9162AF8]: 6(____*_)
 5:nodes
 l
  26:hex[6BE44E3E49E0F5E3BFA1C2D67C62D000969FBC1FBCE6E7BABA82]: 26(k_N>I_______|b____________)
  26:hex[69619813EFD0BC7B242C02E28585BE95FCB2C11992D44F306927]: 26(ia_____{$,____________O0i')
  26:hex[6924A099245E0D1C06B747DEB3124DC843BB8B0C6DB65E96680F]: 26(i$__$^____G___M_C___m_^_h_)
  26:hex[57BF488DC9C34426D306A8F71EF25F8BB3A5B7BC5D67776DFCFE]: 26(W_H___D&____________]gwm__)
  26:hex[276FB00E38FA148DD645065795A8C34FD98A1CF3598E0838DA94]: 26('o__8____E_W___O____Y__8__)
  26:hex[231661D6AE529049F1F1BBE9EBB3A6DB3C870CE159D40B877EA4]: 26(#_a__R_I________<___Y___~_)
  26:hex[1AD22FFE3A3A513AA6DBFBCF684CC55E35BF24BD563D1A5D86C4]: 26(__/_::Q:____hL_^5_$_V=_]__)
  26:hex[18DF2849F1F1BBE9EBB3A6DB3C870C3E99245E52BCE696CDEABF]: 26(__(I________<__>_$^R______)
 e
e

-------------------------------------------------------------------------------
dump does not work

-------------------------------------------------------------------------------
binary insert

-------------------------------------------------------------------------------
assertion failed: (self.root->depth == tmp->depth)
../src/routing_table.cpp: 1166

Stack trace (most recent call last):
#0    unsigned long backward::details::unwind<backward::StackTraceImpl<backward::system_tag::linux_tag>::callback>(backward::StackTraceImpl<backward::system_tag::linux_tag>::callback, unsigned long) backward.hpp:851
#1    backward::StackTraceImpl<backward::system_tag::linux_tag>::load_here(unsigned long, void*, void*) backward.hpp:869
#2    sp::impl::print_backtrace(_IO_FILE*) assert.cpp:124
#3    sp::impl::assert_func(char const*, int, char const*, char const*) assert.cpp:161
#4    dht::make_routing_table(dht::DHTMetaRoutingTable&, unsigned long) routing_table.cpp:1166
#5    dht::insert(dht::DHTMetaRoutingTable&, dht::Node const&) routing_table.cpp:1203
#6    dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}::operator()(dht::Node const&) const scrape.cpp:114
#7    bool dht::for_all<dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}>(dht::Bucket const&, dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}) routing_table.h:224
#8    dht::for_all_node<dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}>(dht::RoutingTable const*, dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1})::{lambda(dht::RoutingTable const&)#1}::operator()(dht::RoutingTable const&) const routing_table.h:278
#9    bool dht::for_all<dht::for_all_node<dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}>(dht::RoutingTable const*, dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1})::{lambda(dht::RoutingTable const&)#1}>(dht::RoutingTable const*, dht::for_all_node<dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}>(dht::RoutingTable const*, dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1})::{lambda(dht::RoutingTable const&)#1}) routing_table.h:261
#10   bool dht::for_all_node<dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}>(dht::RoutingTable const*, dht::swap_in_new(dht::DHT&, unsigned long)::{lambda(dht::Node const&)#1}) routing_table.h:276
#11   dht::swap_in_new(dht::DHT&, unsigned long) scrape.cpp:106
#12   dht::on_awake_scrape(dht::DHT&, sp::IBytesView<unsigned char>&) scrape.cpp:220
#13   auto main::{lambda(sp::IBytesView<unsigned char>&)#1}::operator()(sp::IBytesView<unsigned char>&) const::{lambda(auto:1, auto:2)#1}::operator()<sp::Timestamp, sp::Timestamp (*)(dht::DHT&, sp::IBytesView<unsigned char>&) noexcept>(sp::Timestamp, sp::Timestamp (*)(dht::DHT&, sp::IBytesView<unsigned char>&) noexcept) const dht-server.cpp:580
#14   sp::Timestamp& sp::reduce<sp::Timestamp (*)(dht::DHT&, sp::IBytesView<unsigned char>&) noexcept, sp::Timestamp, main::{lambda(sp::IBytesView<unsigned char>&)#1}::operator()(sp::IBytesView<unsigned char>&) const::{lambda(auto:1, auto:2)#1}>(sp::Array<sp::Timestamp (*)(dht::DHT&, sp::IBytesView<unsigned char>&) noexcept> const&, sp::Timestamp&, main::{lambda(sp::IBytesView<unsigned char>&)#1}::operator()(sp::IBytesView<unsigned char>&) const::{lambda(auto:1, auto:2)#1}) Array.h:2103
#15   main::{lambda(sp::IBytesView<unsigned char>&)#1}::operator()(sp::IBytesView<unsigned char>&) const dht-server.cpp:588
#16   int main_loop<main::{lambda(sp::IBytesView<unsigned char>&)#1}>(dht::DHT&, main::{lambda(sp::IBytesView<unsigned char>&)#1}) dht-server.cpp:449
#17   main dht-server.cpp:600
#18   __libc_init_first
#19   __libc_start_main
#20   _start
#21

>>> p self.root->depth
$3 = 5
>>> p tmp->depth
$4 = 6
>>> p in_tree->depth
$5 = 7

-----------------------------------------------------------------------------------------

2024-12-21 21:24:55|receive response ping      (94.240.216.232:57256) <686EF20E> [UT]
2024-12-21 21:24:55|receive response ping      (172.97.137.169:25600) <6267E4FF> [UT]

assertion failed: (debug_count_good(self) == result)
../src/routing_table.cpp: 1371


-------------------------------------------------------------------------------
TODO
RoutingTableNode[length, Array[...]]
- no linear scans of RoutingTableNode
- if peer is remove from a RoutingTable Node the last peer in thn
  RoutingTableNode gets its place

  - multiple closest
TODO routing_table heap node compare (heap entry needs to be updated when new nodes are added/removed)

- scrape main routing_table as well

- db populate based on 2M latest
  - size of bloomfilter ...
  - query planner


-------------------------------------------------------------------------------
TODO dump

-------------------------------------------------------------------------------
bootstrap bloomfilter eagerly insert only when rank is >= x
- test at each dequeue
- insert at each dequeue

---------------------------------------------------------------------
