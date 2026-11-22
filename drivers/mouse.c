#include "drivers/mouse.h"
#include "kernel/idt.h"
#include "io.h"
#include "drivers/serial.h"

mouse_state_t mouse = {0};

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3] = {0};
static int screen_w = 1024;
static int screen_h = 768;
static bool prev_left_btn = false;
static bool prev_right_btn = false;

static inline void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while ((inb(0x64) & 2) && --timeout);
}

static inline void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (!(inb(0x64) & 1) && --timeout);
}

static void mouse_write(uint8_t write_val) {
    mouse_wait_write();
    outb(0x64, 0xD4); // Tell controller to route next byte to auxiliary mouse
    mouse_wait_write();
    outb(0x60, write_val);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

static void mouse_callback(registers_t* regs) {
    (void)regs;
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) {
        // Data not from mouse
        return;
    }

    uint8_t b = inb(0x60);

    if (mouse_cycle == 0) {
        // Bit 3 must be 1 for valid first packet byte
        if ((b & 0x08) == 0x08) {
            mouse_bytes[0] = b;
            mouse_cycle = 1;
        }
    } else if (mouse_cycle == 1) {
        mouse_bytes[1] = b;
        mouse_cycle = 2;
    } else if (mouse_cycle == 2) {
        mouse_bytes[2] = b;
        mouse_cycle = 0;

        uint8_t flags = mouse_bytes[0];
        int dx = (int)mouse_bytes[1];
        int dy = (int)mouse_bytes[2];

        // Sign extension
        if (flags & 0x10) dx |= 0xFFFFFF00;
        if (flags & 0x20) dy |= 0xFFFFFF00;

        // Discard overflow packets
        if (flags & 0xC0) return;

        mouse.x += dx;
        mouse.y -= dy; // Invert Y for screen coordinates

        if (mouse.x < 0) mouse.x = 0;
        if (mouse.x >= screen_w) mouse.x = screen_w - 1;
        if (mouse.y < 0) mouse.y = 0;
        if (mouse.y >= screen_h) mouse.y = screen_h - 1;

        bool curr_left = (flags & 0x01) != 0;
        bool curr_right = (flags & 0x02) != 0;

        mouse.left_clicked = (!prev_left_btn && curr_left);
        mouse.right_clicked = (!prev_right_btn && curr_right);

        mouse.left_button = curr_left;
        mouse.right_button = curr_right;
        mouse.middle_button = (flags & 0x04) != 0;

        prev_left_btn = curr_left;
        prev_right_btn = curr_right;
    }
}

void mouse_init(void) {
    mouse.x = screen_w / 2;
    mouse.y = screen_h / 2;

    // Enable auxiliary mouse device
    mouse_wait_write();
    outb(0x64, 0xA8);

    // Read Controller Configuration Byte
    mouse_wait_write();
    outb(0x64, 0x20);
    uint8_t comp = mouse_read();

    // Enable mouse interrupt (bit 1) and enable clock (clear bit 5)
    comp |= 0x02;
    comp &= ~0x20;

    // Write back Controller Configuration Byte
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, comp);

    // Set mouse to defaults
    mouse_write(0xF6);
    mouse_read(); // ACK (0xFA)

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read(); // ACK (0xFA)

    register_interrupt_handler(44, mouse_callback);
    serial_printf("[MOUSE] PS/2 Mouse initialized at (%d, %d), IRQ 12 / Vector 44\n", mouse.x, mouse.y);
}

void mouse_update_bounds(int width, int height) {
    screen_w = width;
    screen_h = height;
}
