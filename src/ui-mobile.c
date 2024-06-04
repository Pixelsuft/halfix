#include "display.h"
#include "util.h"
#include "ui-mobile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#ifdef SDL2_INC_DIR
#include <SDL2/SDL_ttf.h>
#else
#include <SDL_ttf.h>
#endif

#define COL1_R 0
#define COL1_G 255
#define COL1_B 0
#define COL2_R 255
#define COL2_G 0
#define COL2_B 0

static SDL_Window* win = NULL;
static SDL_Renderer* ren = NULL;
static TTF_Font* fnt1 = NULL;
static TTF_Font* fnt2 = NULL;
static mobui_elem* focus_elem = NULL;
static int width, height;

typedef struct {
    mobui_elem* elems[20];
    size_t elem_count;
    mobui_button go_btn;
    mobui_button cfg_btn;
    mobui_button help_btn;
    mobui_button d50m_btn;
    mobui_button d100m_btn;
    mobui_button d250m_btn;
    mobui_button d500m_btn;
    mobui_button d1g_btn;
    mobui_button d2g_btn;
    mobui_button d4g_btn;
    mobui_button d6g_btn;
    mobui_input path_inp;
    int allow_to_start;
    int allow_is_dir;
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
    if (!this->enabled)
        return;
    if (this->tex != NULL)
        SDL_SetTextureColorMod(this->tex, 0, 0, 0);
    this->is_down = 1;
}

void mobui_button_on_up(void* elem, SDL_FPoint* pos) {
    UNUSED(pos);
    mobui_button* this = elem;
    if (!this->enabled)
        return;
    if (this->tex != NULL)
        SDL_SetTextureColorMod(this->tex, 255, 255, 255);
    this->is_down = 0;
    this->was_pressed = (SDL_PointInFRect(pos, &this->base.rect) == SDL_TRUE) ? 1 : 0;
}

void mobui_button_draw(void* elem) {
    mobui_button* this = elem;
    if (this->enabled)
        SDL_SetRenderDrawColor(ren, COL1_R, COL1_G, COL1_B, 255);
    else
        SDL_SetRenderDrawColor(ren, COL2_R, COL2_G, COL2_B, 255);
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
    SDL_Color col = { COL1_R, COL1_G, COL1_B, 255 };
    SDL_Surface* surf = TTF_RenderText_Blended(fnt1, text, col);
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
    this->enabled = 1;
    this->was_pressed = 0;
    this->base.on_down = mobui_button_on_down;
    this->base.on_up = mobui_button_on_up;
    this->base.draw = mobui_button_draw;
    this->base.set_rect = mobui_button_set_rect;
    this->base.destroy = mobui_button_destroy;
}

void mobui_input_destroy(void* elem) {
    mobui_input* this = elem;
    if (this->tex != NULL) {
        SDL_DestroyTexture(this->tex);
        this->tex = NULL;
    }
}

void mobui_input_draw(void* elem) {
    mobui_input* this = elem;
    SDL_SetRenderDrawColor(ren, COL1_R, COL1_G, COL1_B, 255);
    SDL_RenderDrawRectF(ren, &this->base.rect);
    if (this->tex == NULL)
        return;
    if (this->src_rect.x == 0)
        this->text_rect.x = this->base.rect.x + this->base.rect.w / 2.0f - this->text_rect.w / 2.0f;
    this->text_rect.y = this->base.rect.y + this->base.rect.h / 2.0f - this->text_rect.h / 2.0f;
    SDL_Rect dst_rect;
    dst_rect.x = (int)this->text_rect.x;
    dst_rect.y = (int)this->text_rect.y;
    dst_rect.w = (int)this->text_rect.w;
    dst_rect.h = (int)this->text_rect.h;
    SDL_RenderCopy(ren, this->tex, &this->src_rect, &dst_rect);
}

void mobui_input_on_down(void* elem, SDL_FPoint* pos) {
    UNUSED(elem);
    UNUSED(pos);
    SDL_StartTextInput();
}

