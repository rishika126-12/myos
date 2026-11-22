#include "drivers/e1000.h"
#include "kernel/heap.h"
#include "string.h"
#include "drivers/serial.h"

// Register Offsets
#define REG_CTRL       0x0000
#define REG_STATUS     0x0008
#define REG_EECD       0x0010
#define REG_EERD       0x0014
#define REG_ICR        0x00C0
#define REG_IMS        0x00D0
#define REG_IMC        0x00D8
#define REG_RCTL       0x0100
#define REG_TCTL       0x0400
#define REG_RDBAL      0x2800
#define REG_RDBAH      0x2804
#define REG_RDLEN      0x2808
#define REG_RDH        0x2810
#define REG_RDT        0x2818
#define REG_TDBAL      0x3800
#define REG_TDBAH      0x3804
#define REG_TDLEN      0x3808
#define REG_TDH        0x3810
#define REG_TDT        0x3818
#define REG_MTA        0x5200
#define REG_RAL        0x5400
#define REG_RAH        0x5404

// RCTL bits
#define RCTL_EN             (1 << 1)
#define RCTL_SBP            (1 << 2)
#define RCTL_UPE            (1 << 3)
#define RCTL_MPE            (1 << 4)
#define RCTL_BAM            (1 << 15)
#define RCTL_BSIZE_2048     (0 << 16)
#define RCTL_SECRC          (1 << 26)

// TCTL bits
#define TCTL_EN             (1 << 1)
#define TCTL_PSP            (1 << 3)
#define TCTL_CT_SHIFT       4
#define TCTL_COLD_SHIFT     12

// Descriptors
struct e1000_rx_desc {
    uint32_t addr_low;
    uint32_t addr_high;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));
typedef struct e1000_rx_desc e1000_rx_desc_t;

struct e1000_tx_desc {
    uint32_t addr_low;
    uint32_t addr_high;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));
typedef struct e1000_tx_desc e1000_tx_desc_t;

e1000_device_t e1000_nic = {0};

static e1000_rx_desc_t rx_descs[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[E1000_NUM_TX_DESC] __attribute__((aligned(16)));

static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_PKT_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][E1000_PKT_BUF_SIZE] __attribute__((aligned(16)));

static inline void write_reg(uint32_t reg, uint32_t val) {
    volatile uint32_t* ptr = (volatile uint32_t*)(e1000_nic.mmio_base + reg);
    *ptr = val;
}

static inline uint32_t read_reg(uint32_t reg) {
    volatile uint32_t* ptr = (volatile uint32_t*)(e1000_nic.mmio_base + reg);
    return *ptr;
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    uint32_t val = (1 << 0) | ((uint32_t)addr << 8); // Start read
    write_reg(REG_EERD, val);
    uint32_t timeout = 100000;
    while (!((val = read_reg(REG_EERD)) & (1 << 4)) && --timeout);
    return (uint16_t)((val >> 16) & 0xFFFF);
}

static void e1000_read_mac(void) {
    uint32_t ral = read_reg(REG_RAL);
    uint32_t rah = read_reg(REG_RAH);

    if (ral != 0 && ral != 0xFFFFFFFF) {
        e1000_nic.mac[0] = (uint8_t)(ral & 0xFF);
        e1000_nic.mac[1] = (uint8_t)((ral >> 8) & 0xFF);
        e1000_nic.mac[2] = (uint8_t)((ral >> 16) & 0xFF);
        e1000_nic.mac[3] = (uint8_t)((ral >> 24) & 0xFF);
        e1000_nic.mac[4] = (uint8_t)(rah & 0xFF);
        e1000_nic.mac[5] = (uint8_t)((rah >> 8) & 0xFF);
    } else {
        uint16_t w0 = e1000_read_eeprom(0);
        uint16_t w1 = e1000_read_eeprom(1);
        uint16_t w2 = e1000_read_eeprom(2);

        e1000_nic.mac[0] = (uint8_t)(w0 & 0xFF);
        e1000_nic.mac[1] = (uint8_t)((w0 >> 8) & 0xFF);
        e1000_nic.mac[2] = (uint8_t)(w1 & 0xFF);
        e1000_nic.mac[3] = (uint8_t)((w1 >> 8) & 0xFF);
        e1000_nic.mac[4] = (uint8_t)(w2 & 0xFF);
        e1000_nic.mac[5] = (uint8_t)((w2 >> 8) & 0xFF);

        // Program RAL/RAH
        write_reg(REG_RAL, (uint32_t)e1000_nic.mac[0] | ((uint32_t)e1000_nic.mac[1] << 8) |
                           ((uint32_t)e1000_nic.mac[2] << 16) | ((uint32_t)e1000_nic.mac[3] << 24));
        write_reg(REG_RAH, (uint32_t)e1000_nic.mac[4] | ((uint32_t)e1000_nic.mac[5] << 8) | (1U << 31));
    }

    serial_printf("[E1000] MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  e1000_nic.mac[0], e1000_nic.mac[1], e1000_nic.mac[2],
                  e1000_nic.mac[3], e1000_nic.mac[4], e1000_nic.mac[5]);
}

