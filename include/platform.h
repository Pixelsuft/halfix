// Platform-specific defines
// Includes things for alloca, O_BINARY, etc.
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
#include <malloc.h>
#else
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(BSD_BUILD)
#include <stdlib.h>
#else
#include <alloca.h>
#endif
// This flag is required for MSVCRT to read binary files normally.
#define O_BINARY 0 
#endif

#if __GNUC__
#  define PACKED(x) x __attribute__((packed))
#elif _MSC_VER
#  define PACKED(x) __pragma(pack(push, 1)) x __pragma(pack(pop))
#else
#  error Do not know how to pack structs on this platform
#endif

#if __GNUC__
#  define ALIGNED(x) __attribute__((aligned(x)))
#elif _MSC_VER
#  define ALIGNED(x) __declspec(align(x))
#else
#  error Do not know how to specify alignment on this platform
#endif

#endif