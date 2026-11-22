#include "drivers/ata.h"
#include "io.h"
#include "drivers/serial.h"

static bool ata_drive_present = false;

static void ata_wait_ready(void) {
    while (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_BSY);
}

static bool ata_wait_drq(void) {
    uint32_t timeout = 100000;
    while (--timeout) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) return false;
        if (status & ATA_STATUS_DRQ) return true;
    }
    return false;
}

bool ata_init(void) {
    // Select Master drive on Primary Bus (0xA0)
    outb(ATA_PRIMARY_DRIVE_SEL, 0xA0);
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);

    // Send IDENTIFY command
    outb(ATA_PRIMARY_CMD, ATA_CMD_IDENTIFY);
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        serial_puts("[ATA] No IDE drive present on Primary Master\n");
        ata_drive_present = false;
        return false;
    }

    ata_wait_ready();
    uint8_t mid = inb(ATA_PRIMARY_LBA_MID);
    uint8_t hi  = inb(ATA_PRIMARY_LBA_HI);
    if (mid != 0 || hi != 0) {
        // Not ATA device (e.g. ATAPI)
        serial_puts("[ATA] Primary Master is not an ATA hard drive\n");
        return false;
    }

    if (!ata_wait_drq()) {
        serial_puts("[ATA] Drive failed DRQ wait on IDENTIFY\n");
        return false;
    }

    // Read 256 words (512 bytes) of identify parameters
    for (int i = 0; i < 256; i++) {
        inw(ATA_PRIMARY_DATA);
    }

    ata_drive_present = true;
    serial_puts("[ATA] Primary Master IDE Hard Drive detected and ready\n");
    return true;
}

bool ata_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer) {
    if (!ata_drive_present || sector_count == 0) return false;

    ata_wait_ready();
    outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ_PIO);

    uint16_t* ptr = (uint16_t*)buffer;
    for (int s = 0; s < sector_count; s++) {
        ata_wait_ready();
        if (!ata_wait_drq()) return false;
        for (int i = 0; i < 256; i++) {
            *ptr++ = inw(ATA_PRIMARY_DATA);
        }
    }
    return true;
}

bool ata_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t* buffer) {
    if (!ata_drive_present || sector_count == 0) return false;

    ata_wait_ready();
    outb(ATA_PRIMARY_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE_PIO);

    const uint16_t* ptr = (const uint16_t*)buffer;
    for (int s = 0; s < sector_count; s++) {
        ata_wait_ready();
        if (!ata_wait_drq()) return false;
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_DATA, *ptr++);
        }
    }
    return true;
}
