#include "gui/gui.h"
#include "gui/apps.h"
#include "drivers/framebuffer.h"
#include "drivers/mouse.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "kernel/pmm.h"
#include "net/net.h"
#include "string.h"
#include "drivers/serial.h"

static gui_window_t windows[MAX_WINDOWS];
static int window_z_order[MAX_WINDOWS];
static int total_windows = 0;
static bool start_menu_open = false;

#define TASKBAR_HEIGHT 40
#define TASKBAR_Y      (fb.height - TASKBAR_HEIGHT)

typedef struct {
    int x;
    int y;
    const char* label;
    const char* icon_glyph;
    uint32_t    icon_bg;
    app_type_t  app;
} desktop_icon_t;

static desktop_icon_t desktop_icons[] = {
    {24, 24,  "Terminal",    ">_", 0x000F172A, APP_TERMINAL},
    {24, 114, "Explorer",    "[]", 0x00D97706, APP_FILEMGR},
    {24, 204, "Text Editor", "# ", 0x002563EB, APP_EDITOR},
    {24, 294, "Calculator",  "+-", 0x00059669, APP_CALC},
    {24, 384, "System Info", "i ", 0x007C3AED, APP_SYSINFO}
};
#define DESKTOP_ICON_COUNT 5

static void init_window(gui_window_t* win, app_type_t app, int id) {
    win->id = id;
    win->app_type = app;
    win->is_open = false;
    win->is_minimized = false;
    win->is_dragging = false;

    switch (app) {
        case APP_TERMINAL:
            strcpy(win->title, "Terminal - Network Diagnostics");
            win->x = 220; win->y = 80; win->width = 640; win->height = 420;
            app_terminal_init(win);
            break;
        case APP_SYSINFO:
            strcpy(win->title, "System Information Dashboard");
            win->x = 180; win->y = 120; win->width = 620; win->height = 360;
            app_sysinfo_init(win);
            break;
        case APP_EDITOR:
            strcpy(win->title, "Text Editor - /home/notes.txt");
            win->x = 260; win->y = 140; win->width = 560; win->height = 380;
            app_editor_init(win);
            break;
        case APP_FILEMGR:
            strcpy(win->title, "File Manager - Virtual File System");
            win->x = 200; win->y = 100; win->width = 620; win->height = 380;
            app_filemgr_init(win);
            break;
        case APP_CALC:
            strcpy(win->title, "Calculator");
            win->x = 340; win->y = 160; win->width = 280; win->height = 340;
            app_calc_init(win);
            break;
        default:
            break;
    }
}

void gui_init(void) {
    total_windows = APP_COUNT;
    for (int i = 0; i < total_windows; i++) {
        init_window(&windows[i], (app_type_t)i, i);
        window_z_order[i] = i;
    }

    // Default open windows: Terminal and System Info
    gui_open_window(APP_TERMINAL);
    gui_open_window(APP_SYSINFO);
    gui_focus_window(&windows[APP_TERMINAL]);

    serial_puts("[GUI] Desktop Compositor & Window Manager initialized\n");
}

gui_window_t* gui_open_window(app_type_t app) {
    if (app < 0 || (int)app >= total_windows) return NULL;
    gui_window_t* win = &windows[app];
    win->is_open = true;
    win->is_minimized = false;
    gui_focus_window(win);
    return win;
}

void gui_close_window(gui_window_t* win) {
    if (win) {
        win->is_open = false;
        win->is_dragging = false;
    }
}

void gui_focus_window(gui_window_t* win) {
    if (!win) return;
    int target_idx = -1;
    for (int i = 0; i < total_windows; i++) {
        if (&windows[window_z_order[i]] == win) {
            target_idx = i;
            break;
        }
    }
    if (target_idx >= 0 && target_idx < total_windows - 1) {
        int win_id = window_z_order[target_idx];
        for (int i = target_idx; i < total_windows - 1; i++) {
            window_z_order[i] = window_z_order[i + 1];
        }
        window_z_order[total_windows - 1] = win_id;
    }
}

static gui_window_t* get_top_window(void) {
    for (int i = total_windows - 1; i >= 0; i--) {
        gui_window_t* win = &windows[window_z_order[i]];
        if (win->is_open && !win->is_minimized) {
            return win;
        }
    }
    return NULL;
}

