#include "gui/apps.h"
#include "drivers/framebuffer.h"
#include "kernel/pmm.h"
#include "kernel/heap.h"
#include "drivers/timer.h"
#include "drivers/pci.h"
#include "drivers/e1000.h"
#include "net/net.h"
#include "fs/vfs.h"
#include "string.h"

// ==============================================================================
// 1. TERMINAL APP
// ==============================================================================
#define TERM_MAX_LINES 16
#define TERM_LINE_LEN  64

static char term_lines[TERM_MAX_LINES][TERM_LINE_LEN];
static int  term_line_count = 0;
static char term_input[TERM_LINE_LEN] = {0};
static int  term_input_len = 0;

static void term_add_line(const char* text) {
    if (term_line_count < TERM_MAX_LINES) {
        strncpy(term_lines[term_line_count++], text, TERM_LINE_LEN - 1);
    } else {
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            strncpy(term_lines[i], term_lines[i + 1], TERM_LINE_LEN - 1);
        }
        strncpy(term_lines[TERM_MAX_LINES - 1], text, TERM_LINE_LEN - 1);
    }
}

static void term_execute_cmd(const char* cmd) {
    char out_buf[128];
    if (strcmp(cmd, "help") == 0) {
        term_add_line("Available commands:");
        term_add_line("  help, ifconfig, ip, ping <ip>, ls, cat <file>");
        term_add_line("  lspci, mem, uptime, clear, echo <text>");
    } else if (strcmp(cmd, "clear") == 0) {
        term_line_count = 0;
    } else if (strcmp(cmd, "uptime") == 0) {
        snprintf(out_buf, sizeof(out_buf), "Uptime: %d seconds (%d ticks)",
                 timer_get_uptime_sec(), timer_get_ticks());
        term_add_line(out_buf);
    } else if (strcmp(cmd, "ifconfig") == 0 || strcmp(cmd, "ip") == 0) {
        char ip_str[20], gw_str[20], nm_str[20];
        ip_to_str(net_cfg.ip, ip_str);
        ip_to_str(net_cfg.gateway, gw_str);
        ip_to_str(net_cfg.netmask, nm_str);
        snprintf(out_buf, sizeof(out_buf), "eth0: inet %s netmask %s gw %s", ip_str, nm_str, gw_str);
        term_add_line(out_buf);
        snprintf(out_buf, sizeof(out_buf), "      MAC: %02x:%02x:%02x:%02x:%02x:%02x (DHCP: %s)",
                 e1000_nic.mac[0], e1000_nic.mac[1], e1000_nic.mac[2],
                 e1000_nic.mac[3], e1000_nic.mac[4], e1000_nic.mac[5],
                 net_cfg.dhcp_bound ? "active" : "preset");
        term_add_line(out_buf);
        snprintf(out_buf, sizeof(out_buf), "      RX packets: %d, TX packets: %d",
                 net_cfg.rx_packets, net_cfg.tx_packets);
        term_add_line(out_buf);
    } else if (strncmp(cmd, "ping ", 5) == 0) {
        const char* target = cmd + 5;
        snprintf(out_buf, sizeof(out_buf), "PING %s 56(84) bytes of data.", target);
        term_add_line(out_buf);
        char ping_res[128];
        net_ping(target, ping_res, sizeof(ping_res));
        term_add_line(ping_res);
    } else if (strcmp(cmd, "lspci") == 0) {
        term_add_line("PCI Bus Devices:");
        for (int i = 0; i < pci_device_count && i < 5; i++) {
            snprintf(out_buf, sizeof(out_buf), " %02x:%02x.%d: %04x:%04x (Class %02x.%02x, IRQ %d)",
                     pci_devices[i].bus, pci_devices[i].slot, pci_devices[i].func,
                     pci_devices[i].vendor_id, pci_devices[i].device_id,
                     pci_devices[i].class_id, pci_devices[i].subclass_id, pci_devices[i].irq);
            term_add_line(out_buf);
        }
    } else if (strcmp(cmd, "mem") == 0) {
        snprintf(out_buf, sizeof(out_buf), "Physical RAM: %d MB total, %d MB free, %d MB used",
                 pmm_get_total_memory() / (1024 * 1024),
                 pmm_get_free_memory() / (1024 * 1024),
                 pmm_get_used_memory() / (1024 * 1024));
        term_add_line(out_buf);
        snprintf(out_buf, sizeof(out_buf), "Kernel Heap: %d KB dynamically allocated",
                 heap_get_used_bytes() / 1024);
        term_add_line(out_buf);
    } else if (strcmp(cmd, "ls") == 0) {
        term_add_line("Directory listing of /:");
        for (int i = 0; i < vfs_get_file_count(); i++) {
            vfs_node_t* node = vfs_get_node_by_index(i);
            snprintf(out_buf, sizeof(out_buf), "  %-6s %6d B  %s",
                     node->is_dir ? "<DIR>" : "<FILE>", (int)node->size, node->path);
            term_add_line(out_buf);
        }
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        const char* path = cmd + 4;
        char file_buf[256];
        int r = vfs_read_file(path, file_buf, sizeof(file_buf));
        if (r >= 0) {
            // Print up to 2 lines
            char* line = file_buf;
            for (int i = 0; i < r; i++) {
                if (file_buf[i] == '\n') {
                    file_buf[i] = '\0';
                    term_add_line(line);
                    line = &file_buf[i + 1];
                }
            }
            if (*line) term_add_line(line);
        } else {
            snprintf(out_buf, sizeof(out_buf), "cat: %s: No such file", path);
            term_add_line(out_buf);
        }
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_add_line(cmd + 5);
    } else if (cmd[0] != '\0') {
        snprintf(out_buf, sizeof(out_buf), "aero: command not found: %s", cmd);
        term_add_line(out_buf);
    }
}

