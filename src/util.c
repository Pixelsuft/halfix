// All platform-dependent stuff

#include "util.h"
#include "cpuapi.h"
#include "display.h"
#include "state.h"
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

//#define REALTIME_TIMING

#ifdef REALTIME_TIMING
#include <sys/time.h>
#endif

#define QMALLOC_SIZE 1 << 20

// #define PREFER_SDL2
// #define PREFER_STD

static void* qmalloc_data;
static int qmalloc_usage, qmalloc_size;

static void** qmalloc_slabs = NULL;
static int qmalloc_slabs_size = 0;

#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
static HANDLE h_heap = NULL;
#define HEAP_FLAGS HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY
#endif

void* h_malloc(size_t size) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    if (h_heap == NULL) {
        // TODO: maybe use own heap?
        h_heap = GetProcessHeap();
    }
    return HeapAlloc(h_heap, HEAP_FLAGS, size);
#elif defined(SDL2) && !defined(PREFER_STD)
    return SDL_malloc(size);
#else
    return malloc(size);
#endif
}

void* h_calloc(size_t elem_count, size_t elem_size) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    return h_malloc(elem_count * elem_size);
#elif defined(SDL2) && !defined(PREFER_STD)
    return SDL_calloc(elem_count, elem_size);
#else
    return calloc(elem_count, elem_size);
#endif
}

void* h_realloc(void* ptr, size_t new_size) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    return HeapReAlloc(h_heap, HEAP_FLAGS, ptr, new_size);
#elif defined(SDL2) && !defined(PREFER_STD)
    return SDL_realloc(ptr, new_size);
#else
    return realloc(ptr, new_size);
#endif
}

void h_free(void* ptr) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    HeapFree(h_heap, HEAP_FLAGS, ptr);
#elif defined(SDL2) && !defined(PREFER_STD)
    SDL_free(ptr);
#else
    free(ptr);
#endif
}

static void qmalloc_slabs_resize(void)
{
    qmalloc_slabs = h_realloc(qmalloc_slabs, qmalloc_slabs_size * sizeof(void*));
}
void qmalloc_init(void)
{
    if (qmalloc_slabs == NULL) {
        qmalloc_slabs_size = 1;
        qmalloc_slabs = h_malloc(8);
        qmalloc_slabs_resize();
    }
    qmalloc_data = h_malloc(QMALLOC_SIZE);
    qmalloc_usage = 0;
    qmalloc_size = QMALLOC_SIZE;
    qmalloc_slabs[qmalloc_slabs_size - 1] = qmalloc_data;
}

void* qmalloc(int size, int align)
{
    if (!align)
        align = 4;
    align--;
    qmalloc_usage = (qmalloc_usage + align) & ~align;

    void* ptr = qmalloc_usage + (uint8_t *)qmalloc_data;
    qmalloc_usage += size;
    if (qmalloc_usage >= qmalloc_size) {
        LOG("QMALLOC", "Creating additional slab\n");
        qmalloc_init();
        return qmalloc(size, align);
    }

    return ptr;
}

void qfree(void)
{
    for (int i = 0; i < qmalloc_slabs_size; i++) {
        h_free(qmalloc_slabs[i]);
    }
    h_free(qmalloc_slabs);
    qmalloc_slabs = NULL;
    qmalloc_init();
}

struct aalloc_info {
    void* actual_ptr;
    uint8_t data[0];
};

void* aalloc(int size, int align)
{
    int adjusted = align - 1;
    void *actual = h_calloc(1, sizeof(void *) + size + adjusted);
    struct aalloc_info *ai = (struct aalloc_info*)((uint8_t *)((void*)((uintptr_t)((uint8_t *)actual + sizeof(void*) + adjusted) & ~adjusted)) - sizeof(void *));
    ai->actual_ptr = actual;
    return ((uint8_t *)ai) + sizeof(void *);
}
void afree(void* ptr)
{
    struct aalloc_info* a = (struct aalloc_info*)((uint8_t *)ptr - 1);
    h_free(a->actual_ptr);
}

// Timing functions

// TODO: Make this configurable
#ifndef REALTIME_TIMING
uint32_t ticks_per_second = 50000000;
#else
uint32_t ticks_per_second = 1000000;
itick_t base;
#endif

void set_ticks_per_second(uint32_t value)
{
    ticks_per_second = value;
}

static itick_t tick_base;

void util_state(void)
{
    struct bjson_object* obj = state_obj("util", 1);
    state_field(obj, 8, "tick_base", &tick_base);
}

// "Constant" source of ticks, in either usec or CPU instructions
itick_t get_now(void)
{
#ifndef REALTIME_TIMING
    return tick_base + cpu_get_cycles();
#else
    // XXX
    struct timeval tv;
    gettimeofday(&tv, NULL);
    itick_t hi = (itick_t)tv.tv_sec * (itick_t)1000000 + (itick_t)tv.tv_usec;
    if (!base)
        base = hi;
    return hi - base;
#endif
}

// A function to mess with the emulator's sense of time
void add_now(itick_t a)
{
    tick_base += a;
}

void util_debug(void)
{
    display_release_mouse();
#ifndef EMSCRIPTEN
#if ENABLE_BREAKPOINTS
#ifdef _MSC_VER
    __debugbreak();
#else
    __asm__("int3");
#endif
#endif
#else
    printf("Breakpoint reached -- aborting\n");
    abort();
#endif
}
void util_abort(void)
{
    display_release_mouse();
    abort();
}