// ------------------------------------------------------------------------------
// Wallpaper & Desktop Drawing
// ------------------------------------------------------------------------------
static void render_wallpaper(void) {
    // Deep gradient background: Night Indigo to Deep Midnight Blue
    fb_draw_gradient_v(0, 0, fb.width, fb.height - TASKBAR_HEIGHT, 0x000A1128, 0x001C2541);

    // Modern geometric backdrop grid lines
    for (int y = 0; y < (int)(fb.height - TASKBAR_HEIGHT); y += 64) {
        for (int x = 0; x < (int)fb.width; x += 64) {
            fb_putpixel(x, y, 0x00243258);
        }
    }

    // Aesthetic OS Branding Watermark on bottom right of desktop
    fb_draw_string(fb.width - 280, fb.height - TASKBAR_HEIGHT - 36,
                   "AeroOS 1.0  |  Academic OS", 0x00475569, 0, true);
    fb_draw_string(fb.width - 280, fb.height - TASKBAR_HEIGHT - 18,
                   "Intel x86 Bare-Metal Kernel", 0x00334155, 0, true);
}

static void render_desktop_icons(void) {
    for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
        desktop_icon_t* icon = &desktop_icons[i];

        // Highlight icon on mouse hover
        bool hovered = (mouse.x >= icon->x - 4 && mouse.x <= icon->x + 52 &&
                        mouse.y >= icon->y - 4 && mouse.y <= icon->y + 64);

        if (hovered) {
            fb_draw_rect_fill(icon->x - 4, icon->y - 4, 56, 68, 0x001E293B);
            fb_draw_rect(icon->x - 4, icon->y - 4, 56, 68, 0x0038BDF8);
        }

        // Icon Tile (48x48 rounded appearance)
        fb_draw_rect_fill(icon->x, icon->y, 48, 44, icon->icon_bg);
        fb_draw_rect(icon->x, icon->y, 48, 44, 0x00475569);
        fb_draw_string(icon->x + 16, icon->y + 14, icon->icon_glyph, COLOR_WHITE, 0, true);

        // Icon Label
        int label_len = (int)strlen(icon->label);
        int lx = icon->x + 24 - (label_len * 4);
        fb_draw_string(lx, icon->y + 48, icon->label, COLOR_TEXT_LIGHT, 0, true);
    }
}

// ------------------------------------------------------------------------------
// Window Rendering
// ------------------------------------------------------------------------------
static void render_window(gui_window_t* win, bool is_active) {
    if (!win->is_open || win->is_minimized) return;

    // Window Outer Border & Shadow
    fb_draw_rect_fill(win->x, win->y, win->width, win->height, 0x00181A20);
    fb_draw_rect(win->x, win->y, win->width, win->height, is_active ? 0x000A84FF : 0x00333842);

    // Titlebar (26px height)
    uint32_t title_top = is_active ? 0x001E293B : 0x00141A24;
    uint32_t title_bot = is_active ? 0x000F172A : 0x000F141C;
    fb_draw_gradient_v(win->x + 1, win->y + 1, win->width - 2, 25, title_top, title_bot);
    fb_draw_line(win->x + 1, win->y + 26, win->x + win->width - 2, win->y + 26, 0x00334155);

    // Titlebar text
    fb_draw_string(win->x + 12, win->y + 5, win->title, is_active ? COLOR_WHITE : 0x0094A3B8, 0, true);

    // Close Button [X] at top right
    int btn_x = win->x + win->width - 24;
    int btn_y = win->y + 4;
    fb_draw_rect_fill(btn_x, btn_y, 18, 18, 0x00EF4444);
    fb_draw_string(btn_x + 5, btn_y + 1, "x", COLOR_WHITE, 0, true);

    // Client Area
    if (win->render_client) {
        win->render_client(win);
    }
}

