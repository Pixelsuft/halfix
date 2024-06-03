#include "display.h"
#include "ui-mobile.h"
#ifdef SDL2_INC_DIR
#include <SDL2/SDL_ttf.h>
#else
#include <SDL_ttf.h>
#endif

static SDL_Window* win = NULL;
static SDL_Renderer* ren = NULL;
static TTF_Font* fnt = NULL;

typedef struct {
    mobui_elem* elems[10];
    size_t elem_count;
    mobui_button go_btn;
} mobui_page;

mobui_page page;

void mobui_init_elem(void* elem) {
    mobui_elem* this = elem;
    this->on_down = NULL;
    this->on_up = NULL;
    this->on_move = NULL;
    this->set_rect = NULL;
    this->rect.x = this->rect.y = this->rect.w = this->rect.h = 0.0f;
}

void mobui_init_button(mobui_button* this) {
    mobui_init_elem(this);
}

void mobui_run_main(void) {
    int running = 1;
    SDL_Event ev;
    while (running) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: {
                    running = 0;
                    break;
                }
            }
        }
        for (size_t i = 0; i < page.elem_count; i++) {
            if (page.elems[i] == NULL)
                continue;
            if (page.elems[i]->draw == NULL)
                continue;
            page.elems[i]->draw(page.elems[i]);
        }
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderPresent(ren);
    }
}

void mobui_init(void) {
    if (TTF_Init() < 0)
        return;
    fnt = TTF_OpenFont("fonts/liberationmonob.ttf", 32);
    if (fnt == NULL)
        return;
    win = display_get_handle(0);
    ren = display_get_handle(1);
    memset(page.elems, 0, sizeof(page.elems));
    page.elems[0] = (mobui_elem*)&page.go_btn;
    page.elem_count = 10;
}

void mobui_quit(void) {
    TTF_CloseFont(fnt);
    TTF_Quit();
}
