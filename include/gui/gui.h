#ifndef GUI_H
#define GUI_H

#include "types.h"

#define MAX_WINDOWS 8

typedef enum {
    APP_TERMINAL = 0,
    APP_SYSINFO,
    APP_EDITOR,
    APP_FILEMGR,
    APP_CALC,
    APP_COUNT
} app_type_t;

typedef struct gui_window {
    int         id;
    app_type_t  app_type;
    char        title[48];
    int         x;
    int         y;
    int         width;
    int         height;
    bool        is_open;
    bool        is_minimized;
    bool        is_dragging;
    int         drag_off_x;
    int         drag_off_y;
    void        (*render_client)(struct gui_window* win);
    void        (*handle_key)(struct gui_window* win, char key);
    void        (*handle_click)(struct gui_window* win, int local_x, int local_y);
} gui_window_t;

void gui_init(void);
void gui_render_frame(void);
void gui_handle_input(void);

gui_window_t* gui_open_window(app_type_t app);
void          gui_close_window(gui_window_t* win);
void          gui_focus_window(gui_window_t* win);

#endif
