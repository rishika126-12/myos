#ifndef NET_H
#define NET_H

#include "types.h"

// Endian conversions
static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}
static inline uint16_t ntohs(uint16_t v) {
    return htons(v);
}
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}
static inline uint32_t ntohl(uint32_t v) {
    return htonl(v);
}

// IP representation
typedef uint32_t ip4_addr_t;

#define IP4(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

typedef struct {
    ip4_addr_t ip;
    ip4_addr_t gateway;
    ip4_addr_t netmask;
    ip4_addr_t dns;
    bool       dhcp_bound;
    uint32_t   rx_packets;
    uint32_t   tx_packets;
} net_config_t;

extern net_config_t net_cfg;

void net_init(void);
void net_poll(void);

void net_dhcp_discover(void);
void net_send_arp_request(ip4_addr_t target_ip);
bool net_ping(const char* ip_str, char* out_msg, size_t max_len);

void ip_to_str(ip4_addr_t ip, char* buf);
ip4_addr_t str_to_ip(const char* str);

#endif
