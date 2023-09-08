#include "util/util.h"
#include "SDL_atomic.h"

// TODO: Support more than Linux here?
#if defined(__linux__)
#include <unistd.h>

size_t mem_left() {
	const long pages = sysconf(_SC_AVPHYS_PAGES);
	const long pagesize = sysconf(_SC_PAGESIZE);

	// Handle errors.
	if (pages < 0 || pagesize < 0) {
		return 0u;
	}
	// Handle overflow (documented as possible).
	else if (pages >= SIZE_MAX / pagesize) {
		return SIZE_MAX;
	}
	// Success, return memory left.
	else {
		return (size_t)pages * (size_t)pagesize;
	}
}

#elif defined(__APPLE__)
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>

size_t mem_left() {
	vm_size_t page_size;
	vm_statistics64_data_t stats;
	mach_msg_type_number_t count = sizeof(stats) / sizeof(natural_t);
	mach_port_t port = mach_host_self();

	// Handle errors.
	if (
		host_page_size(port, &page_size) != KERN_SUCCESS ||
		host_statistics64(port, HOST_VM_INFO, (host_info64_t)&stats, &count) != KERN_SUCCESS
	) {
		return 0u;
	}
	// Handle overflow.
	else if (stats.free_count >= SIZE_MAX / page_size) {
		return SIZE_MAX;
	}
	// Success, return memory left.
	else {
		return (size_t)stats.free_count * (size_t)page_size;
	}
}

#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

size_t mem_left() {
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);

	// Handle errors.
	if (!GlobalMemoryStatusEx(&statex)) {
		return 0u;
	}
	// Success, return memory left.
	else {
		return (size_t)statex.ullAvailPhys;
	}
}

#else
/*
 * Since there's no standard C way to get available physical memory, just
 * default to the maximum amount it could ever be. Of course SIZE_MAX can be
 * always greater than the amount of physical memory, but this at least allows
 * code using mem_left to still function on platforms that don't have a real
 * implementation.
 */

size_t mem_left() {
	return SIZE_MAX;
}

#endif

#ifdef MEM_DEBUG
static void* total_alloc = 0;

static void *(SDLCALL *orig_malloc)(size_t size) = NULL;
static void *(SDLCALL *orig_calloc)(size_t nmemb, size_t size) = NULL;
static void *(SDLCALL *orig_realloc)(void *mem, size_t size) = NULL;
static void (SDLCALL *orig_free)(void *mem) = NULL;

bool mem_init() {
	SDL_AtomicSetPtr((void**)&total_alloc, NULL);

	SDL_GetMemoryFunctions(
		&orig_malloc,
		&orig_calloc,
		&orig_realloc,
		&orig_free
	);

	return SDL_SetMemoryFunctions(
		mem_malloc,
		mem_calloc,
		mem_realloc,
		mem_free
	) == 0;
}

/*
 * ABA case in the total_alloc_* functions' solution for atomic adds to
 * pointers with SDL's atomics:
 *
 * There can be an ABA case in a thread A, between the atomic read of
 * total_alloc and CAS by thread A, where other thread(s) change total_alloc in
 * the mean time between read and CAS, but total_alloc ends up as the same
 * value as thread A initially read at the point of thread A's CAS; this is not
 * an "unacceptable ABA case", because such a case still preserves the desired
 * invariant, that total_alloc always have every allocation's bytes accounted
 * for eventually, with no bytes missed.
 */

static bool total_alloc_add(void* const mem) {
	const size_t size = mem_sizeof(mem);
	size_t old_total;
	do {
		old_total = (size_t)(uintptr_t)SDL_AtomicGetPtr(&total_alloc);
		if (size > SIZE_MAX - old_total) {
			fprintf(stderr, "size > SIZE_MAX - old_total\n");
			fflush(stderr);
			return false;
		}
	} while (!SDL_AtomicCASPtr(&total_alloc, (void*)(uintptr_t)old_total, (void*)(uintptr_t)(old_total + size)));
	return true;
}

static void total_alloc_sub(void* const mem) {
	const size_t size = mem_sizeof(mem);
	size_t old_total;
	do {
		old_total = (size_t)(uintptr_t)SDL_AtomicGetPtr(&total_alloc);
		if (size > old_total) {
			fprintf(stderr, "size > old_total\n");
			fflush(stderr);
			abort();
		}
	} while (!SDL_AtomicCASPtr(&total_alloc, (void*)(uintptr_t)old_total, (void*)(uintptr_t)(old_total - size)));
}

void* SDLCALL mem_malloc(size_t size) {
	if (size <= mem_left()) {
		void* const mem = orig_malloc(size);
		if (mem != NULL) {
			if (!total_alloc_add(mem)) {
				mem_free(mem);
				return NULL;
			}
		}
		return mem;
	}
	else {
		return NULL;
	}
}

void* SDLCALL mem_calloc(size_t nmemb, size_t size) {
	if (nmemb <= mem_left() / size) {
		void* const mem = orig_calloc(nmemb, size);
		if (mem != NULL) {
			if (!total_alloc_add(mem)) {
				mem_free(mem);
				return NULL;
			}
		}
		return mem;
	}
	else {
		return NULL;
	}
}

void* SDLCALL mem_realloc(void* mem, size_t size) {
	if (size <= mem_left()) {
		if (mem != NULL) {
			const size_t old_size = mem_sizeof(mem);
			if (old_size == 0u) {
				return NULL;
			}
			total_alloc_sub(mem);
		}
		void* const realloc_mem = orig_realloc(mem, size);
		if (realloc_mem != NULL) {
			if (!total_alloc_add(realloc_mem)) {
				if (realloc_mem != mem) {
					mem_free(realloc_mem);
				}
				return NULL;
			}
		}
		return realloc_mem;
	}
	else {
		return NULL;
	}
}

void SDLCALL mem_free(void* mem) {
	total_alloc_sub(mem);
	orig_free(mem);
}

/*
 * Standard C aligned allocation isn't supported by Windows CRT due to Windows'
 * standard C free being unable to reclaim aligned-allocated memory, so we have
 * to use real_aligned_free for portability.
 */
#ifdef _WIN32
#define real_aligned_alloc(alignment, size) _aligned_malloc((size), (alignment))
#define real_aligned_free(mem) _aligned_free((mem))

#else
#define real_aligned_alloc(alignment, size) aligned_alloc((alignment), (size))
#define real_aligned_free(mem) free((mem))

#endif

void* mem_aligned_alloc(size_t alignment, size_t size) {
	if (size <= mem_left()) {
		void* const mem = real_aligned_alloc(alignment, size);
		if (mem != NULL) {
			if (!total_alloc_add(mem)) {
				real_aligned_free(mem);
				return NULL;
			}
		}
		return mem;
	}
	else {
		return NULL;
	}
}

void mem_aligned_free(void* mem) {
	total_alloc_sub(mem);
	real_aligned_free(mem);
}

size_t mem_total() {
	const size_t total = (size_t)(uintptr_t)SDL_AtomicGetPtr(&total_alloc);
	SDL_MemoryBarrierAcquire();
	return total;
}

#else
bool mem_init() {
	return true;
}

#endif
