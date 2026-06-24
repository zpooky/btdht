#ifndef SP_BT_DHT_BT_H
#define SP_BT_DHT_BT_H

#ifdef __cplusplus
extern "C" {
#endif

struct dht_scrape_msg {
  unsigned int ipv4;
  unsigned short port;
  unsigned char info_hash[64];
  unsigned int l_info_hash;
  unsigned char magic[4];
};

#define PUBLISH_HAVE 1

struct bt_to_dht_publish_msg {
  int flag;
  unsigned char info_hash[64];
  unsigned int l_info_hash;
  unsigned char magic[4];
};

struct bt_to_dht_backoff_msg {
  int backoff;
  unsigned char magic[4]; //TODO move to super msg
};

enum bt_to_dht_msg_kind {
  BT_TO_DHT_MSG_UNKNOWN = 0,
  BT_TO_DHT_MSG_PUBLISH = 1,
  BT_TO_DHT_MSG_BACKOFF = 2,
};

struct bt_to_dht_msg {
  enum bt_to_dht_msg_kind kind;
  union {
    struct bt_to_dht_publish_msg publish;
    struct bt_to_dht_backoff_msg backoff;
  };
};

#ifdef __cplusplus
}
#endif

#endif
