#ifndef VFS_H
#define VFS_H

#include "types.h"

#define VFS_MAX_PATH     128
#define VFS_MAX_FILENAME 32
#define VFS_MAX_FILES    64
#define VFS_FILE_MAX_SIZE 16384

typedef struct vfs_node {
    char   name[VFS_MAX_FILENAME];
    char   path[VFS_MAX_PATH];
    bool   is_dir;
    size_t size;
    char*  content;
} vfs_node_t;

void vfs_init(void);
bool vfs_create_file(const char* path, const char* content, size_t size);
bool vfs_write_file(const char* path, const char* content, size_t size);
int  vfs_read_file(const char* path, char* buffer, size_t max_len);
bool vfs_file_exists(const char* path);
int  vfs_list_dir(const char* dir_path, char* out_list, size_t max_len);
int  vfs_get_file_count(void);
vfs_node_t* vfs_get_node_by_index(int idx);

#endif
