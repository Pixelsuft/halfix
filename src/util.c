// All platform-dependent stuff

#include "util.h"
#include "cpuapi.h"
#include "display.h"
#include "state.h"
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#if defined(MOBILE_BUILD) && !defined(MOBILE_WIP)
#include <SDL.h>
#endif

//#define REALTIME_TIMING

#ifdef REALTIME_TIMING
#include <sys/time.h>
#endif

#define QMALLOC_SIZE 1 << 20

static void* qmalloc_data;
static int qmalloc_usage, qmalloc_size;

static void** qmalloc_slabs = NULL;
static int qmalloc_slabs_size = 0;

#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
static HANDLE h_heap = NULL;
#define HEAP_FLAGS HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE
#endif

void* h_malloc(size_t size) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    if (h_heap == NULL) {
        // TODO: maybe use own heap?
        h_heap = GetProcessHeap();
    }
    return HeapAlloc(h_heap, HEAP_FLAGS, size);
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_malloc(size);
#else
    return malloc(size);
#endif
}

void* h_calloc(size_t elem_count, size_t elem_size) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    return h_malloc(elem_count * elem_size);
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_calloc(elem_count, elem_size);
#else
    return calloc(elem_count, elem_size);
#endif
}

void* h_realloc(void* ptr, size_t new_size) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    return HeapReAlloc(h_heap, HEAP_FLAGS, ptr, new_size);
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_realloc(ptr, new_size);
#else
    return realloc(ptr, new_size);
#endif
}

void h_free(void* ptr) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    HeapFree(h_heap, HEAP_FLAGS, ptr);
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    SDL_free(ptr);
#else
    free(ptr);
#endif
}

void* h_fopen(const char* fp, const char* mode) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    int can_write = mode[0] == 'r' && mode[1] == 'b' && mode[2] == '+'; // Hack
#ifdef FILES_WIN32_USE_ANSI
    HANDLE res = CreateFileA(
        fp, GENERIC_READ | (can_write ? GENERIC_WRITE : 0), FILE_SHARE_READ, NULL,
        OPEN_EXISTING,  FILE_ATTRIBUTE_NORMAL, NULL
    );
#else
    int count = MultiByteToWideChar(CP_UTF8, 0, fp, (int)strlen(fp), NULL, 0);
    if (count <= 0)
        return NULL;
    wchar_t* fp_buf = h_malloc((size_t)(count + 1) * sizeof(wchar_t));
    if (fp_buf == NULL)
        return NULL;
    int encode_res = MultiByteToWideChar(CP_UTF8, 0, fp, (int)strlen(fp), fp_buf, count);
    if (encode_res <= 0) {
        h_free(fp_buf);
        return NULL;
    }
    fp_buf[encode_res] = L'\0';
    HANDLE res = CreateFileW(
        fp_buf, GENERIC_READ | (can_write ? GENERIC_WRITE : 0), FILE_SHARE_READ, NULL,
        OPEN_EXISTING,  FILE_ATTRIBUTE_NORMAL, NULL
    );
    h_free(fp_buf);
#endif
    if (res == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    return res;
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_RWFromFile(fp, mode);
#else
    return fopen(fp, mode);
#endif
}

int h_fclose(void* file) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    return CloseHandle(file) != 0;
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_RWclose(file);
#else
    return fclose(file);
#endif
}

size_t h_fread(void* buf, size_t elem_size, size_t elem_count, void* file) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    DWORD bytes_read = 0;
    if (ReadFile(file, buf, (DWORD)(elem_size * elem_count), &bytes_read, NULL) == FALSE) {
        return 0;
    }
    return (size_t)bytes_read / elem_size;
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_RWread(file, buf, elem_size, elem_count);
#else
    return fread(buf, elem_size, elem_count, file);
#endif
}

size_t h_fwrite(const void* buf, size_t elem_size, size_t elem_count, void* file) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    DWORD bytes_written = 0;
    if (WriteFile(file, buf, (DWORD)(elem_size * elem_count), &bytes_written, NULL) == FALSE) {
        return 0;
    }
    return (size_t)bytes_written / elem_size;
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_RWwrite(file, buf, elem_size, elem_count);
#else
    return fread((void*)buf, elem_size, elem_count, file);
#endif
}

int64_t h_ftell(void* file) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    // seek(file, 0, OMG_FILE_SEEK_CUR);
    LARGE_INTEGER res_buf;
    LARGE_INTEGER inp_val;
    inp_val.QuadPart = (LONGLONG)0;
    if (!SetFilePointerEx(file, inp_val, &res_buf, FILE_CURRENT)) {
        return -2;
    }
    return (int64_t)res_buf.QuadPart;
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_RWtell(file);
#else
    return (int64_t)ftell(file);
#endif
}

int64_t h_fsize(void* file) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    DWORD size_high = 0;
    DWORD size_low = GetFileSize(file, &size_high);
    if ((size_low == INVALID_FILE_SIZE) && (size_high == 0) && (GetLastError() != NO_ERROR)) {
        return -2;
    }
    return ((int64_t)size_high << 32) | (int64_t)size_low;
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    return SDL_RWsize(file);
#else
    long cur_pos = ftell(file);
    if (cur_pos < 0)
        return -1;
    fseek(file, 0, SEEK_END);
    int64_t res = (int64_t)ftell(file);
    fseek(file, cur_pos, SEEK_SET);
    return res;
#endif
}

int h_fseek(void* file, int64_t offset, int origin) {
#if defined(_WIN32) && !defined(PREFER_SDL2) && !defined(PREFER_STD)
    LARGE_INTEGER res_buf;
    LARGE_INTEGER inp_val;
    inp_val.QuadPart = (LONGLONG)offset;
    if (!SetFilePointerEx(file, inp_val, &res_buf, (
        (origin == SEEK_END) ? FILE_END : ((origin == SEEK_CUR) ? FILE_CURRENT : FILE_BEGIN)
    ))) {
        return -2;
    }
    return 0;
#elif defined(PREFER_SDL2) && !defined(PREFER_STD)
    if (origin == SEEK_CUR)
        origin = RW_SEEK_CUR;
    else if (origin == SEEK_END)
        origin = RW_SEEK_END;
    else
        origin = RW_SEEK_SET;
    return SDL_RWseek(file, offset, origin);
#else
    return fseek(file, (long)offset, origin);
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
    h_printf("Breakpoint reached -- aborting\n");
    abort();
#endif
}
void util_abort(void)
{
    display_release_mouse();
    abort();
}