void mobui_input_on_update(mobui_input* this) {
    SDL_Color col = { COL1_R, COL1_G, COL1_B, 255 };
    SDL_Surface* surf = TTF_RenderText_Blended(fnt2, strlen(this->text) > 0 ? this->text : " ", col);
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
    this->src_rect.y = 0;
    this->src_rect.h = th;
    this->src_rect.x = 0;
    this->src_rect.w = tw;
    if ((float)tw > this->base.rect.w) {
        this->src_rect.w = (int)this->base.rect.w;
        this->src_rect.x = tw - this->src_rect.w;
        this->text_rect.w = this->base.rect.w;
        this->base.rect.x = this->base.rect.x;
    }
}

void mobui_input_set_rect(void* elem, SDL_FRect* rect) {
    mobui_input* this = elem;
    mobui_elem_set_rect(elem, rect);
    mobui_input_on_update(this);
}

void mobui_init_input(mobui_input* this) {
    mobui_init_elem(this);
    this->tex = NULL;
    this->text[0] = '\0';
    this->base.on_down = mobui_input_on_down;
    this->base.draw = mobui_input_draw;
    this->base.set_rect = mobui_input_set_rect;
    this->base.destroy = mobui_input_destroy;
}

void mobui_on_path_input_update(void) {
    struct stat path_stat;
    int res = stat(page.path_inp.text, &path_stat);
    if (res == 0 && S_ISREG(path_stat.st_mode)) {
        page.go_btn.enabled = 1;
        page.cfg_btn.enabled = 0;
        page.d50m_btn.enabled = 0;
        page.d100m_btn.enabled = 0;
        page.d250m_btn.enabled = 0;
        page.d500m_btn.enabled = 0;
        page.d1g_btn.enabled = 0;
        page.d2g_btn.enabled = 0;
        page.d4g_btn.enabled = 0;
        page.d6g_btn.enabled = 0;
    }
    else if (res == 0 && S_ISDIR(path_stat.st_mode)) {
        page.go_btn.enabled = 0;
        page.cfg_btn.enabled = 1;
        page.d50m_btn.enabled = 1;
        page.d100m_btn.enabled = 1;
        page.d250m_btn.enabled = 1;
        page.d500m_btn.enabled = 1;
        page.d1g_btn.enabled = 1;
        page.d2g_btn.enabled = 1;
        page.d4g_btn.enabled = 1;
        page.d6g_btn.enabled = 1;
    }
    else {
        page.go_btn.enabled = 0;
        page.cfg_btn.enabled = 0;
        page.d50m_btn.enabled = 0;
        page.d100m_btn.enabled = 0;
        page.d250m_btn.enabled = 0;
        page.d500m_btn.enabled = 0;
        page.d1g_btn.enabled = 0;
        page.d2g_btn.enabled = 0;
        page.d4g_btn.enabled = 0;
        page.d6g_btn.enabled = 0;
    }
}

void mobui_place_elems(void) {
    if (SDL_GetRendererOutputSize(ren, &width, &height) < 0) {
        SDL_GetWindowSize(win, &width, &height);
    }
    float sx = (float)width / 640.0f;
    float sy = (float)height / 480.0f;
    float sm = (sx > sy) ? sy : sx;
    TTF_SetFontSize(fnt1, (int)(32.0f * sm));
    TTF_SetFontSize(fnt2, (int)(16.0f * sm));
    SDL_FRect tr1 = { (640.0f - 65.0f) * sx, 5.0f * sm, 60.0f * sx, 60.0f * sy };
    page.go_btn.base.set_rect(&page.go_btn, &tr1);
    SDL_FRect tr2 = { 5.0f * sx, 5.0f * sm, (640.0f - 75.0f) * sx, 60.0f * sy };
    page.path_inp.base.set_rect(&page.path_inp, &tr2);
    SDL_FRect tr3 = { 5.0f * sx, 70.0f * sm, 630.0f * sx, 60.0f * sy };
    page.cfg_btn.base.set_rect(&page.cfg_btn, &tr3);
    SDL_FRect tr4 = { 5.0f * sx, 135.0f * sm, 630.0f * sx, 60.0f * sy };
    page.help_btn.base.set_rect(&page.help_btn, &tr4);
    for (size_t i = 0; i < 4; i++) {
        mobui_button* elem = (mobui_button*)page.elems[4 + i];
        SDL_FRect tr5 = { (5.0f + (615.0f / 4.0f + 5.0f) * (float)i) * sx, 200.0f * sm, 615.0f / 4.0f * sx, 60.0f * sy };
        elem->base.set_rect(elem, &tr5);
    }
    for (size_t i = 0; i < 4; i++) {
        mobui_button* elem = (mobui_button*)page.elems[8 + i];
        SDL_FRect tr5 = { (5.0f + (615.0f / 4.0f + 5.0f) * (float)i) * sx, 265.0f * sm, 615.0f / 4.0f * sx, 60.0f * sy };
        elem->base.set_rect(elem, &tr5);
    }
}

