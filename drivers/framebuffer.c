#include "drivers/framebuffer.h"
#include "font.h"
#include "string.h"
#include "drivers/serial.h"

framebuffer_t fb = {0};

// Static pointer for double buffer in conventional high-low RAM
#define BACKBUFFER_ADDR 0x00800000

bool fb_init(multiboot_info_t* mboot) {
    if (!mboot || !(mboot->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        serial_puts("[FB] Multiboot framebuffer flag NOT present!\n");
        return false;
    }

    fb.frontbuffer   = (uint32_t*)(uint32_t)mboot->framebuffer_addr;
    fb.width         = mboot->framebuffer_width;
    fb.height        = mboot->framebuffer_height;
    fb.pitch         = mboot->framebuffer_pitch;
    fb.pitch_pixels  = mboot->framebuffer_pitch / 4;
    fb.bpp           = mboot->framebuffer_bpp;
    fb.backbuffer    = (uint32_t*)BACKBUFFER_ADDR;
    fb.is_initialized = true;

    serial_printf("[FB] Framebuffer initialized: %dx%d, %d bpp, pitch=%d, front=%p, back=%p\n",
                  fb.width, fb.height, fb.bpp, fb.pitch, fb.frontbuffer, fb.backbuffer);

    // Clear backbuffer to deep dark desktop theme
    fb_clear(COLOR_WINDOW_BG);
    fb_flip();
    return true;
}

void fb_flip(void) {
    if (!fb.is_initialized || !fb.frontbuffer || !fb.backbuffer) return;
    uint32_t total_pixels = fb.pitch_pixels * fb.height;
    uint32_t* src = fb.backbuffer;
    uint32_t* dst = fb.frontbuffer;

    for (uint32_t i = 0; i < total_pixels; i++) {
        dst[i] = src[i];
    }
}

void fb_clear(uint32_t color) {
    if (!fb.is_initialized) return;
    uint32_t total_pixels = fb.pitch_pixels * fb.height;
    uint32_t* buf = fb.backbuffer;
    for (uint32_t i = 0; i < total_pixels; i++) {
        buf[i] = color;
    }
}

inline void fb_putpixel(int x, int y, uint32_t color) {
    if (!fb.is_initialized) return;
    if (x < 0 || x >= (int)fb.width || y < 0 || y >= (int)fb.height) return;
    fb.backbuffer[y * fb.pitch_pixels + x] = color;
}

inline uint32_t fb_getpixel(int x, int y) {
    if (!fb.is_initialized) return 0;
    if (x < 0 || x >= (int)fb.width || y < 0 || y >= (int)fb.height) return 0;
    return fb.backbuffer[y * fb.pitch_pixels + x];
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        fb_putpixel(x + i, y, color);
        fb_putpixel(x + i, y + h - 1, color);
    }
    for (int j = 0; j < h; j++) {
        fb_putpixel(x, y + j, color);
        fb_putpixel(x + w - 1, y + j, color);
    }
}

void fb_draw_rect_fill(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w) > (int)fb.width  ? (int)fb.width  : (x + w);
    int y2 = (y + h) > (int)fb.height ? (int)fb.height : (y + h);

    for (int cy = y1; cy < y2; cy++) {
        uint32_t* line = &fb.backbuffer[cy * fb.pitch_pixels + x1];
        int count = x2 - x1;
        for (int cx = 0; cx < count; cx++) {
            line[cx] = color;
        }
    }
}

static inline uint32_t lerp_color(uint32_t c1, uint32_t c2, int step, int total) {
    if (total <= 0) return c1;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;

    int r = r1 + ((r2 - r1) * step) / total;
    int g = g1 + ((g2 - g1) * step) / total;
    int b = b1 + ((b2 - b1) * step) / total;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void fb_draw_gradient_v(int x, int y, int w, int h, uint32_t top_col, uint32_t bot_col) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < h; i++) {
        uint32_t row_color = lerp_color(top_col, bot_col, i, h);
        fb_draw_rect_fill(x, y + i, w, 1, row_color);
    }
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    while (1) {
        fb_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent) {
    uint8_t ch = (uint8_t)c;
    const uint8_t* glyph = font8x16[ch];

    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                fb_putpixel(x + col, y + row, fg);
            } else if (!transparent) {
                fb_putpixel(x + col, y + row, bg);
            }
        }
    }
}

void fb_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg, bool transparent) {
    if (!str) return;
    int cx = x;
    int cy = y;

    while (*str) {
        if (*str == '\n') {
            cy += 18;
            cx = x;
        } else if (*str == '\r') {
            cx = x;
        } else if (*str == '\t') {
            cx += 8 * 4;
        } else {
            fb_draw_char(cx, cy, *str, fg, bg, transparent);
            cx += 8;
        }
        str++;
    }
}

// 12x19 sleek arrow cursor bitmap:
// 'X' = border (black 0x00000000)
// '#' = body (white 0x00FFFFFF)
// '.' = shadow (transparent dark 0x00202020)
// ' ' = none
static const char* cursor_bitmap[19] = {
    "X           ",
    "XX          ",
    "X#X         ",
    "X##X        ",
    "X###X       ",
    "X####X      ",
    "X#####X     ",
    "X######X    ",
    "X#######X   ",
    "X########X  ",
    "X#####XXXXX ",
    "X##X##X     ",
    "X#X X##X    ",
    "XX   X##X   ",
    "X     X##X  ",
    "       X##X ",
    "        XX  ",
    "            ",
    "            "
};

void fb_draw_cursor(int x, int y) {
    for (int row = 0; row < 19; row++) {
        const char* line = cursor_bitmap[row];
        for (int col = 0; col < 12; col++) {
            char pixel = line[col];
            int px = x + col;
            int py = y + row;
            if (pixel == 'X') {
                fb_putpixel(px, py, 0x00000000);
            } else if (pixel == '#') {
                fb_putpixel(px, py, 0x00FFFFFF);
            } else if (pixel == '.') {
                fb_putpixel(px, py, 0x00222222);
            }
        }
    }
}
