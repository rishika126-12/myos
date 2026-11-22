#include "drivers/pci.h"
#include "io.h"
#include "drivers/serial.h"

pci_device_t pci_devices[MAX_PCI_DEVICES];
int          pci_device_count = 0;

uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31) |
                                  ((uint32_t)bus << 16) |
                                  ((uint32_t)slot << 11) |
                                  ((uint32_t)func << 8) |
                                  (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((1U << 31) |
                                  ((uint32_t)bus << 16) |
                                  ((uint32_t)slot << 11) |
                                  ((uint32_t)func << 8) |
                                  (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_read_config_dword(bus, slot, func, offset);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

static void pci_check_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t reg0 = pci_read_config_dword(bus, slot, func, 0x00);
    uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
    uint16_t device_id = (uint16_t)((reg0 >> 16) & 0xFFFF);

    if (vendor_id == 0xFFFF || vendor_id == 0x0000) return;

    uint32_t class_reg = pci_read_config_dword(bus, slot, func, 0x08);
    uint8_t  class_id  = (uint8_t)(class_reg >> 24);
    uint8_t  subclass  = (uint8_t)(class_reg >> 16);
    uint8_t  prog_if   = (uint8_t)(class_reg >> 8);

    uint32_t intr_reg  = pci_read_config_dword(bus, slot, func, 0x3C);
    uint8_t  irq       = (uint8_t)(intr_reg & 0xFF);

    if (pci_device_count < MAX_PCI_DEVICES) {
        pci_device_t* dev = &pci_devices[pci_device_count++];
        dev->bus         = bus;
        dev->slot        = slot;
        dev->func        = func;
        dev->vendor_id   = vendor_id;
        dev->device_id   = device_id;
        dev->class_id    = class_id;
        dev->subclass_id = subclass;
        dev->prog_if     = prog_if;
        dev->irq         = irq;

        dev->bar0 = pci_read_config_dword(bus, slot, func, 0x10);
        dev->bar1 = pci_read_config_dword(bus, slot, func, 0x14);
        dev->bar2 = pci_read_config_dword(bus, slot, func, 0x18);
        dev->bar3 = pci_read_config_dword(bus, slot, func, 0x1C);
        dev->bar4 = pci_read_config_dword(bus, slot, func, 0x20);
        dev->bar5 = pci_read_config_dword(bus, slot, func, 0x24);

        serial_printf("[PCI] Found %02x:%02x.%d: Vendor %04x Device %04x (Class %02x.%02x, IRQ %d, BAR0=%p)\n",
                      bus, slot, func, vendor_id, device_id, class_id, subclass, irq, (void*)dev->bar0);
    }
}

static void pci_check_device(uint8_t bus, uint8_t slot) {
    uint32_t reg0 = pci_read_config_dword(bus, slot, 0, 0x00);
    uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000) return;

    pci_check_function(bus, slot, 0);

    uint32_t header_reg = pci_read_config_dword(bus, slot, 0, 0x0C);
    uint8_t header_type = (uint8_t)((header_reg >> 16) & 0xFF);

    // If multi-function bit is set, check functions 1..7
    if (header_type & 0x80) {
        for (uint8_t func = 1; func < 8; func++) {
            pci_check_function(bus, slot, func);
        }
    }
}

void pci_init(void) {
    pci_device_count = 0;
    serial_puts("[PCI] Scanning PCI bus 0...\n");

    for (uint8_t slot = 0; slot < 32; slot++) {
        pci_check_device(0, slot);
    }
    serial_printf("[PCI] PCI scan complete. Found %d devices.\n", pci_device_count);
}

pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            (device_id == 0xFFFF || pci_devices[i].device_id == device_id)) {
            return &pci_devices[i];
        }
    }
    return NULL;
}