static void term_render(gui_window_t* win) {
    int cx = win->x + 6;
    int cy = win->y + 28;
    int cw = win->width - 12;
    int ch = win->height - 34;

    // Dark console background
    fb_draw_rect_fill(cx, cy, cw, ch, 0x000F141C);
    fb_draw_rect(cx, cy, cw, ch, 0x001E293B);

    int line_y = cy + 6;
    for (int i = 0; i < term_line_count; i++) {
        fb_draw_string(cx + 8, line_y, term_lines[i], 0x00E2E8F0, 0, true);
        line_y += 18;
    }

    // Input prompt line
    fb_draw_string(cx + 8, line_y, "aero@kernel:~$ ", 0x0038BDF8, 0, true);
    fb_draw_string(cx + 8 + 15 * 8, line_y, term_input, 0x00FFFFFF, 0, true);

    // Blinking cursor
    if ((timer_get_ticks() / 30) % 2 == 0) {
        fb_draw_rect_fill(cx + 8 + (15 + term_input_len) * 8, line_y, 8, 16, 0x0038BDF8);
    }
}

static void term_key(gui_window_t* win, char key) {
    (void)win;
    if (key == '\n') {
        char prompt_echo[TERM_LINE_LEN + 20];
        snprintf(prompt_echo, sizeof(prompt_echo), "aero@kernel:~$ %s", term_input);
        term_add_line(prompt_echo);
        term_execute_cmd(term_input);
        term_input[0] = '\0';
        term_input_len = 0;
    } else if (key == '\b') {
        if (term_input_len > 0) {
            term_input[--term_input_len] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        if (term_input_len < TERM_LINE_LEN - 1) {
            term_input[term_input_len++] = key;
            term_input[term_input_len] = '\0';
        }
    }
}

void app_terminal_init(gui_window_t* win) {
    win->render_client = term_render;
    win->handle_key = term_key;
    win->handle_click = NULL;
    if (term_line_count == 0) {
        term_add_line("AeroOS Diagnostic Shell v1.0 [x86 Protected Mode]");
        term_add_line("Type 'help' for commands, 'ifconfig' for network status.");
        term_add_line("Try 'ping 10.0.2.2' to test gateway connectivity.");
    }
}

// ==============================================================================
// 2. SYSTEM INFORMATION APP
// ==============================================================================
static void sysinfo_render(gui_window_t* win) {
    int cx = win->x + 8;
    int cy = win->y + 30;

    fb_draw_rect_fill(cx, cy, win->width - 16, win->height - 38, 0x00131824);
    fb_draw_rect(cx, cy, win->width - 16, win->height - 38, 0x002D3748);

    // Header Badge
    fb_draw_rect_fill(cx + 12, cy + 12, 60, 60, 0x000A84FF);
    fb_draw_string(cx + 24, cy + 32, "AERO", COLOR_WHITE, 0, true);

    fb_draw_string(cx + 84, cy + 18, "AeroOS 1.0 (Academic Edition)", 0x0060A5FA, 0, true);
    fb_draw_string(cx + 84, cy + 38, "Custom 32-bit Freestanding Operating System", COLOR_TEXT_LIGHT, 0, true);
    fb_draw_string(cx + 84, cy + 56, "DeepMind Architecture & Modern UI Compositor", COLOR_TEXT_DIM, 0, true);

    // Divider
    fb_draw_line(cx + 12, cy + 82, cx + win->width - 28, cy + 82, 0x00334155);

    int y = cy + 96;
    char buf[128];

    // Hardware Specs Grid
    fb_draw_string(cx + 16, y, "CPU & Architecture:", 0x0038BDF8, 0, true);
    fb_draw_string(cx + 200, y, "Intel x86 (IA-32) Protected Mode, Ring 0", COLOR_TEXT_LIGHT, 0, true);
    y += 24;

    fb_draw_string(cx + 16, y, "Display Resolution:", 0x0038BDF8, 0, true);
    snprintf(buf, sizeof(buf), "%d x %d @ %d bpp (VBE Linear Framebuffer)", fb.width, fb.height, fb.bpp);
    fb_draw_string(cx + 200, y, buf, COLOR_TEXT_LIGHT, 0, true);
    y += 24;

    fb_draw_string(cx + 16, y, "Physical Memory:", 0x0038BDF8, 0, true);
    snprintf(buf, sizeof(buf), "%d MB Total (%d MB Free, %d MB Used)",
             pmm_get_total_memory() / (1024 * 1024),
             pmm_get_free_memory() / (1024 * 1024),
             pmm_get_used_memory() / (1024 * 1024));
    fb_draw_string(cx + 200, y, buf, COLOR_TEXT_LIGHT, 0, true);
    y += 24;

    fb_draw_string(cx + 16, y, "Kernel Dynamic Heap:", 0x0038BDF8, 0, true);
    snprintf(buf, sizeof(buf), "%d KB dynamically active", heap_get_used_bytes() / 1024);
    fb_draw_string(cx + 200, y, buf, COLOR_TEXT_LIGHT, 0, true);
    y += 24;

    fb_draw_string(cx + 16, y, "Network Interface:", 0x0038BDF8, 0, true);
    char ip_str[20];
    ip_to_str(net_cfg.ip, ip_str);
    snprintf(buf, sizeof(buf), "Intel PRO/1000 MT (IP: %s)", ip_str);
    fb_draw_string(cx + 200, y, buf, COLOR_TEXT_LIGHT, 0, true);
    y += 24;

    fb_draw_string(cx + 16, y, "Hardware MAC Addr:", 0x0038BDF8, 0, true);
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             e1000_nic.mac[0], e1000_nic.mac[1], e1000_nic.mac[2],
             e1000_nic.mac[3], e1000_nic.mac[4], e1000_nic.mac[5]);
    fb_draw_string(cx + 200, y, buf, COLOR_TEXT_LIGHT, 0, true);
    y += 24;

    fb_draw_string(cx + 16, y, "System Uptime:", 0x0038BDF8, 0, true);
    snprintf(buf, sizeof(buf), "%d seconds (%d timer ticks at 100Hz)", timer_get_uptime_sec(), timer_get_ticks());
    fb_draw_string(cx + 200, y, buf, COLOR_TEXT_LIGHT, 0, true);
}