// ------------------------------------------------------------------------------
// Taskbar & Start Menu Rendering
// ------------------------------------------------------------------------------
static void render_taskbar(void) {
    // Taskbar bar gradient
    fb_draw_gradient_v(0, TASKBAR_Y, fb.width, TASKBAR_HEIGHT, 0x00182030, 0x000F141F);
    fb_draw_line(0, TASKBAR_Y, fb.width, TASKBAR_Y, 0x00334155);

    // Start Button [ Aero ]
    uint32_t start_col = start_menu_open ? 0x002563EB : 0x000A84FF;
    fb_draw_rect_fill(8, TASKBAR_Y + 5, 84, 30, start_col);
    fb_draw_rect(8, TASKBAR_Y + 5, 84, 30, 0x0060A5FA);
    fb_draw_string(20, TASKBAR_Y + 12, "* Aero", COLOR_WHITE, 0, true);

    // Taskbar Open Windows Pills
    int tab_x = 104;
    for (int i = 0; i < total_windows; i++) {
        gui_window_t* win = &windows[i];
        if (!win->is_open) continue;

        bool is_top = (win == get_top_window());
        uint32_t tab_bg = is_top ? 0x002563EB : 0x001E293B;

        fb_draw_rect_fill(tab_x, TASKBAR_Y + 6, 120, 28, tab_bg);
        fb_draw_rect(tab_x, TASKBAR_Y + 6, 120, 28, is_top ? 0x0038BDF8 : 0x00334155);

        // Truncate title to fit pill
        char short_title[14];
        strncpy(short_title, win->title, 13);
        short_title[13] = '\0';
        fb_draw_string(tab_x + 8, TASKBAR_Y + 12, short_title, COLOR_WHITE, 0, true);

        tab_x += 126;
    }

    // System Tray (Right Side)
    int tray_x = fb.width - 320;

    // Ethernet status
    fb_draw_rect_fill(tray_x, TASKBAR_Y + 14, 8, 8, 0x0022C55E); // Green connection dot
    char ip_str[20];
    ip_to_str(net_cfg.ip, ip_str);
    fb_draw_string(tray_x + 14, TASKBAR_Y + 12, ip_str, 0x00E2E8F0, 0, true);

    // Memory status
    char mem_str[32];
    snprintf(mem_str, sizeof(mem_str), "RAM: %dMB", pmm_get_used_memory() / (1024 * 1024));
    fb_draw_string(tray_x + 124, TASKBAR_Y + 12, mem_str, 0x0094A3B8, 0, true);

    // Digital Clock (HH:MM:SS from system timer ticks)
    uint32_t sec_total = timer_get_uptime_sec();
    uint32_t hours = (sec_total / 3600) % 24;
    uint32_t mins  = (sec_total / 60) % 60;
    uint32_t secs  = sec_total % 60;
    char clock_str[16];
    snprintf(clock_str, sizeof(clock_str), "%02d:%02d:%02d", hours, mins, secs);
    fb_draw_rect_fill(fb.width - 86, TASKBAR_Y + 6, 76, 28, 0x00111827);
    fb_draw_rect(fb.width - 86, TASKBAR_Y + 6, 76, 28, 0x00374151);
    fb_draw_string(fb.width - 76, TASKBAR_Y + 12, clock_str, 0x0038BDF8, 0, true);
}

static void render_start_menu(void) {
    if (!start_menu_open) return;

    int sm_x = 8;
    int sm_w = 230;
    int sm_h = 240;
    int sm_y = TASKBAR_Y - sm_h - 4;

    fb_draw_rect_fill(sm_x, sm_y, sm_w, sm_h, 0x00111827);
    fb_draw_rect(sm_x, sm_y, sm_w, sm_h, 0x000A84FF);

    // Header
    fb_draw_rect_fill(sm_x + 1, sm_y + 1, sm_w - 2, 44, 0x001F2937);
    fb_draw_rect_fill(sm_x + 10, sm_y + 8, 28, 28, 0x000A84FF);
    fb_draw_string(sm_x + 18, sm_y + 14, "A", COLOR_WHITE, 0, true);
    fb_draw_string(sm_x + 46, sm_y + 10, "AeroOS Applications", COLOR_WHITE, 0, true);
    fb_draw_string(sm_x + 46, sm_y + 26, "Academic Kernel System", 0x0094A3B8, 0, true);
    fb_draw_line(sm_x + 1, sm_y + 45, sm_x + sm_w - 2, sm_y + 45, 0x00374151);

    // Application List Items
    const char* items[] = {
        ">_ Terminal & Network Shell",
        "[] File Manager & VFS",
        "#  Text / Code Editor",
        "+- Calculator",
        "i  System Information"
    };

    int iy = sm_y + 54;
    for (int i = 0; i < 5; i++) {
        bool hover = (mouse.x >= sm_x + 6 && mouse.x <= sm_x + sm_w - 6 &&
                      mouse.y >= iy - 2 && mouse.y <= iy + 22);

        if (hover) {
            fb_draw_rect_fill(sm_x + 6, iy - 2, sm_w - 12, 24, 0x002563EB);
        }
        fb_draw_string(sm_x + 14, iy + 2, items[i], COLOR_WHITE, 0, true);
        iy += 34;
    }
}

