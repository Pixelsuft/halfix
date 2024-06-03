#include "display.h"
#include "ui-mobile.h"
#ifdef SDL2_INC_DIR
#include <SDL2/SDL_ttf.h>
#else
#include <SDL_ttf.h>
#endif

#define COL_BUTTON1_R 0
#define COL_BUTTON1_G 255
#define COL_BUTTON1_B 0

static SDL_Window* win = NULL;
static SDL_Renderer* ren = NULL;
static TTF_Font* fnt = NULL;
static int width, height;

typedef struct {
    mobui_elem* elems[10];
    size_t elem_count;
    mobui_button go_btn;
} mobui_page;

mobui_page page;

void mobui_elem_set_rect(void* elem, SDL_FRect* rect) {
    mobui_elem* this = elem;
    this->rect.x = rect->x;
    this->rect.y = rect->y;
    this->rect.w = rect->w;
    this->rect.h = rect->h;
}

void mobui_destroy_elem(void* elem) {
    (void)elem;
}

void mobui_init_elem(void* elem) {
    mobui_elem* this = elem;
    this->on_down = NULL;
    this->on_up = NULL;
    this->on_move = NULL;
    this->destroy = mobui_destroy_elem;
    this->set_rect = mobui_elem_set_rect;
    this->rect.x = this->rect.y = this->rect.w = this->rect.h = 0.0f;
}

void mobui_draw_button(void* elem) {
    mobui_button* this = elem;
    SDL_SetRenderDrawColor(ren, COL_BUTTON1_R, COL_BUTTON1_G, COL_BUTTON1_B, 255);
    SDL_RenderDrawRectF(ren, &this->base.rect);
}

void mobui_init_button(mobui_button* this) {
    mobui_init_elem(this);
    this->tex = NULL;
    this->text = NULL;
    this->base.draw = mobui_draw_button;
}

void mobui_place_elems(void) {
    if (SDL_GetRendererOutputSize(ren, &width, &height) < 0) {
        SDL_GetWindowSize(win, &width, &height);
    }
    float sx = (float)width / 640.0f;
    float sy = (float)height / 480.0f;
    float sm = (sx > sy) ? sy : sx;
    SDL_FRect tr = { (640.0f - 65.0f) * sx, 5.0f * sm, 60.0f * sx, 40.0f * sy };
    page.go_btn.base.set_rect(&page.go_btn, &tr);
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
                case SDL_WINDOWEVENT: {
                    if (ev.window.event == SDL_WINDOWEVENT_RESIZED || ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                        mobui_place_elems();
                    break;
                }
            }
        }
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        for (size_t i = 0; i < page.elem_count; i++) {
            if (page.elems[i] == NULL)
                continue;
            if (page.elems[i]->draw == NULL)
                continue;
            page.elems[i]->draw(page.elems[i]);
        }
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
    mobui_init_button(&page.go_btn);
    mobui_place_elems();
    page.elems[0] = (mobui_elem*)&page.go_btn;
    page.elem_count = 10;
}

void mobui_quit(void) {
    TTF_CloseFont(fnt);
    TTF_Quit();
}