void mobui_copy_config(void) {
    void* in_f = h_fopen("default_mobile.conf", "rb");
    if (in_f == NULL)
        return;
    int64_t sz = h_fsize(in_f);
    if (sz <= 0) {
        fclose(in_f);
        return;
    }
    void* buf = h_malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(in_f);
        return;
    }
    h_fread(buf, 1, (size_t)sz, in_f);
    h_fclose(in_f);
    char out_path[1024 * 10];
    size_t path_inp_len = strlen(page.path_inp.text);
    memcpy(out_path, page.path_inp.text, path_inp_len);
    strcpy(out_path + path_inp_len, "/halfix.conf");
    void* out_f = h_fopen(out_path, "wb");
    if (out_f == NULL) {
        h_free(buf);
        return;
    }
    h_fwrite(buf, 1, (size_t)sz, out_f);
    h_fclose(out_f);
    h_free(buf);
}

void mobui_new_image(size_t size_mb) {
    void* buf = h_malloc(1024 * 1024 * 2);
    if (buf == NULL)
        return;
    char out_path[1024 * 10];
    size_t path_inp_len = strlen(page.path_inp.text);
    memcpy(out_path, page.path_inp.text, path_inp_len);
    strcpy(out_path + path_inp_len, "/hd_image.img");
    void* out_file = h_fopen(out_path, "wb");
    if (out_file == NULL) {
        h_free(buf);
        return;
    }
    memset(buf, 0, 1024 * 1024 * 2);
    for (size_t i = 0; i < (size_mb / 2); i++) {
        h_fwrite(buf, 1, 1024 * 1024 * 2, out_file);
    }
    h_fclose(out_file);
    h_free(buf);
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
                case SDL_TEXTINPUT: {
                    memcpy(page.path_inp.text + strlen(page.path_inp.text), ev.text.text, strlen(ev.text.text));
                    mobui_input_on_update(&page.path_inp);
                    mobui_on_path_input_update();
                    break;
                }
                case SDL_KEYDOWN: {
                    if (ev.key.keysym.sym == SDLK_BACKSPACE) {
                        if (page.path_inp.text[0] != '\0') {
                            page.path_inp.text[strlen(page.path_inp.text) - 1] = '\0';
                            mobui_input_on_update(&page.path_inp);
                            mobui_on_path_input_update();
                        }
                    }
#ifdef MOBILE_WIP
                    else if (ev.key.keysym.sym == SDLK_q) {
                        running = 0;
                    }
#endif
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
            page.allow_to_start = 1;
            page.allow_is_dir = 0;
            running = 0;
        }
        if (page.cfg_btn.was_pressed) {
            page.cfg_btn.was_pressed = 0;
            mobui_copy_config();
        }
        if (page.help_btn.was_pressed) {
            page.help_btn.was_pressed = 0;
            SDL_OpenURL("https://github.com/Pixelsuft/halfix/blob/main/ANDROID.md");
        }
        if (page.d50m_btn.was_pressed) {
            page.d50m_btn.was_pressed = 0;
            mobui_new_image(50);
        }
        if (page.d100m_btn.was_pressed) {
            page.d100m_btn.was_pressed = 0;
            mobui_new_image(100);
        }
        if (page.d250m_btn.was_pressed) {
            page.d250m_btn.was_pressed = 0;
            mobui_new_image(250);
        }
        if (page.d500m_btn.was_pressed) {
            page.d500m_btn.was_pressed = 0;
            mobui_new_image(500);
        }
        if (page.d1g_btn.was_pressed) {
            page.d1g_btn.was_pressed = 0;
            mobui_new_image(1024);
        }
        if (page.d2g_btn.was_pressed) {
            page.d2g_btn.was_pressed = 0;
            mobui_new_image(1024 * 2);
        }
        if (page.d4g_btn.was_pressed) {
            page.d4g_btn.was_pressed = 0;
            mobui_new_image(1024 * 4);
        }
        if (page.d6g_btn.was_pressed) {
            page.d6g_btn.was_pressed = 0;
            mobui_new_image(1024 * 6);
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
    page.allow_to_start = 0;
    page.allow_is_dir = 0;
    if (TTF_Init() < 0)
        return;
    fnt1 = TTF_OpenFont("fonts/liberationmonob.ttf", 30);
    fnt2 = TTF_OpenFont("fonts/liberationmonob.ttf", 16);
    if (fnt1 == NULL || fnt2 == NULL)
        return;
    win = display_get_handle(0);
    ren = display_get_handle(1);
    memset(page.elems, 0, sizeof(page.elems));
    mobui_init_button(&page.go_btn);
    mobui_button_set_text(&page.go_btn, "GO!");
    mobui_init_button(&page.cfg_btn);
    mobui_button_set_text(&page.cfg_btn, "Create default config here!");
    mobui_init_button(&page.d50m_btn);
    mobui_button_set_text(&page.d50m_btn, "50MB here");
    mobui_init_button(&page.d100m_btn);
    mobui_button_set_text(&page.d100m_btn, "100MB");
    mobui_init_button(&page.d250m_btn);
    mobui_button_set_text(&page.d250m_btn, "250MB");
    mobui_init_button(&page.d500m_btn);
    mobui_button_set_text(&page.d500m_btn, "500MB");
    mobui_init_button(&page.d1g_btn);
    mobui_button_set_text(&page.d1g_btn, "1GB");
    mobui_init_button(&page.d2g_btn);
    mobui_button_set_text(&page.d2g_btn, "2GB");
    mobui_init_button(&page.d4g_btn);
    mobui_button_set_text(&page.d4g_btn, "4GB");
    mobui_init_button(&page.d6g_btn);
    mobui_button_set_text(&page.d6g_btn, "6GB");
    mobui_init_button(&page.help_btn);
    mobui_button_set_text(&page.help_btn, "How to use me?");
    mobui_init_input(&page.path_inp);
#ifdef MOBILE_WIP
    strcpy(page.path_inp.text, ".");
#else
    strcpy(page.path_inp.text, "/storage/emulated/0/");
#endif
    mobui_input_on_update(&page.path_inp);
    mobui_on_path_input_update();
    page.elems[0] = (mobui_elem*)&page.go_btn;
    page.elems[1] = (mobui_elem*)&page.path_inp;
    page.elems[2] = (mobui_elem*)&page.cfg_btn;
    page.elems[3] = (mobui_elem*)&page.help_btn;
    page.elems[4] = (mobui_elem*)&page.d50m_btn;
    page.elems[5] = (mobui_elem*)&page.d100m_btn;
    page.elems[6] = (mobui_elem*)&page.d250m_btn;
    page.elems[7] = (mobui_elem*)&page.d500m_btn;
    page.elems[8] = (mobui_elem*)&page.d1g_btn;
    page.elems[9] = (mobui_elem*)&page.d2g_btn;
    page.elems[10] = (mobui_elem*)&page.d4g_btn;
    page.elems[11] = (mobui_elem*)&page.d6g_btn;
    mobui_place_elems();
    page.elem_count = 20;
#ifndef MOBILE_WIP
    // IDK which is right...
    SDL_AndroidRequestPermission("READ_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("permission.READ_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("android.permission.READ_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("WRITE_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("permission.WRITE_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("android.permission.WRITE_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("MANAGE_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("permission.MANAGE_EXTERNAL_STORAGE");
    SDL_AndroidRequestPermission("android.permission.MANAGE_EXTERNAL_STORAGE");
#endif
}

void mobui_quit(void) {
    TTF_CloseFont(fnt2);
    TTF_CloseFont(fnt1);
    TTF_Quit();
}

char* mobui_get_config_path(void) {
    if (!page.allow_to_start)
        return NULL;
    return (char*)page.path_inp.text;
}