// ------------------------------------------------------------------------------
// Input Event Handling (Mouse & Keyboard)
// ------------------------------------------------------------------------------
void gui_handle_input(void) {
    gui_window_t* top_win = get_top_window();

    // 1. Keyboard Input -> Relay to active window
    while (keyboard_has_char()) {
        char ch = keyboard_getchar();
        if (top_win && top_win->handle_key) {
            top_win->handle_key(top_win, ch);
        }
    }

    // 2. Mouse Dragging
    if (top_win && top_win->is_dragging) {
        if (mouse.left_button) {
            top_win->x = mouse.x - top_win->drag_off_x;
            top_win->y = mouse.y - top_win->drag_off_y;

            // Clamp window to screen bounds
            if (top_win->x < 0) top_win->x = 0;
            if (top_win->x > (int)fb.width - 80) top_win->x = (int)fb.width - 80;
            if (top_win->y < 0) top_win->y = 0;
            if (top_win->y > (int)TASKBAR_Y - 30) top_win->y = (int)TASKBAR_Y - 30;
        } else {
            top_win->is_dragging = false;
        }
    }

    // 3. Mouse Clicks
    if (mouse.left_clicked) {
        // Check Start Menu items click
        if (start_menu_open) {
            int sm_x = 8;
            int sm_w = 230;
            int sm_h = 240;
            int sm_y = TASKBAR_Y - sm_h - 4;

            if (mouse.x >= sm_x && mouse.x <= sm_x + sm_w &&
                mouse.y >= sm_y && mouse.y <= sm_y + sm_h) {
                int item_idx = (mouse.y - (sm_y + 54)) / 34;
                if (item_idx >= 0 && item_idx < APP_COUNT) {
                    gui_open_window((app_type_t)item_idx);
                    start_menu_open = false;
                    return;
                }
            } else {
                start_menu_open = false;
            }
        }

        // Check Start Button click
        if (mouse.x >= 8 && mouse.x <= 92 && mouse.y >= (int)TASKBAR_Y + 5 && mouse.y <= (int)TASKBAR_Y + 35) {
            start_menu_open = !start_menu_open;
            return;
        }

        // Check Taskbar Window Tabs click
        if (mouse.y >= (int)TASKBAR_Y) {
            int tab_x = 104;
            for (int i = 0; i < total_windows; i++) {
                gui_window_t* win = &windows[i];
                if (!win->is_open) continue;

                if (mouse.x >= tab_x && mouse.x <= tab_x + 120) {
                    if (win == top_win && !win->is_minimized) {
                        win->is_minimized = true;
                    } else {
                        win->is_minimized = false;
                        gui_focus_window(win);
                    }
                    return;
                }
                tab_x += 126;
            }
            return;
        }

        // Check Windows (from top to bottom)
        bool window_clicked = false;
        for (int i = total_windows - 1; i >= 0; i--) {
            gui_window_t* win = &windows[window_z_order[i]];
            if (!win->is_open || win->is_minimized) continue;

            if (mouse.x >= win->x && mouse.x <= win->x + win->width &&
                mouse.y >= win->y && mouse.y <= win->y + win->height) {

                gui_focus_window(win);
                window_clicked = true;

                // Close Button [X]
                int btn_x = win->x + win->width - 24;
                int btn_y = win->y + 4;
                if (mouse.x >= btn_x && mouse.x <= btn_x + 18 &&
                    mouse.y >= btn_y && mouse.y <= btn_y + 18) {
                    gui_close_window(win);
                    return;
                }

                // Titlebar Dragging
                if (mouse.y <= win->y + 26) {
                    win->is_dragging = true;
                    win->drag_off_x = mouse.x - win->x;
                    win->drag_off_y = mouse.y - win->y;
                    return;
                }

                // Client Area Click
                if (win->handle_click) {
                    win->handle_click(win, mouse.x - win->x, mouse.y - win->y);
                }
                return;
            }
        }

        // Check Desktop Icons click (if no window was clicked)
        if (!window_clicked) {
            for (int i = 0; i < DESKTOP_ICON_COUNT; i++) {
                desktop_icon_t* icon = &desktop_icons[i];
                if (mouse.x >= icon->x - 4 && mouse.x <= icon->x + 52 &&
                    mouse.y >= icon->y - 4 && mouse.y <= icon->y + 64) {
                    gui_open_window(icon->app);
                    return;
                }
            }
        }
    }
}

void gui_render_frame(void) {
    // 1. Background Wallpaper
    render_wallpaper();

    // 2. Desktop Icons
    render_desktop_icons();

    // 3. Application Windows (z-ordered)
    gui_window_t* top_win = get_top_window();
    for (int i = 0; i < total_windows; i++) {
        gui_window_t* win = &windows[window_z_order[i]];
        render_window(win, win == top_win);
    }

    // 4. Taskbar (Dock)
    render_taskbar();

    // 5. Start Menu (if open)
    render_start_menu();

    // 6. Modern Sleek Mouse Cursor
    fb_draw_cursor(mouse.x, mouse.y);

    // 7. Blit double buffer to frontbuffer
    fb_flip();
}