bool e1000_init(pci_device_t* pci_dev) {
    if (!pci_dev) return false;

    // Enable Bus Mastering (bit 2), Memory Space (bit 1), I/O Space (bit 0)
    uint32_t cmd = pci_read_config_dword(pci_dev->bus, pci_dev->slot, pci_dev->func, 0x04);
    cmd |= 0x07;
    pci_write_config_dword(pci_dev->bus, pci_dev->slot, pci_dev->func, 0x04, cmd);

    e1000_nic.mmio_base = pci_dev->bar0 & ~0x0F;
    serial_printf("[E1000] Initializing Intel 82540EM at MMIO %p\n", (void*)e1000_nic.mmio_base);

    // Read MAC address
    e1000_read_mac();

    // Reset Multicast Table Array
    for (int i = 0; i < 128; i++) {
        write_reg(REG_MTA + i * 4, 0);
    }

    // Initialize RX Ring
    memset(rx_descs, 0, sizeof(rx_descs));
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr_low = (uint32_t)&rx_buffers[i][0];
        rx_descs[i].addr_high = 0;
        rx_descs[i].status = 0;
    }

    write_reg(REG_RDBAL, (uint32_t)&rx_descs[0]);
    write_reg(REG_RDBAH, 0);
    write_reg(REG_RDLEN, sizeof(rx_descs));
    write_reg(REG_RDH, 0);
    write_reg(REG_RDT, E1000_NUM_RX_DESC - 1);
    e1000_nic.rx_cur = 0;

    // Initialize TX Ring
    memset(tx_descs, 0, sizeof(tx_descs));
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr_low = (uint32_t)&tx_buffers[i][0];
        tx_descs[i].addr_high = 0;
        tx_descs[i].status = 1; // Mark initially free/done
    }

    write_reg(REG_TDBAL, (uint32_t)&tx_descs[0]);
    write_reg(REG_TDBAH, 0);
    write_reg(REG_TDLEN, sizeof(tx_descs));
    write_reg(REG_TDH, 0);
    write_reg(REG_TDT, 0);
    e1000_nic.tx_cur = 0;

    // Enable RX: broadcast accept, buffer size 2048, strip CRC, multicast promiscuous
    write_reg(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC | RCTL_MPE);

    // Enable TX: pad short packets, collision thresholds
    write_reg(REG_TCTL, TCTL_EN | TCTL_PSP | (0x0F << TCTL_CT_SHIFT) | (0x40 << TCTL_COLD_SHIFT));

    // Disable interrupts for polling mode
    write_reg(REG_IMC, 0xFFFFFFFF);

    e1000_nic.is_initialized = true;
    serial_puts("[E1000] Intel PRO/1000 initialized successfully!\n");
    return true;
}

bool e1000_send_packet(const uint8_t* data, uint16_t len) {
    if (!e1000_nic.is_initialized || len == 0 || len > E1000_PKT_BUF_SIZE) return false;

    uint32_t cur = e1000_nic.tx_cur;
    e1000_tx_desc_t* desc = &tx_descs[cur];

    memcpy((void*)desc->addr_low, data, len);
    desc->length = len;
    desc->cmd = (1 << 0) | (1 << 3); // EOP (End of Packet) | RS (Report Status)
    desc->status = 0;

    e1000_nic.tx_cur = (cur + 1) % E1000_NUM_TX_DESC;
    write_reg(REG_TDT, e1000_nic.tx_cur);

    // Wait for transmit complete
    uint32_t timeout = 100000;
    while (!(desc->status & 1) && --timeout);

    return (desc->status & 1) != 0;
}

int e1000_poll_packet(uint8_t* buffer, uint16_t max_len) {
    if (!e1000_nic.is_initialized || !buffer || max_len == 0) return 0;

    uint32_t cur = e1000_nic.rx_cur;
    e1000_rx_desc_t* desc = &rx_descs[cur];

    if (!(desc->status & 1)) {
        return 0; // No packet ready
    }

    uint16_t len = desc->length;
    if (len > max_len) len = max_len;

    memcpy(buffer, (const void*)desc->addr_low, len);

    // Reset status and advance tail
    desc->status = 0;
    write_reg(REG_RDT, cur);
    e1000_nic.rx_cur = (cur + 1) % E1000_NUM_RX_DESC;

    return (int)len;
}
