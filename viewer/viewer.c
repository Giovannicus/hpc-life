#include <stdlib.h>
#include <stdio.h>
#include "viewer.h"
#include <SDL2/SDL.h>

struct Viewer {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int grid_w, grid_h, cell_px;
    int delay_ms;
};

Viewer *viewer_open(const char *title, int grid_w, int grid_h, int cell_px){

    Viewer *v = malloc(sizeof(Viewer));
    if (v == NULL){free(v); return NULL;}
    v->delay_ms = 16;
    if (SDL_Init(SDL_INIT_VIDEO) < 0){ fprintf(stderr,     
        "SDL_Init failed: %s\n", SDL_GetError()); free(v); return NULL;}

    SDL_Window *window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        grid_w * cell_px, grid_h * cell_px, 0);
    v->window=window;
    v->grid_w = grid_w;
    v->grid_h = grid_h;
    v->cell_px = cell_px;

    SDL_Renderer *renderer = SDL_CreateRenderer(window,-1,0);
    v->renderer=renderer;

    if (window == NULL || renderer == NULL){
        fprintf(stderr, "window or renderer failed: %s\n", SDL_GetError());
        if (window != NULL) {SDL_DestroyWindow(window); SDL_Quit(); free(v); return NULL;}
    }

    return v;
    
}

int viewer_poll(Viewer *v){  // 0 when the user asks to quit
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        if (event.type == SDL_QUIT) return 0;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) return 0; 
    }
    return 1;
}   

void viewer_draw(Viewer *v, const unsigned char *grid) { // grid_w * grid_h cells

    SDL_SetRenderDrawColor(v->renderer, 0, 0, 0, 255);
    SDL_RenderClear(v->renderer);

    SDL_SetRenderDrawColor(v->renderer, 255, 255, 255, 255);
    
    int i,j;
    for ( i=0; i<v->grid_h; i++){
        for ( j=0; j<v->grid_w; j++){
            
            if (grid[i * v->grid_w + j] != 0){
                SDL_Rect r = { j * v->cell_px, i * v->cell_px, v->cell_px, v->cell_px };
                SDL_RenderFillRect(v->renderer, &r);
            }
        }
    }
    SDL_RenderPresent(v->renderer);
    SDL_Delay(v->delay_ms);
}

void viewer_close(Viewer *v){
    SDL_DestroyRenderer(v->renderer);
    SDL_DestroyWindow(v->window);
    SDL_Quit();    
    free(v);
}

void viewer_set_delay(Viewer *v, int delay_ms){   
       v->delay_ms = delay_ms; 
}