void app_sysinfo_init(gui_window_t* win) {
    win->render_client = sysinfo_render;
    win->handle_key = NULL;
    win->handle_click = NULL;
}

// ==============================================================================
// 3. TEXT EDITOR APP
// ==============================================================================
#define EDITOR_MAX_CHARS 2048
static char editor_buffer[EDITOR_MAX_CHARS] = {0};
static int  editor_len = 0;
static char editor_status[64] = "Ready. File: /home/notes.txt";

static void editor_render(gui_window_t* win) {
    int cx = win->x + 6;
    int cy = win->y + 28;
    int cw = win->width - 12;
    int ch = win->height - 34;

    // Background
    fb_draw_rect_fill(cx, cy, cw, ch, 0x00181A20);

    // Toolbar top
    fb_draw_rect_fill(cx, cy, cw, 30, 0x00232730);
    fb_draw_line(cx, cy + 30, cx + cw, cy + 30, 0x00333842);

    // Action button: [ Save File ]
    fb_draw_rect_fill(cx + 8, cy + 5, 80, 20, 0x000A84FF);
    fb_draw_string(cx + 16, cy + 7, "Save File", COLOR_WHITE, 0, true);

    // Action button: [ Reload ]
    fb_draw_rect_fill(cx + 96, cy + 5, 70, 20, 0x00334155);
    fb_draw_string(cx + 106, cy + 7, "Reload", COLOR_WHITE, 0, true);

    // Status message
    fb_draw_string(cx + 180, cy + 8, editor_status, 0x0094A3B8, 0, true);

    // Text editing area
    int text_y = cy + 40;
    int text_x = cx + 10;
    int line_start_x = text_x;

    for (int i = 0; i < editor_len; i++) {
        char c = editor_buffer[i];
        if (c == '\n') {
            text_y += 18;
            text_x = line_start_x;
        } else {
            fb_draw_char(text_x, text_y, c, COLOR_TEXT_LIGHT, 0, true);
            text_x += 8;
        }
    }

    // Blinking cursor
    if ((timer_get_ticks() / 30) % 2 == 0) {
        fb_draw_rect_fill(text_x, text_y, 8, 16, 0x000A84FF);
    }
}

