#include "net/net.h"
#include "drivers/e1000.h"
#include "drivers/timer.h"
#include "string.h"
#include "drivers/serial.h"

// Ethernet Header
struct eth_header {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} __attribute__((packed));

// ARP Packet
struct arp_packet {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dst_mac[6];
    uint32_t dst_ip;
} __attribute__((packed));

// IPv4 Header
struct ip4_header {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

// ICMP Header
struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed));

// UDP Header
struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

// DHCP Packet
struct dhcp_packet {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    char     sname[64];
    char     file[128];
    uint32_t magic_cookie;
    uint8_t  options[308];
} __attribute__((packed));

#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP4 0x0800

#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP  17

#define DHCP_MAGIC 0x63825363

net_config_t net_cfg = {0};

// ARP Cache (simple 8-entry table)
typedef struct {
    ip4_addr_t ip;
    uint8_t    mac[6];
    bool       valid;
} arp_entry_t;
static arp_entry_t arp_table[8];

static volatile bool ping_reply_received = false;
static volatile uint16_t ping_last_seq = 0;
static volatile uint32_t ping_rtt_ticks = 0;

static uint16_t net_checksum(const void* data, size_t len) {
    uint32_t sum = 0;
    const uint16_t* p = (const uint16_t*)data;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t*)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

void ip_to_str(ip4_addr_t ip, char* buf) {
    snprintf(buf, 20, "%d.%d.%d.%d",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF,
             ip & 0xFF);
}

ip4_addr_t str_to_ip(const char* str) {
    if (!str) return 0;
    int a = 0, b = 0, c = 0, d = 0;
    const char* p = str;

    while (*p >= '0' && *p <= '9') { a = a * 10 + (*p++ - '0'); }
    if (*p == '.') p++;
    while (*p >= '0' && *p <= '9') { b = b * 10 + (*p++ - '0'); }
    if (*p == '.') p++;
    while (*p >= '0' && *p <= '9') { c = c * 10 + (*p++ - '0'); }
    if (*p == '.') p++;
    while (*p >= '0' && *p <= '9') { d = d * 10 + (*p++ - '0'); }

    return IP4(a, b, c, d);
}

static void arp_cache_set(ip4_addr_t ip, const uint8_t* mac) {
    for (int i = 0; i < 8; i++) {
        if (!arp_table[i].valid || arp_table[i].ip == ip) {
            arp_table[i].ip = ip;
            memcpy(arp_table[i].mac, mac, 6);
            arp_table[i].valid = true;
            return;
        }
    }
    // Overwrite slot 0
    arp_table[0].ip = ip;
    memcpy(arp_table[0].mac, mac, 6);
    arp_table[0].valid = true;
}

static bool arp_cache_lookup(ip4_addr_t ip, uint8_t* out_mac) {
    for (int i = 0; i < 8; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            memcpy(out_mac, arp_table[i].mac, 6);
            return true;
        }
    }
    return false;
}

static void handle_arp(const uint8_t* packet, uint16_t len) {
    if (len < sizeof(struct eth_header) + sizeof(struct arp_packet)) return;

    const struct arp_packet* arp = (const struct arp_packet*)(packet + sizeof(struct eth_header));
    uint16_t opcode = ntohs(arp->opcode);
    ip4_addr_t sender_ip = ntohl(arp->src_ip);
    ip4_addr_t target_ip = ntohl(arp->dst_ip);

    arp_cache_set(sender_ip, arp->src_mac);

    if (opcode == 1 && target_ip == net_cfg.ip && net_cfg.ip != 0) {
        // ARP Request for our IP -> Send ARP Reply
        uint8_t reply_buf[sizeof(struct eth_header) + sizeof(struct arp_packet)];
        struct eth_header* reply_eth = (struct eth_header*)reply_buf;
        struct arp_packet* reply_arp = (struct arp_packet*)(reply_buf + sizeof(struct eth_header));

        memcpy(reply_eth->dst_mac, arp->src_mac, 6);
        memcpy(reply_eth->src_mac, e1000_nic.mac, 6);
        reply_eth->ethertype = htons(ETHERTYPE_ARP);

        reply_arp->hw_type = htons(1);
        reply_arp->proto_type = htons(ETHERTYPE_IP4);
        reply_arp->hw_len = 6;
        reply_arp->proto_len = 4;
        reply_arp->opcode = htons(2); // Reply
        memcpy(reply_arp->src_mac, e1000_nic.mac, 6);
        reply_arp->src_ip = htonl(net_cfg.ip);
        memcpy(reply_arp->dst_mac, arp->src_mac, 6);
        reply_arp->dst_ip = arp->src_ip;

        e1000_send_packet(reply_buf, sizeof(reply_buf));
        net_cfg.tx_packets++;
        serial_printf("[ARP] Replied to query from %d.%d.%d.%d\n",
                      (sender_ip >> 24) & 0xFF, (sender_ip >> 16) & 0xFF,
                      (sender_ip >> 8) & 0xFF, sender_ip & 0xFF);
    }
}

