#include "util/mem.h"
#include "util/log.h"
#include "util/queue.h"
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
static SDL_SpinLock total_alloc_lock;
static size_t total_alloc = 0u;

static void *(SDLCALL *orig_malloc)(size_t size) = NULL;
static void *(SDLCALL *orig_calloc)(size_t nmemb, size_t size) = NULL;
static void *(SDLCALL *orig_realloc)(void *mem, size_t size) = NULL;
static void (SDLCALL *orig_free)(void *mem) = NULL;

bool mem_init() {
	SDL_AtomicLock(&total_alloc_lock);
	total_alloc = 0u;
	SDL_AtomicUnlock(&total_alloc_lock);

	SDL_GetMemoryFunctions(
		&orig_malloc,
		&orig_calloc,
		&orig_realloc,
		&orig_free
	);

	const int status = SDL_SetMemoryFunctions(
		mem_malloc,
		mem_calloc,
		mem_realloc,
		mem_free
	);
	if (status != 0) {
		log_printf("Failed to set SDL's memory functions to the mem_* functions\n");
	}
	return status == 0;
}

bool mem_deinit() {
	assert(orig_malloc != NULL);
	assert(orig_calloc != NULL);
	assert(orig_realloc != NULL);
	assert(orig_free != NULL);

	const int status = SDL_SetMemoryFunctions(
		orig_malloc,
		orig_calloc,
		orig_realloc,
		orig_free
	);
	if (status != 0) {
		log_printf("Failed to restore SDL's memory functions to their initial defaults\n");
	}
	return status == 0;
}

static bool total_alloc_add(void* const mem) {
	const size_t size = mem_sizeof(mem);
	SDL_AtomicLock(&total_alloc_lock);
	if (size > SIZE_MAX - total_alloc) {
		SDL_AtomicUnlock(&total_alloc_lock);
		log_printf("Error allocating memory: size > SIZE_MAX - total_alloc\n");
		return false;
	}
	total_alloc += size;
	SDL_AtomicUnlock(&total_alloc_lock);
	return true;
}

static void total_alloc_sub(void* const mem) {
	const size_t size = mem_sizeof(mem);
	SDL_AtomicLock(&total_alloc_lock);
	if (size > total_alloc) {
		SDL_AtomicUnlock(&total_alloc_lock);
		log_printf("Error freeing memory: size > total_alloc\n");
		abort();
	}
	total_alloc -= size;
	SDL_AtomicUnlock(&total_alloc_lock);
}

void* SDLCALL mem_malloc(size_t size) {
	if (size <= mem_left()) {
		void* const mem = malloc(size);
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
		void* const mem = calloc(nmemb, size);
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
		void* const realloc_mem = realloc(mem, size);
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
	free(mem);
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
	SDL_AtomicLock(&total_alloc_lock);
	const register size_t current_total_alloc = total_alloc;
	SDL_AtomicUnlock(&total_alloc_lock);
	return current_total_alloc;
}

#else
bool mem_init() {
	return true;
}

bool mem_deinit() {
	return true;
}

#endif

void* mem_lua_alloc(void* userdata, void* mem, size_t old_size, size_t new_size) {
	if (new_size > 0u) {
		return mem_realloc(mem, new_size);
	}
	else if (mem != NULL) {
		mem_free(mem);
	}
	return NULL;
}

// TODO: Make mem_bump_* thread-safe. Probably require that create/destroy are
// called in a "controlling" thread, where the object is passed down to other
// threads, with the controlling thread calling update once the subordinate
// threads' usage is guaranteed to be completed at the point update is called,
// then the controlling thread allowing the subordinate threads to use the
// object up until the next update, etc. The API can be "difficult" to use for
// better performance/safety, as it's intended for the engine internals managed
// by engine-developers, not engine-users.

struct mem_bump_object {
	uint8_t* chunk;
	size_t chunk_pos;
	size_t chunk_size;
	size_t next_size;

	// TODO: Looks like chunk_pos could just be advanced every allocation, then
	// if pos is beyond the current size in update, reallocate the chunk up to
	// pos in update, negating the need for bumps_waiting entirely.
	bool bumps_waiting;

	// TODO: This can probably can be a multiple-producer/single-consumer
	// conqueue.
	queue_object* bumps;
};

mem_bump_object* mem_bump_create(const size_t total_size) {
	mem_bump_object* const allocator = mem_malloc(sizeof(mem_bump_object));
	if (allocator == NULL) {
		return NULL;
	}

	if (total_size > 0u) {
		allocator->chunk = mem_malloc(total_size);
		if (allocator->chunk == NULL) {
			mem_free(allocator);
			return NULL;
		}
	}
	else {
		allocator->chunk = NULL;
	}
	allocator->chunk_size = total_size;
	allocator->chunk_pos = 0u;
	allocator->next_size = 0u;

	allocator->bumps = queue_create();
	if (allocator->bumps == NULL) {
		mem_free(allocator->chunk);
		mem_free(allocator);
		return NULL;
	}
	allocator->bumps_waiting = false;

	return allocator;
}

void mem_bump_destroy(mem_bump_object* const allocator) {
	assert(allocator != NULL);

	for (void* bump = queue_dequeue(allocator->bumps); bump != NULL; bump = queue_dequeue(allocator->bumps)) {
		mem_free(bump);
	}
	queue_destroy(allocator->bumps);
	mem_free(allocator->chunk);
	mem_free(allocator);
}

void* mem_bump_malloc(mem_bump_object* const allocator, const size_t size) {
	assert(allocator != NULL);
	assert(size > 0u);

	if (allocator->chunk_pos + size > allocator->chunk_size) {
		void* const bump = mem_malloc(size);
		if (bump == NULL) {
			return NULL;
		}
		allocator->next_size += size;
		if (!queue_enqueue(allocator->bumps, bump)) {
			mem_free(bump);
			return NULL;
		}
		allocator->bumps_waiting = true;
		return bump;
	}
	else {
		void* const bump = allocator->chunk + allocator->chunk_pos;
		allocator->chunk_pos += size;
		return bump;
	}
}

void* mem_bump_calloc(mem_bump_object* const allocator, const size_t nmemb, const size_t size) {
	assert(allocator != NULL);
	assert(size > 0u);

	if (allocator->chunk_pos + nmemb * size > allocator->chunk_size) {
		void* const bump = mem_calloc(nmemb, size);
		if (bump == NULL) {
			return NULL;
		}
		allocator->next_size += nmemb * size;
		if (!queue_enqueue(allocator->bumps, bump)) {
			mem_free(bump);
			return NULL;
		}
		allocator->bumps_waiting = true;
		return bump;
	}
	else {
		void* const bump = allocator->chunk + allocator->chunk_pos;
		memset(bump, 0, nmemb * size);
		allocator->chunk_pos += nmemb * size;
		return bump;
	}
}

bool mem_bump_update(mem_bump_object* const allocator) {
	if (allocator->bumps_waiting) {
		for (void* bump = queue_dequeue(allocator->bumps); bump != NULL; bump = queue_dequeue(allocator->bumps)) {
			mem_free(bump);
		}

		mem_free(allocator->chunk);
		allocator->chunk_size = allocator->next_size;
		allocator->chunk = mem_malloc(allocator->next_size);
		if (allocator->chunk == NULL) {
			return false;
		}

		allocator->bumps_waiting = false;
	}

	allocator->next_size = 0u;
	return true;
}