static void editor_key(gui_window_t* win, char key) {
    (void)win;
    if (key == '\b') {
        if (editor_len > 0) {
            editor_buffer[--editor_len] = '\0';
        }
    } else if (key == '\n' || (key >= 32 && key <= 126)) {
        if (editor_len < EDITOR_MAX_CHARS - 1) {
            editor_buffer[editor_len++] = key;
            editor_buffer[editor_len] = '\0';
        }
    }
}

static void editor_click(gui_window_t* win, int local_x, int local_y) {
    (void)win;
    // Check [ Save File ] button (x: 8..88, y: 5..25 in toolbar)
    if (local_y >= 28 + 5 && local_y <= 28 + 25) {
        if (local_x >= 6 + 8 && local_x <= 6 + 88) {
            vfs_write_file("/home/notes.txt", editor_buffer, editor_len);
            strncpy(editor_status, "Saved successfully to /home/notes.txt", sizeof(editor_status) - 1);
        } else if (local_x >= 6 + 96 && local_x <= 6 + 166) {
            int r = vfs_read_file("/home/notes.txt", editor_buffer, sizeof(editor_buffer));
            if (r >= 0) {
                editor_len = r;
                strncpy(editor_status, "Reloaded /home/notes.txt", sizeof(editor_status) - 1);
            }
        }
    }
}

void app_editor_init(gui_window_t* win) {
    win->render_client = editor_render;
    win->handle_key = editor_key;
    win->handle_click = editor_click;

    if (editor_len == 0) {
        int r = vfs_read_file("/home/notes.txt", editor_buffer, sizeof(editor_buffer));
        if (r >= 0) {
            editor_len = r;
        } else {
            const char* init_text = "Welcome to AeroOS Text Editor!\nType here to create notes or edit files.\n";
            strcpy(editor_buffer, init_text);
            editor_len = strlen(init_text);
        }
    }
}

// ==============================================================================
// 4. FILE MANAGER APP
// ==============================================================================
static int filemgr_selected = 0;

