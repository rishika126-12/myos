#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"
#include "multiboot.h"

#define COLOR_BLACK         0x00000000
#define COLOR_WHITE         0x00FFFFFF
#define COLOR_RED           0x00FF3B30
#define COLOR_GREEN         0x0034C759
#define COLOR_BLUE          0x00007AFF
#define COLOR_YELLOW        0x00FFCC00
#define COLOR_CYAN          0x0032ADE6
#define COLOR_PURPLE        0x00AF52DE
#define COLOR_ORANGE        0x00FF9500
#define COLOR_DARK_GRAY     0x001C1C1E
#define COLOR_MID_GRAY      0x002C2C2E
#define COLOR_LIGHT_GRAY    0x0048484A
#define COLOR_TEXT_LIGHT    0x00F2F2F7
#define COLOR_TEXT_DIM      0x008E8E93
#define COLOR_ACCENT        0x000A84FF
#define COLOR_TASKBAR       0x0014161B
#define COLOR_WINDOW_BG     0x00181A20
#define COLOR_WINDOW_TITLE  0x00232730
#define COLOR_WINDOW_BORDER 0x00333842
#define COLOR_DESKTOP_TOP   0x000B132B
#define COLOR_DESKTOP_BOT   0x001C2541

typedef struct {
    uint32_t* frontbuffer;
    uint32_t* backbuffer;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;       // bytes per line
    uint32_t  pitch_pixels;
    uint8_t   bpp;
    bool      is_initialized;
} framebuffer_t;

extern framebuffer_t fb;

bool fb_init(multiboot_info_t* mboot);
void fb_flip(void);
void fb_clear(uint32_t color);

void fb_putpixel(int x, int y, uint32_t color);
uint32_t fb_getpixel(int x, int y);

void fb_draw_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_rect_fill(int x, int y, int w, int h, uint32_t color);
void fb_draw_gradient_v(int x, int y, int w, int h, uint32_t top_col, uint32_t bot_col);
void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color);

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, bool transparent);
void fb_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg, bool transparent);
void fb_draw_cursor(int x, int y);

#endif
