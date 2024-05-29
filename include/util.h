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

void* aalloc(int size, int align);
void afree(void* ptr);

// #define LOGGING_ENABLED

//#define LOG(component, x, ...) fprintf(stderr, "[" component "] " x, ##__VA_ARGS__)
//#define LOG(component, x, ...) printf("[" component "] " x, ##__VA_ARGS__)
// We have the Halfix abort function (which releases the mouse and optionally writes out the event log) and then the real abort function to appease the compiler.
#if 0
#define FATAL(component, x, ...)                              \
    do {                                                      \
        fprintf(stderr, "[" component "] " x, ##__VA_ARGS__); \
        ABORT();                                              \
        abort();                                              \
    } while (0)
#else
#define FATAL(component, x, ...)                              \
    do {                                                      \
        fprintf(stderr, "[" component "] " x, ##__VA_ARGS__); \
        break;                                                \
    } while (0)
#endif
#define NOP() \
    do {      \
    } while (0)

#define ABORT() util_abort()
#define debugger util_debug()

#ifdef LOGGING_ENABLED
#define LOG(component, x, ...) fprintf(stdout, "[" component "] " x, ##__VA_ARGS__)
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
// void* h_fopen(char* fp);

// Functions that mess around with timing
void add_now(itick_t a);

// Quick Malloc API
void qmalloc_init(void);
void* qmalloc(int size, int align);
void qfree(void);

void util_debug(void);
void util_abort(void);

#endif