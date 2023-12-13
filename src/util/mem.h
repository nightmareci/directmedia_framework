#pragma once
/*
 * MIT License
 *
 * Copyright (c) 2023 Brandon McGriff <nightmareci@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "SDL_stdinc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#define lengthof(array) (sizeof((array)) / sizeof(*(array)))
#define lengthoffield(type, array_field) sizeof(((type*)NULL)->array_field) / sizeof(*((type*)NULL)->array_field)
#define sizeoffield(type, field) sizeof(((type*)NULL)->field)

typedef enum bytes_per_type {
	/*
	 * Use the minimum-required-size UINT*_C() function-like macros here for
	 * each constant. And don't include constants here that go beyond the range
	 * of SIZE_MAX; that would result in bytes_per_type being a larger type than
	 * size_t, requiring completely unnecessary conversions between size_t and
	 * bytes_per_type with all BYTES_PER_* constants.
	 */

#if SIZE_MAX >= UINT32_MAX
	BYTES_PER_MEBIBYTE = UINT32_C(1048576),
#endif

	/*
	 * This just ensures the BYTES_PER_* constants' range matches size_t, so
	 * direct usage of the bytes_per_type is fully compatible with
	 * size_t/pointer math, likely resulting in no type conversion in the
	 * generated machine code between bytes_per_type and size_t.
	 */
	BYTES_PER_MAX = SIZE_MAX
} bytes_per_type;

/*
 * TODO: Change from using a compile-time option for enabling/disabling memory
 * debugging to a runtime option, that must be passed in before startup. That
 * way, the prebuilt release builds only capable of running Lua script code can
 * have memory debugging enabled by users, without having to rebuild with memory
 * debugging enabled. And use separate functions for no-debugging and debugging,
 * so that the unnecessary overhead of conditional checks aren't necessary when
 * debugging is off; just use function pointers to choose the debugging mode.
 */

/*
 * Initializes the memory subsystem. Call first thing at startup, before any
 * dynamic memory allocations, in the main thread, before any threads have been
 * created.
 */
bool mem_init();

#ifdef MEM_DEBUG
void* SDLCALL mem_malloc(size_t size);
void* SDLCALL mem_calloc(size_t nmemb, size_t size);
void* SDLCALL mem_realloc(void* mem, size_t size);
void SDLCALL mem_free(void* mem);

void* mem_aligned_alloc(size_t alignment, size_t size);
void mem_aligned_free(void* mem);

/*
 * Returns the current amount of dynamically-allocated memory.
 */
size_t mem_total();

#else
#define mem_malloc SDL_malloc
#define mem_calloc SDL_calloc
#define mem_realloc SDL_realloc
#define mem_free SDL_free

/*
 * Standard C aligned allocation isn't supported by Windows CRT due to Windows'
 * standard C free being unable to reclaim aligned-allocated memory, so we have
 * to use a separate free function for aligned allocations for portability.
 */
#ifdef _WIN32
#define mem_aligned_alloc(alignment, size) _aligned_malloc((size), (alignment))
#define mem_aligned_free(mem) _aligned_free((mem))

#else
#define mem_aligned_alloc(alignment, size) aligned_alloc((alignment), (size))
#define mem_aligned_free(mem) free((mem))

#endif

#define mem_total() ((size_t)0)

#endif

/*
 * mem_sizeof returns the actual size of the mem_*-functions' allocated memory.
 * The returned size is greater than or equal to the size originally requested
 * at the time of allocation; do not make use of the additional memory beyond
 * the originally requested size, as that's bad programming practice.
 */

#if defined(_WIN32)
#include <malloc.h>
#define mem_sizeof(mem) _msize(mem)

/*
 * Must come before __GNUC__. Apple Clang defines __GNUC__ but doesn't provide
 * <malloc.h> nor malloc_usable_size.
 */
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#define mem_sizeof(mem) malloc_size(mem)

/*
 * This probably should only ever be used with real GCC. Maybe GCC-like
 * compilers other than Apple Clang work with it?
 */
#elif defined(__GNUC__)
#include <malloc.h>
#define mem_sizeof(mem) malloc_usable_size(mem)

#else
#define mem_sizeof(mem) ((size_t)0)

#endif

/*
 * Returns the amount of physical memory left available for allocation, as a
 * count of bytes. "Physical" means the memory is only main memory, not
 * including portions of virtual memory in persistent storage. The intended use
 * case is for checking available memory in performance-focused applications,
 * such as game development, especially game development on very
 * memory-constrained platforms. If SIZE_MAX is returned by mem_left, either the
 * current platform doesn't have a real implementation to check for available
 * memory, or the actual amount of available memory may be larger than SIZE_MAX,
 * but can't be indicated by a single size_t-type count of bytes; you might
 * never encounter a situation where there's a real possibility of SIZE_MAX
 * being returned for a given platform/application. Since the operating system,
 * other processes, and other threads can themselves allocate and free memory
 * concurrently with the thread that called mem_left, every call of mem_left can
 * return a different value. The returned value might only be an estimate, and
 * might even not be truly reflective of the amount of memory the application
 * could reasonably allocate without crashing nor disrupting other processes.
 */
size_t mem_left();

/*
 * Allocation function suitable for passing to lua_newstate().
 */
void* mem_lua_alloc(void* userdata, void* mem, size_t old_size, size_t new_size);

/*
 * TODO: Implement and use a game-development-optimized bump memory allocator,
 * that accumulates allocation request sizes between update points, returning
 * typical mem_malloc allocations when the bump allocation memory buffer is
 * exhausted, extending the bump memory buffer upon calling some "update"
 * function (mem_bump_update?); that would result in the bump memory allocator
 * magically producing good performance without requiring the developer
 * preallocate sufficient memory ahead of time (though provide that as an
 * option).
 */

// TODO: Finalize the bump allocator code and use it once finalized.

typedef struct mem_bump_object mem_bump_object;

mem_bump_object* mem_bump_create(const size_t total_size);
void mem_bump_destroy(mem_bump_object* const allocator);
void* mem_bump_malloc(mem_bump_object* const allocator, const size_t size);
void* mem_bump_calloc(mem_bump_object* const allocator, const size_t nmemb, const size_t size);
bool mem_bump_update(mem_bump_object* const allocator);
