-------------------------------------------------------------------------------
# scrape
TODO sample_infohashes
TODO collect peer version of clients that support sample_infohashes

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


-------------------------------------------------------------------------------
TODO
- save routing table to disk
- restore routing table from disk
- if we regenerate new self node id we reinsert to the routing table otherwise
  we just restore it as is from disk

-------------------------------------------------------------------------------
TODO
RoutingTableNode[length, Array[...]]
- if peer is remove from a RoutingTableNode the last peer in the
  RoutingTableNode gets its place

TODO no linear scans of RoutingTableNode

-------------------------------------------------------------------------------
