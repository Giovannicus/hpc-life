#ifndef VIEWER_H
#define VIEWER_H

typedef struct Viewer Viewer;   // opaque type: its fields are hidden in viewer.c

Viewer *viewer_open(const char *title, int grid_w,
    int grid_h, int cell_px);

int     viewer_poll(Viewer *v);    // 0 when the user asks to quit
void    viewer_draw(Viewer *v, const unsigned char *grid); // grid_w * grid_h cells
void    viewer_close(Viewer *v);
void viewer_set_delay(Viewer *v, int delay_ms); // pause after each frame

#endif