static void filemgr_render(gui_window_t* win) {
    int cx = win->x + 6;
    int cy = win->y + 28;
    int cw = win->width - 12;
    int ch = win->height - 34;

    fb_draw_rect_fill(cx, cy, cw, ch, 0x00131824);

    // Left pane: Directory list (width: 200px)
    fb_draw_rect_fill(cx, cy, 200, ch, 0x001A202C);
    fb_draw_line(cx + 200, cy, cx + 200, cy + ch, 0x002D3748);

    fb_draw_string(cx + 12, cy + 10, "Filesystem Explorer", 0x0038BDF8, 0, true);
    fb_draw_line(cx + 10, cy + 28, cx + 190, cy + 28, 0x00334155);

    int count = vfs_get_file_count();
    int item_y = cy + 36;
    for (int i = 0; i < count; i++) {
        vfs_node_t* node = vfs_get_node_by_index(i);
        if (i == filemgr_selected) {
            fb_draw_rect_fill(cx + 6, item_y - 2, 188, 20, 0x000A84FF);
        }

        const char* icon = node->is_dir ? "[DIR]" : "[TXT]";
        uint32_t color = (i == filemgr_selected) ? COLOR_WHITE : (node->is_dir ? 0x00FBBF24 : COLOR_TEXT_LIGHT);

        fb_draw_string(cx + 10, item_y, icon, color, 0, true);
        fb_draw_string(cx + 56, item_y, node->name, color, 0, true);
        item_y += 22;
    }

    // Right pane: File Preview
    int rx = cx + 210;
    fb_draw_string(rx, cy + 10, "File Details & Preview:", 0x0038BDF8, 0, true);
    fb_draw_line(rx, cy + 28, cx + cw - 10, cy + 28, 0x00334155);

    vfs_node_t* sel_node = vfs_get_node_by_index(filemgr_selected);
    if (sel_node) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Path: %s", sel_node->path);
        fb_draw_string(rx, cy + 36, buf, COLOR_WHITE, 0, true);
        snprintf(buf, sizeof(buf), "Size: %d bytes | Type: %s",
                 (int)sel_node->size, sel_node->is_dir ? "Directory" : "Text Document");
        fb_draw_string(rx, cy + 54, buf, COLOR_TEXT_DIM, 0, true);

        // Preview box
        fb_draw_rect_fill(rx, cy + 76, cw - 220, ch - 86, 0x000F141C);
        fb_draw_rect(rx, cy + 76, cw - 220, ch - 86, 0x002D3748);

        if (!sel_node->is_dir && sel_node->content) {
            int py = cy + 84;
            char line_buf[64];
            const char* p = sel_node->content;
            while (*p && py < cy + ch - 20) {
                int l = 0;
                while (*p && *p != '\n' && l < 60) {
                    line_buf[l++] = *p++;
                }
                if (*p == '\n') p++;
                line_buf[l] = '\0';
                fb_draw_string(rx + 8, py, line_buf, 0x00E2E8F0, 0, true);
                py += 18;
            }
        } else if (sel_node->is_dir) {
            fb_draw_string(rx + 16, cy + 96, "(Directory contents displayed in left tree)", COLOR_TEXT_DIM, 0, true);
        }
    }
}

static void filemgr_click(gui_window_t* win, int local_x, int local_y) {
    (void)win;
    int cx = 6;
    int cy = 28;
    if (local_x >= cx + 6 && local_x <= cx + 194) {
        int clicked_idx = (local_y - (cy + 36)) / 22;
        if (clicked_idx >= 0 && clicked_idx < vfs_get_file_count()) {
            filemgr_selected = clicked_idx;
        }
    }
}

void app_filemgr_init(gui_window_t* win) {
    win->render_client = filemgr_render;
    win->handle_key = NULL;
    win->handle_click = filemgr_click;
}

// ==============================================================================
// 5. CALCULATOR APP
// ==============================================================================
static char calc_display[32] = "0";
static int  calc_acc = 0;
static char calc_op = 0;
static bool calc_clear_on_next = false;

