#include <stdio.h>
#include <stdlib.h> 
#include "viewer.h"

static void step(unsigned char *cur, unsigned char *next, int n){
    for (int i = 0; i < n; i++){
        int up = (i == 0) ? n -1 : i - 1;
        int down = (i == n-1) ? 0 : i + 1;

        for (int j = 0; j < n; j++){
            int left = (j == 0) ? n - 1 : j - 1;
            int right = (j == n-1) ? 0 : j + 1;
            
            size_t alive = cur[up * n + left] + cur[up * n + j] + cur[up * n + right] +
                                  cur[i * n + left] + cur[i * n + right] +
                                  cur[down * n + left] + cur[down * n + j] + cur[down * n + right];
            if (alive == 3){
                next[i * n + j] = 1;
            } else if (alive == 2){
                next[i * n + j] = cur[i * n + j];
            } else {
                next[i * n + j] = 0;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int n = (argc > 1) ? atoi(argv[1]) : 256;
    int steps = (argc > 2) ? atoi(argv[2]) : 500;
    int cell_px = (argc > 3) ? atoi(argv[3]) : 3;
    int delay_ms = (argc > 4) ? atoi(argv[4]) : 16;

    size_t size = (size_t)n * n;
    unsigned char *cur = malloc(size);
    unsigned char *next = malloc(size);
    if (cur == NULL || next == NULL){ fprintf(stderr, "Failed to allocate memory\n"); return 1;}
    Viewer *v = viewer_open("life", n, n, cell_px);
    viewer_set_delay(v, delay_ms);
    if (v == NULL){ free(cur); free(next); fprintf(stderr, "Failed to create viewer\n"); return 1;} 

    int seed = 0;
    srand(seed);
    for (size_t i = 0; i < size; i++){
        cur[i] = rand() % 100 < 30;
    }

    for (int s = 0; s < steps && viewer_poll(v); s++){
        step(cur, next, n);
        unsigned char *temp = cur;
        cur = next;
        next = temp;
        viewer_draw(v, cur);
    }
    
    while (viewer_poll(v)){  // steps are over: keep the last image until the user quits    
        viewer_draw(v, cur);
    }

    viewer_close(v);
    free(cur);
    free(next);

    return 0;
}