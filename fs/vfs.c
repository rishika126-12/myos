#include "fs/vfs.h"
#include "kernel/heap.h"
#include "string.h"
#include "drivers/serial.h"

static vfs_node_t file_table[VFS_MAX_FILES];
static int file_count = 0;

void vfs_init(void) {
    file_count = 0;
    memset(file_table, 0, sizeof(file_table));

    // Populate initial system directories & files
    vfs_create_file("/system", NULL, 0);
    file_table[file_count - 1].is_dir = true;

    vfs_create_file("/docs", NULL, 0);
    file_table[file_count - 1].is_dir = true;

    vfs_create_file("/home", NULL, 0);
    file_table[file_count - 1].is_dir = true;

    const char* os_info =
        "AeroOS Version 1.0 Release\n"
        "Architecture: IA-32 (x86 32-bit Protected Mode)\n"
        "Kernel: Aero Freestanding Monolithic Kernel\n"
        "GUI: High-Res 1024x768x32bpp Linear Compositor\n"
        "Networking: Intel PRO/1000 (82540EM) Driver\n"
        "Protocols: Ethernet, ARP, IPv4, ICMP, UDP, DHCP\n";
    vfs_create_file("/system/os_info.txt", os_info, strlen(os_info));

    const char* readme =
        "Welcome to AeroOS Academic Project!\n"
        "====================================\n"
        "Features:\n"
        "- Real-time desktop GUI with taskbar, start menu, clock\n"
        "- Interactive movable, focusable, closable windows\n"
        "- Full PS/2 mouse & keyboard support\n"
        "- Intel e1000 Gigabit Ethernet with DHCP & ICMP ping\n"
        "- Applications: Terminal, File Manager, Text Editor, Calculator, System Info\n"
        "- Pure freestanding C & Assembly kernel (Zero Linux dependencies)\n";
    vfs_create_file("/docs/readme.txt", readme, strlen(readme));

    const char* notes =
        "Operating Systems Academic Design:\n"
        "1. Boot: Multiboot 1 compliant GRUB header with VBE linear framebuffer.\n"
        "2. Core: GDT (Kernel CS 0x08, DS 0x10), IDT with 256 gates.\n"
        "3. Interrupts: 8259 PIC remapped to 32-47, PIT timer at 100Hz.\n"
        "4. Memory: PMM page bitmap + 32MB dynamic heap (kmalloc/kfree).\n"
        "5. Networking: MMIO BAR0 e1000 ring buffers + DHCP autoconfig.\n";
    vfs_create_file("/docs/academic_notes.txt", notes, strlen(notes));

    const char* user_notes =
        "My Personal Notes:\n"
        "- Test network ping in Terminal: 'ping 10.0.2.2'\n"
        "- View IP configuration: 'ifconfig'\n"
        "- Explore filesystem: 'ls'\n";
    vfs_create_file("/home/notes.txt", user_notes, strlen(user_notes));

    serial_printf("[VFS] Virtual File System initialized with %d entries\n", file_count);
}

bool vfs_create_file(const char* path, const char* content, size_t size) {
    if (!path || file_count >= VFS_MAX_FILES) return false;

    // Check if already exists
    for (int i = 0; i < file_count; i++) {
        if (strcmp(file_table[i].path, path) == 0) {
            return vfs_write_file(path, content, size);
        }
    }

    vfs_node_t* node = &file_table[file_count++];
    strncpy(node->path, path, VFS_MAX_PATH - 1);

    // Extract file name
    const char* name_ptr = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/') name_ptr = p + 1;
    }
    strncpy(node->name, name_ptr, VFS_MAX_FILENAME - 1);

    node->is_dir = (content == NULL && size == 0);
    node->size = size;

    if (content && size > 0) {
        node->content = (char*)kmalloc(size + 1);
        if (node->content) {
            memcpy(node->content, content, size);
            node->content[size] = '\0';
        }
    } else {
        node->content = NULL;
    }
    return true;
}

bool vfs_write_file(const char* path, const char* content, size_t size) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(file_table[i].path, path) == 0) {
            if (file_table[i].content) {
                kfree(file_table[i].content);
            }
            file_table[i].size = size;
            file_table[i].content = (char*)kmalloc(size + 1);
            if (file_table[i].content) {
                if (content && size > 0) {
                    memcpy(file_table[i].content, content, size);
                }
                file_table[i].content[size] = '\0';
            }
            return true;
        }
    }
    return vfs_create_file(path, content, size);
}

int vfs_read_file(const char* path, char* buffer, size_t max_len) {
    if (!path || !buffer || max_len == 0) return -1;
    for (int i = 0; i < file_count; i++) {
        if (strcmp(file_table[i].path, path) == 0) {
            if (file_table[i].is_dir) return -1;
            size_t copy_size = file_table[i].size;
            if (copy_size >= max_len) copy_size = max_len - 1;
            if (file_table[i].content) {
                memcpy(buffer, file_table[i].content, copy_size);
            }
            buffer[copy_size] = '\0';
            return (int)copy_size;
        }
    }
    return -1;
}

bool vfs_file_exists(const char* path) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(file_table[i].path, path) == 0) {
            return true;
        }
    }
    return false;
}

int vfs_list_dir(const char* dir_path, char* out_list, size_t max_len) {
    if (!out_list || max_len == 0) return 0;
    out_list[0] = '\0';
    size_t dlen = strlen(dir_path);

    int found = 0;
    for (int i = 0; i < file_count; i++) {
        const char* p = file_table[i].path;
        if (strncmp(p, dir_path, dlen) == 0) {
            // Check direct child
            const char* sub = p + dlen;
            if (*sub == '/') sub++;
            if (*sub != '\0' && !strchr(sub, '/')) {
                strcat(out_list, file_table[i].is_dir ? "[DIR]  " : "[FILE] ");
                strcat(out_list, sub);
                strcat(out_list, "\n");
                found++;
            }
        }
    }
    return found;
}

int vfs_get_file_count(void) {
    return file_count;
}

vfs_node_t* vfs_get_node_by_index(int idx) {
    if (idx >= 0 && idx < file_count) {
        return &file_table[idx];
    }
    return NULL;
}