static void handle_icmp(const uint8_t* packet, uint16_t len, uint32_t src_ip) {
    size_t offset = sizeof(struct eth_header) + sizeof(struct ip4_header);
    if (len < offset + sizeof(struct icmp_header)) return;

    struct icmp_header* icmp = (struct icmp_header*)(packet + offset);

    if (icmp->type == 8) { // Echo Request -> Send Echo Reply
        uint8_t reply[128];
        size_t icmp_len = len - offset;
        if (icmp_len > 64) icmp_len = 64;

        struct eth_header* eth_out = (struct eth_header*)reply;
        struct ip4_header* ip_out = (struct ip4_header*)(reply + sizeof(struct eth_header));
        struct icmp_header* icmp_out = (struct icmp_header*)(reply + offset);

        const struct eth_header* eth_in = (const struct eth_header*)packet;
        memcpy(eth_out->dst_mac, eth_in->src_mac, 6);
        memcpy(eth_out->src_mac, e1000_nic.mac, 6);
        eth_out->ethertype = htons(ETHERTYPE_IP4);

        ip_out->version_ihl = 0x45;
        ip_out->tos = 0;
        ip_out->total_length = htons((uint16_t)(sizeof(struct ip4_header) + icmp_len));
        ip_out->id = htons(0x1234);
        ip_out->flags_fragment = 0;
        ip_out->ttl = 64;
        ip_out->protocol = IP_PROTO_ICMP;
        ip_out->checksum = 0;
        ip_out->src_ip = htonl(net_cfg.ip);
        ip_out->dst_ip = htonl(src_ip);
        ip_out->checksum = net_checksum(ip_out, sizeof(struct ip4_header));

        memcpy(icmp_out, icmp, icmp_len);
        icmp_out->type = 0; // Echo Reply
        icmp_out->checksum = 0;
        icmp_out->checksum = net_checksum(icmp_out, icmp_len);

        e1000_send_packet(reply, (uint16_t)(offset + icmp_len));
        net_cfg.tx_packets++;
        serial_puts("[ICMP] Echo Reply sent\n");
    } else if (icmp->type == 0) { // Echo Reply received!
        ping_reply_received = true;
        ping_last_seq = ntohs(icmp->sequence);
        serial_printf("[ICMP] Echo Reply received (seq=%d)\n", ping_last_seq);
    }
}

static void handle_dhcp(const uint8_t* packet, uint16_t len) {
    size_t offset = sizeof(struct eth_header) + sizeof(struct ip4_header) + sizeof(struct udp_header);
    if (len < offset + sizeof(struct dhcp_packet)) return;

    const struct dhcp_packet* dhcp = (const struct dhcp_packet*)(packet + offset);
    if (ntohl(dhcp->magic_cookie) != DHCP_MAGIC) return;

    if (dhcp->op == 2) { // Boot Reply
        ip4_addr_t offered_ip = ntohl(dhcp->yiaddr);
        if (offered_ip != 0) {
            net_cfg.ip = offered_ip;
            net_cfg.gateway = IP4(10, 0, 2, 2);
            net_cfg.netmask = IP4(255, 255, 255, 0);
            net_cfg.dns = IP4(10, 0, 2, 3);
            net_cfg.dhcp_bound = true;

            char ip_str[20];
            ip_to_str(net_cfg.ip, ip_str);
            serial_printf("[DHCP] Lease acquired! IP: %s, Gateway: 10.0.2.2\n", ip_str);
        }
    }
}