static void calc_press(char c) {
    if (c >= '0' && c <= '9') {
        if (calc_clear_on_next || strcmp(calc_display, "0") == 0) {
            calc_display[0] = c;
            calc_display[1] = '\0';
            calc_clear_on_next = false;
        } else {
            size_t len = strlen(calc_display);
            if (len < 12) {
                calc_display[len] = c;
                calc_display[len + 1] = '\0';
            }
        }
    } else if (c == 'C') {
        strcpy(calc_display, "0");
        calc_acc = 0;
        calc_op = 0;
        calc_clear_on_next = false;
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        calc_acc = 0;
        const char* p = calc_display;
        while (*p >= '0' && *p <= '9') { calc_acc = calc_acc * 10 + (*p++ - '0'); }
        calc_op = c;
        calc_clear_on_next = true;
    } else if (c == '=') {
        if (calc_op != 0) {
            int operand = 0;
            const char* p = calc_display;
            while (*p >= '0' && *p <= '9') { operand = operand * 10 + (*p++ - '0'); }

            int res = 0;
            if (calc_op == '+') res = calc_acc + operand;
            else if (calc_op == '-') res = calc_acc - operand;
            else if (calc_op == '*') res = calc_acc * operand;
            else if (calc_op == '/') res = (operand != 0) ? (calc_acc / operand) : 0;

            snprintf(calc_display, sizeof(calc_display), "%d", res);
            calc_op = 0;
            calc_clear_on_next = true;
        }
    }
}

static const char* calc_btn_labels[4][4] = {
    {"7", "8", "9", "/"},
    {"4", "5", "6", "*"},
    {"1", "2", "3", "-"},
    {"C", "0", "=", "+"}
};

static void calc_render(gui_window_t* win) {
    int cx = win->x + 8;
    int cy = win->y + 28;
    int cw = win->width - 16;
    int ch = win->height - 34;

    fb_draw_rect_fill(cx, cy, cw, ch, 0x001A202C);

    // Digital LCD display area
    fb_draw_rect_fill(cx + 8, cy + 8, cw - 16, 40, 0x000F172A);
    fb_draw_rect(cx + 8, cy + 8, cw - 16, 40, 0x00334155);

    // Right-aligned LCD digits
    int text_len = (int)strlen(calc_display);
    int text_x = cx + cw - 28 - (text_len * 8);
    fb_draw_string(text_x, cy + 20, calc_display, 0x0038BDF8, 0, true);

    // 4x4 Button Grid
    int btn_w = (cw - 32) / 4;
    int btn_h = (ch - 70) / 4;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = cx + 8 + c * (btn_w + 5);
            int by = cy + 56 + r * (btn_h + 5);

            uint32_t btn_bg = 0x002D3748;
            const char* label = calc_btn_labels[r][c];
            if (*label == '=' || *label == '+' || *label == '-' || *label == '*' || *label == '/') {
                btn_bg = 0x000A84FF;
            } else if (*label == 'C') {
                btn_bg = 0x00DC2626;
            }

            fb_draw_rect_fill(bx, by, btn_w, btn_h, btn_bg);
            fb_draw_rect(bx, by, btn_w, btn_h, 0x00475569);
            fb_draw_string(bx + (btn_w / 2) - 4, by + (btn_h / 2) - 8, label, COLOR_WHITE, 0, true);
        }
    }
}

static void calc_click(gui_window_t* win, int local_x, int local_y) {
    int cx = 8;
    int cy = 28;
    int cw = win->width - 16;
    int ch = win->height - 34;

    int btn_w = (cw - 32) / 4;
    int btn_h = (ch - 70) / 4;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = cx + 8 + c * (btn_w + 5);
            int by = cy + 56 + r * (btn_h + 5);

            if (local_x >= bx && local_x <= bx + btn_w &&
                local_y >= by && local_y <= by + btn_h) {
                calc_press(calc_btn_labels[r][c][0]);
                return;
            }
        }
    }
}

static void calc_key(gui_window_t* win, char key) {
    (void)win;
    if ((key >= '0' && key <= '9') || key == '+' || key == '-' || key == '*' || key == '/' || key == '=' || key == 'c' || key == 'C') {
        if (key == 'c') key = 'C';
        calc_press(key);
    } else if (key == '\n') {
        calc_press('=');
    }
}

void app_calc_init(gui_window_t* win) {
    win->render_client = calc_render;
    win->handle_key = calc_key;
    win->handle_click = calc_click;
}
