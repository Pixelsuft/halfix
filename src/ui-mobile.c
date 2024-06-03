#include "display.h"
#include "util.h"
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
static mobui_elem* focus_elem = NULL;
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
    UNUSED(elem);
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

void mobui_button_on_down(void* elem, SDL_FPoint* pos) {
    UNUSED(pos);
    mobui_button* this = elem;
    if (this->tex != NULL)
        SDL_SetTextureColorMod(this->tex, 0, 0, 0);
    this->is_down = 1;
}

void mobui_button_on_up(void* elem, SDL_FPoint* pos) {
    UNUSED(pos);
    mobui_button* this = elem;
    if (this->tex != NULL)
        SDL_SetTextureColorMod(this->tex, 255, 255, 255);
    this->is_down = 0;
    this->was_pressed = (SDL_PointInFRect(pos, &this->base.rect) == SDL_TRUE) ? 1 : 0;
}

void mobui_button_draw(void* elem) {
    mobui_button* this = elem;
    SDL_SetRenderDrawColor(ren, COL_BUTTON1_R, COL_BUTTON1_G, COL_BUTTON1_B, 255);
    if (this->is_down)
        SDL_RenderFillRectF(ren, &this->base.rect);
    else
        SDL_RenderDrawRectF(ren, &this->base.rect);
    if (this->tex == NULL)
        return;
    this->text_rect.x = this->base.rect.x + this->base.rect.w / 2.0f - this->text_rect.w / 2.0f;
    this->text_rect.y = this->base.rect.y + this->base.rect.h / 2.0f - this->text_rect.h / 2.0f;
    // WTF why it's bad
    // SDL_RenderCopyF(ren, this->tex, NULL, &this->text_rect);
    SDL_Rect dst_rect;
    dst_rect.x = (int)this->text_rect.x;
    dst_rect.y = (int)this->text_rect.y;
    dst_rect.w = (int)this->text_rect.w;
    dst_rect.h = (int)this->text_rect.h;
    SDL_RenderCopy(ren, this->tex, NULL, &dst_rect);
}

void mobui_button_set_text(mobui_button* this, const char* text) {
    if (text == NULL)
        return;
    this->text = (char*)text;
    SDL_Color col = { COL_BUTTON1_R, COL_BUTTON1_G, COL_BUTTON1_B, 255 };
    SDL_Surface* surf = TTF_RenderText_Solid(fnt, text, col);
    if (surf == NULL)
        return;
    int tw = surf->w;
    int th = surf->h;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    if (tex == NULL)
        return;
    this->tex = tex;
    this->text_rect.w = (float)tw;
    this->text_rect.h = (float)th;
}

void mobui_button_set_rect(void* elem, SDL_FRect* rect) {
    mobui_button* this = elem;
    mobui_elem_set_rect(elem, rect);
    mobui_button_set_text(this, this->text);
}

void mobui_button_destroy(void* elem) {
    mobui_button* this = elem;
    if (this->tex != NULL) {
        SDL_DestroyTexture(this->tex);
        this->tex = NULL;
    }
}

void mobui_init_button(mobui_button* this) {
    mobui_init_elem(this);
    this->tex = NULL;
    this->text = NULL;
    this->is_down = 0;
    this->was_pressed = 0;
    this->base.on_down = mobui_button_on_down;
    this->base.on_up = mobui_button_on_up;
    this->base.draw = mobui_button_draw;
    this->base.set_rect = mobui_button_set_rect;
    this->base.destroy = mobui_button_destroy;
}

void mobui_place_elems(void) {
    if (SDL_GetRendererOutputSize(ren, &width, &height) < 0) {
        SDL_GetWindowSize(win, &width, &height);
    }
    float sx = (float)width / 640.0f;
    float sy = (float)height / 480.0f;
    float sm = (sx > sy) ? sy : sx;
    TTF_SetFontSize(fnt, (int)(32.0f * sm));
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
                case SDL_MOUSEMOTION: {
                    if (focus_elem == NULL || focus_elem->on_move == NULL)
                        break;
                    SDL_FPoint dt_move = { (float)ev.motion.xrel, (float)ev.motion.yrel };
                    focus_elem->on_move(focus_elem, &dt_move);
                    break;
                }
                case SDL_MOUSEBUTTONDOWN: {
                    SDL_FPoint cur_pos = { (float)ev.button.x, (float)ev.button.y };
                    for (size_t i = 0; i < page.elem_count; i++) {
                        if (page.elems[i] == NULL)
                            continue;
                        if (SDL_PointInFRect(&cur_pos, &page.elems[i]->rect) == SDL_TRUE) {
                            focus_elem = page.elems[i];
                            if (focus_elem->on_down != NULL)
                                focus_elem->on_down(focus_elem, &cur_pos);
                            break;
                        }
                    }
                    break;
                }
                case SDL_MOUSEBUTTONUP: {
                    if (focus_elem == NULL)
                        break;
                    SDL_FPoint cur_pos = { (float)ev.button.x, (float)ev.button.y };
                    if (focus_elem->on_up != NULL)
                        focus_elem->on_up(focus_elem, &cur_pos);
                    focus_elem = NULL;
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
        if (page.go_btn.was_pressed) {
            page.go_btn.was_pressed = 0;
            printf("GO PRESS!\n");
        }
        SDL_RenderPresent(ren);
    }
    for (size_t i = 0; i < page.elem_count; i++) {
        if (page.elems[i] == NULL)
            continue;
        page.elems[i]->destroy(page.elems[i]);
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
    mobui_button_set_text(&page.go_btn, "GO!");
    mobui_place_elems();
    page.elems[0] = (mobui_elem*)&page.go_btn;
    page.elem_count = 10;
}

void mobui_quit(void) {
    TTF_CloseFont(fnt);
    TTF_Quit();
}
