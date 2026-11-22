#include "drivers/keyboard.h"
#include "kernel/idt.h"
#include "io.h"
#include "drivers/serial.h"

#define KBD_BUFFER_SIZE 256
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static bool shift_pressed = false;
static bool caps_lock = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;

// Standard US QWERTY Scancode Set 1 (non-shifted)
static const char kbd_us_nomod[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, /* Right Shift */
    '*',
    0,  /* Alt */
    ' ', /* Space */
    0,  /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1 - F10 */
    0,  /* Num lock */
    0,  /* Scroll Lock */
    0,  /* Home */
    KEY_UP, /* Up Arrow */
    0,  /* Page Up */
    '-',
    KEY_LEFT, /* Left Arrow */
    0,
    KEY_RIGHT, /* Right Arrow */
    '+',
    0,  /* End */
    KEY_DOWN, /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert */
    0,  /* Delete */
    0, 0, 0,
    0,  /* F11 */
    0,  /* F12 */
    0
};

// Standard US QWERTY Scancode Set 1 (shifted)
static const char kbd_us_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, /* Ctrl */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, /* Left Shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0, /* Right Shift */
    '*',
    0,  /* Alt */
    ' ', /* Space */
    0,  /* Caps lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* F1 - F10 */
    0, 0, 0, KEY_UP, 0, '-', KEY_LEFT, 0, KEY_RIGHT, '+', 0, KEY_DOWN, 0, 0, 0,
    0, 0, 0, 0, 0, 0
};

static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    // Key releases (scancode & 0x80)
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) { // Left/Right Shift
            shift_pressed = false;
        } else if (released == 0x1D) {             // Ctrl
            ctrl_pressed = false;
        } else if (released == 0x38) {             // Alt
            alt_pressed = false;
        }
        return;
    }

    // Key presses
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    } else if (scancode == 0x1D) {
        ctrl_pressed = true;
        return;
    } else if (scancode == 0x38) {
        alt_pressed = true;
        return;
    } else if (scancode == 0x3A) { // CapsLock toggle
        caps_lock = !caps_lock;
        return;
    }

    if (scancode < 128) {
        char ch = 0;
        bool use_shifted = shift_pressed;
        if (caps_lock) {
            char base = kbd_us_nomod[scancode];
            if (base >= 'a' && base <= 'z') {
                use_shifted = !use_shifted;
            }
        }

        if (use_shifted) {
            ch = kbd_us_shifted[scancode];
        } else {
            ch = kbd_us_nomod[scancode];
        }

        if (ch != 0) {
            int next = (kbd_head + 1) % KBD_BUFFER_SIZE;
            if (next != kbd_tail) {
                kbd_buffer[kbd_head] = ch;
                kbd_head = next;
            }
        }
    }
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_callback);
    serial_puts("[KBD] PS/2 Keyboard initialized (IRQ 1 / Vector 33)\n");
}

bool keyboard_has_char(void) {
    return kbd_head != kbd_tail;
}

char keyboard_getchar(void) {
    if (kbd_head == kbd_tail) return 0;
    char ch = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return ch;
}
