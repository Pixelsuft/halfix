#ifdef SDL2_INC_DIR
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

typedef struct {
    SDL_FRect rect;
    void (*on_down)(void* this, SDL_FPoint* pos);
    void (*on_up)(void* this, SDL_FPoint* pos);
    void (*on_move)(void* this, SDL_FPoint* dt);
    void (*set_rect)(void* this, SDL_FRect* rect);
    void (*draw)(void* this);
    void (*destroy)(void* this);
} mobui_elem;

typedef struct {
    mobui_elem base;
    SDL_Texture* tex;
    char* text;
} mobui_button;

void mobui_init_elem(void* elem);
void mobui_init_button(mobui_button* this);
void mobui_init(void);
void mobui_quit(void);
void mobui_run_main(void);
