#include "drivers/timer.h"
#include "kernel/idt.h"
#include "io.h"
#include "drivers/serial.h"

static volatile uint32_t ticks = 0;
static uint32_t tick_freq = 100;

static void timer_callback(registers_t* regs) {
    (void)regs;
    ticks++;
}

void timer_init(uint32_t frequency) {
    tick_freq = frequency;
    register_interrupt_handler(32, timer_callback);

    uint32_t divisor = 1193180 / frequency;

    // Send command byte (channel 0, access mode low/high byte, rate generator mode)
    outb(0x43, 0x36);

    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);

    serial_printf("[PIT] Timer initialized at %d Hz (divisor=%d)\n", frequency, divisor);
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

uint32_t timer_get_uptime_sec(void) {
    return ticks / tick_freq;
}

void timer_sleep_ms(uint32_t ms) {
    uint32_t start_ticks = ticks;
    uint32_t ticks_to_wait = (ms * tick_freq) / 1000;
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    while ((ticks - start_ticks) < ticks_to_wait) {
        __asm__ volatile ("hlt");
    }
}
