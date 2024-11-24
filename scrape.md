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

- only store one bit (support scrape in Node in routing_table (bitmap)) instead of version
```
struct {
  bool:1 support_scrape;
  bool:1 is_strict_node_id;
  bool:1 is_bad;
} properties;
```
- scrape main routing_table as well

- db populate based on 1M latest
  - size of bloomfilter ...
  - query planner

-------------------------------------------------------------------------------