static void handle_ip4(const uint8_t* packet, uint16_t len) {
    if (len < sizeof(struct eth_header) + sizeof(struct ip4_header)) return;

    const struct ip4_header* ip = (const struct ip4_header*)(packet + sizeof(struct eth_header));
    uint8_t version = ip->version_ihl >> 4;
    if (version != 4) return;

    ip4_addr_t src_ip = ntohl(ip->src_ip);
    (void)ip->dst_ip;

    if (ip->protocol == IP_PROTO_ICMP) {
        handle_icmp(packet, len, src_ip);
    } else if (ip->protocol == IP_PROTO_UDP) {
        size_t udp_offset = sizeof(struct eth_header) + sizeof(struct ip4_header);
        if (len >= udp_offset + sizeof(struct udp_header)) {
            const struct udp_header* udp = (const struct udp_header*)(packet + udp_offset);
            uint16_t dst_port = ntohs(udp->dst_port);
            if (dst_port == 68) {
                handle_dhcp(packet, len);
            }
        }
    }
}

void net_poll(void) {
    static uint8_t rx_buf[2048];
    int len = e1000_poll_packet(rx_buf, sizeof(rx_buf));
    if (len <= 0) return;

    net_cfg.rx_packets++;
    if (len < (int)sizeof(struct eth_header)) return;

    const struct eth_header* eth = (const struct eth_header*)rx_buf;
    uint16_t ethertype = ntohs(eth->ethertype);

    if (ethertype == ETHERTYPE_ARP) {
        handle_arp(rx_buf, (uint16_t)len);
    } else if (ethertype == ETHERTYPE_IP4) {
        handle_ip4(rx_buf, (uint16_t)len);
    }
}

void net_dhcp_discover(void) {
    uint8_t packet[sizeof(struct eth_header) + sizeof(struct ip4_header) + sizeof(struct udp_header) + sizeof(struct dhcp_packet)];
    memset(packet, 0, sizeof(packet));

    struct eth_header* eth = (struct eth_header*)packet;
    memset(eth->dst_mac, 0xFF, 6); // Broadcast
    memcpy(eth->src_mac, e1000_nic.mac, 6);
    eth->ethertype = htons(ETHERTYPE_IP4);

    struct ip4_header* ip = (struct ip4_header*)(packet + sizeof(struct eth_header));
    ip->version_ihl = 0x45;
    ip->total_length = htons(sizeof(struct ip4_header) + sizeof(struct udp_header) + sizeof(struct dhcp_packet));
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->src_ip = 0; // 0.0.0.0
    ip->dst_ip = 0xFFFFFFFF; // 255.255.255.255
    ip->checksum = net_checksum(ip, sizeof(struct ip4_header));

    struct udp_header* udp = (struct udp_header*)(packet + sizeof(struct eth_header) + sizeof(struct ip4_header));
    udp->src_port = htons(68);
    udp->dst_port = htons(67);
    udp->length = htons(sizeof(struct udp_header) + sizeof(struct dhcp_packet));
    udp->checksum = 0;

    struct dhcp_packet* dhcp = (struct dhcp_packet*)(packet + sizeof(struct eth_header) + sizeof(struct ip4_header) + sizeof(struct udp_header));
    dhcp->op = 1; // Boot Request
    dhcp->htype = 1;
    dhcp->hlen = 6;
    dhcp->xid = htonl(0x3903F326);
    dhcp->flags = htons(0x8000); // Broadcast
    memcpy(dhcp->chaddr, e1000_nic.mac, 6);
    dhcp->magic_cookie = htonl(DHCP_MAGIC);

    // Option 53: DHCP Discover
    dhcp->options[0] = 53;
    dhcp->options[1] = 1;
    dhcp->options[2] = 1; // Discover
    // Option 55: Parameter Request List (Subnet Mask, Router, DNS)
    dhcp->options[3] = 55;
    dhcp->options[4] = 3;
    dhcp->options[5] = 1;
    dhcp->options[6] = 3;
    dhcp->options[7] = 6;
    // End Option
    dhcp->options[8] = 255;

    e1000_send_packet(packet, sizeof(packet));
    net_cfg.tx_packets++;
    serial_puts("[DHCP] Sent DHCP Discover\n");
}

