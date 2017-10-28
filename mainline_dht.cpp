#include "mainline_dht.h"
namespace dht {

/*Contact*/
Contact::Contact()
    : last_activity()
    , outstanding_ping(false)
    , peer() {
}

/*Bucket*/
Bucket::Bucket()
    : higher(nullptr)
    , lower(nullptr) {
}
Bucket::~Bucket() {
}

/*RoutingTable*/
RoutingTable::RoutingTable()
    : base(nullptr) {
}
RoutingTable::~RoutingTable() {
}

void
split(Bucket *) {
}

//
void
get_peers(const infohash &key) {
  kadmelia::FIND_VALUE(key);
}

} // namespace dht
