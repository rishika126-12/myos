#ifndef E1000_H
#define E1000_H

#include "types.h"
#include "drivers/pci.h"

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 16
#define E1000_PKT_BUF_SIZE 2048

typedef struct {
    uint8_t mac[6];
    uint32_t mmio_base;
    bool is_initialized;
    uint32_t rx_cur;
    uint32_t tx_cur;
} e1000_device_t;

extern e1000_device_t e1000_nic;

bool e1000_init(pci_device_t* pci_dev);
bool e1000_send_packet(const uint8_t* data, uint16_t len);
int  e1000_poll_packet(uint8_t* buffer, uint16_t max_len);

#endif
