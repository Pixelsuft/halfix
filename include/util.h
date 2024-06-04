#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef SDL2_BUILD
#if defined(_WIN32) && !defined(_MSC_VER)
#define SDL_MAIN_HANDLED
#endif
#ifdef SDL2_INC_DIR
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
#endif

#define UNUSED(x) (void)(x)

// This is to make Visual Studio Code not complain
#if defined(__linux__) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS 0x20
#endif

#if !defined(_WIN32)
#define PREFER_SDL2
#endif
// #define PREFER_STD
// #define FILES_WIN32_USE_ANSI

#if defined(SDL2_BUILD) && !defined(PREFER_STD)
#define h_printf(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define h_fprintf(out_file, ...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define h_sprintf(buf, ...) SDL_snprintf(buf, 6144, __VA_ARGS__)
#else
#define h_printf printf
#define h_fprintf fprintf
#define h_sprintf sprintf
#endif

void* aalloc(int size, int align);
void afree(void* ptr);

// #define LOGGING_ENABLED

//#define LOG(component, x, ...) h_fprintf(stderr, "[" component "] " x, ##__VA_ARGS__)
//#define LOG(component, x, ...) h_printf("[" component "] " x, ##__VA_ARGS__)
// We have the Halfix abort function (which releases the mouse and optionally writes out the event log) and then the real abort function to appease the compiler.
#if 0
#define FATAL(component, x, ...)                              \
    do {                                                      \
        h_fprintf(stderr, "[" component "] " x, ##__VA_ARGS__); \
        ABORT();                                              \
        abort();                                              \
    } while (0)
#else
#define FATAL(component, x, ...)                              \
    do {                                                      \
        h_fprintf(stderr, "[" component "] " x, ##__VA_ARGS__); \
        break;                                                \
    } while (0)
#endif
#define NOP() \
    do {      \
    } while (0)

#define ABORT() util_abort()
#define debugger util_debug()

#ifdef LOGGING_ENABLED
#define LOG(component, x, ...) h_fprintf(stdout, "[" component "] " x, ##__VA_ARGS__)
#else
#define LOG(component, x, ...) NOP()
#endif

typedef uint64_t itick_t;
itick_t get_now(void);
extern uint32_t ticks_per_second;

void* h_malloc(size_t size);
void* h_calloc(size_t elem_count, size_t elem_size);
void* h_realloc(void* ptr, size_t new_size);
void h_free(void* ptr);

// TODO
void* h_fopen(const char* fp, const char* mode);
int h_fclose(void* file);
size_t h_fread(void* buf, size_t elem_size, size_t elem_count, void* file);
size_t h_fwrite(const void* buf, size_t elem_size, size_t elem_count, void* file);
int64_t h_ftell(void* file);
int64_t h_fsize(void* file);
int h_fseek(void* file, int64_t offset, int origin);

// Functions that mess around with timing
void add_now(itick_t a);

// Quick Malloc API
void qmalloc_init(void);
void* qmalloc(int size, int align);
void qfree(void);

void util_debug(void);
void util_abort(void);

#endif