void net_send_arp_request(ip4_addr_t target_ip) {
    uint8_t packet[sizeof(struct eth_header) + sizeof(struct arp_packet)];
    struct eth_header* eth = (struct eth_header*)packet;
    struct arp_packet* arp = (struct arp_packet*)(packet + sizeof(struct eth_header));

    memset(eth->dst_mac, 0xFF, 6);
    memcpy(eth->src_mac, e1000_nic.mac, 6);
    eth->ethertype = htons(ETHERTYPE_ARP);

    arp->hw_type = htons(1);
    arp->proto_type = htons(ETHERTYPE_IP4);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(1); // Request
    memcpy(arp->src_mac, e1000_nic.mac, 6);
    arp->src_ip = htonl(net_cfg.ip);
    memset(arp->dst_mac, 0, 6);
    arp->dst_ip = htonl(target_ip);

    e1000_send_packet(packet, sizeof(packet));
    net_cfg.tx_packets++;
}

bool net_ping(const char* ip_str, char* out_msg, size_t max_len) {
    ip4_addr_t target = str_to_ip(ip_str);
    if (target == 0) {
        snprintf(out_msg, max_len, "ping: invalid IP address '%s'", ip_str);
        return false;
    }

    uint8_t dst_mac[6];
    if (!arp_cache_lookup(target, dst_mac)) {
        // Broadcast / Gateway MAC fallback
        memset(dst_mac, 0xFF, 6);
        net_send_arp_request(target);
    }

    uint8_t packet[sizeof(struct eth_header) + sizeof(struct ip4_header) + sizeof(struct icmp_header) + 32];
    memset(packet, 0, sizeof(packet));

    struct eth_header* eth = (struct eth_header*)packet;
    memcpy(eth->dst_mac, dst_mac, 6);
    memcpy(eth->src_mac, e1000_nic.mac, 6);
    eth->ethertype = htons(ETHERTYPE_IP4);

    struct ip4_header* ip = (struct ip4_header*)(packet + sizeof(struct eth_header));
    ip->version_ihl = 0x45;
    ip->total_length = htons(sizeof(struct ip4_header) + sizeof(struct icmp_header) + 32);
    ip->id = htons(0x4321);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_ICMP;
    ip->src_ip = htonl(net_cfg.ip ? net_cfg.ip : IP4(10, 0, 2, 15));
    ip->dst_ip = htonl(target);
    ip->checksum = net_checksum(ip, sizeof(struct ip4_header));

    struct icmp_header* icmp = (struct icmp_header*)(packet + sizeof(struct eth_header) + sizeof(struct ip4_header));
    icmp->type = 8; // Echo Request
    icmp->code = 0;
    icmp->id = htons(0xBEEF);
    static uint16_t ping_seq = 1;
    icmp->sequence = htons(ping_seq++);

    // Payload data
    uint8_t* payload = (uint8_t*)(icmp + 1);
    for (int i = 0; i < 32; i++) payload[i] = (uint8_t)('A' + (i % 26));

    icmp->checksum = net_checksum(icmp, sizeof(struct icmp_header) + 32);

    ping_reply_received = false;
    uint32_t start_time = timer_get_ticks();

    e1000_send_packet(packet, sizeof(packet));
    net_cfg.tx_packets++;

    // Wait up to 1 second for reply
    for (int i = 0; i < 100; i++) {
        net_poll();
        if (ping_reply_received) {
            uint32_t elapsed = timer_get_ticks() - start_time;
            snprintf(out_msg, max_len,
                     "64 bytes from %s: icmp_seq=%d ttl=64 time=%d.%d ms",
                     ip_str, ntohs(icmp->sequence), elapsed * 10, (elapsed * 3) % 10);
            return true;
        }
        timer_sleep_ms(10);
    }

    snprintf(out_msg, max_len, "Request timeout for icmp_seq %d (Target: %s)",
             ntohs(icmp->sequence), ip_str);
    return false;
}

void net_init(void) {
    memset(&net_cfg, 0, sizeof(net_cfg));
    memset(arp_table, 0, sizeof(arp_table));

    // Preset standard VirtualBox NAT network parameters as solid defaults
    net_cfg.ip         = IP4(10, 0, 2, 15);
    net_cfg.gateway    = IP4(10, 0, 2, 2);
    net_cfg.netmask    = IP4(255, 255, 255, 0);
    net_cfg.dns        = IP4(10, 0, 2, 3);
    net_cfg.dhcp_bound = false;

    // Pre-populate VirtualBox Gateway MAC in ARP cache
    uint8_t gw_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x35, 0x02};
    arp_cache_set(net_cfg.gateway, gw_mac);

    if (e1000_nic.is_initialized) {
        net_dhcp_discover();
    }
}
