#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

typedef struct {
    int  x;
    int  y;
    bool left_button;
    bool right_button;
    bool middle_button;
    bool left_clicked;
    bool right_clicked;
} mouse_state_t;

extern mouse_state_t mouse;

void mouse_init(void);
void mouse_update_bounds(int width, int height);

#endif
