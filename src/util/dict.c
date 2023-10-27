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

#include "util/dict.h"
#include "util/mem.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

/*
 * This is the 64-bit Fowler/Noll/Vo FNV-1a hash algorithm.
 */
static inline uint64_t hash(const void* const data, const size_t size) {
	const uint8_t* p = (uint8_t*)data;
	uint64_t out = UINT64_C(0xCBF29CE484222325);
	for (const uint8_t* const end = (uint8_t*)data + size; p < end; p++) {
		out ^= *p;
		out *= UINT64_C(0x100000001B3);
	}
	return out;
}

/*
 * The type used for key-value entries in dictionaries.
 */
typedef struct entry_object entry_object;
struct entry_object {
	entry_object* next;	

	void* key;
	size_t key_size;
	void* value;
	size_t value_size;
	bool (* value_destroy)(void* const data);
	bool (* value_copy)(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size);
};

struct dict_object {
	size_t size;
	size_t count;
	entry_object** entries;
};

dict_object* dict_create(const size_t size) {
	if (size == 0u) {
		return NULL;
	}

	dict_object* dict = mem_malloc(sizeof(dict_object));
	if (dict == NULL) {
		return NULL;
	}

	dict->size = size;
	dict->count = 0u;
	dict->entries = mem_calloc(size, sizeof(entry_object*));
	if (dict->entries == NULL) {
		mem_free(dict);
		return NULL;
	}

	return dict;
}

bool dict_destroy(dict_object* const dict) {
	if (dict != NULL) {
		size_t count = dict->count;
		for (size_t i = 0u; count > 0u; i++) {
			for (entry_object* entry = dict->entries[i], * next = NULL; entry != NULL; entry = next) {
				next = entry->next;
				mem_free(entry->key);
				if (entry->value_destroy != NULL) {
					if (!entry->value_destroy(entry->value)) {
						return false;
					}
				}
				else {
					mem_free((void*)entry->value);
				}
				mem_free(entry);
				count--;
			}
		}

		mem_free(dict->entries);
		mem_free(dict);
	}
	return true;
}

dict_object* dict_copy(dict_object* const dict) {
	dict_object* const copy = dict_create(dict->size);

	for (size_t i = 0u; i < dict->size; i++) {
		for (entry_object* entry = dict->entries[i]; entry != NULL; entry = entry->next) {
			void* value = entry->value;
			size_t value_size = entry->value_size;
			if (entry->value_copy != NULL) {
				if (!entry->value_copy(entry->value, entry->value_size, &value, &value_size)) {
					dict_destroy(copy);
					return NULL;
				}
			}
			if (!dict_set(copy, entry->key, entry->key_size, value, value_size, entry->value_destroy, entry->value_copy)) {
				if (entry->value_destroy != NULL) {
					entry->value_destroy(value);
				}
				dict_destroy(copy);
				return NULL;
			}
		}
	}

	return copy;
}

/*
 * Get an entry in the dictionary. entry_found is set to the entry if an entry
 * with the requested key is found, otherwise entry_found is set to NULL if an
 * entry with the requested key was not found. If remove is true, the found
 * entry is removed from the dictionary, allowing entry_found to be moved to
 * another dictionary; if remove is false, the found entry is left in the
 * dictionary.
 */
static inline void get(dict_object* const dict, const void* const key, const size_t key_size, const bool remove, entry_object** entry_found) {
	if (dict->size == 0u || dict->count == 0u) {
		*entry_found = NULL;
	}
	else {
		const size_t i = hash(key, key_size) % dict->size;
		for (entry_object* entry = dict->entries[i], * prev = NULL; entry != NULL; prev = entry, entry = entry->next) {
			if (key_size == entry->key_size && memcmp(entry->key, key, key_size) == 0) {
				*entry_found = entry;
				if (remove) {
					if (prev != NULL) {
						prev->next = entry->next;
					}
					else if (entry->next != NULL) {
						dict->entries[i] = entry->next;
					}
					else {
						dict->entries[i] = NULL;
					}
					dict->count--;
				}
				return;
			}
		}
		*entry_found = NULL;
	}
}

bool dict_get(
	dict_object* const dict,
	const void* const key, const size_t key_size,
	void** const value, size_t* const value_size
) {
	if (dict == NULL || key == NULL || key_size == 0u || value == NULL) {
		return false;
	}

	entry_object* entry_found;
	get(dict, key, key_size, false, &entry_found);
	if (entry_found != NULL) {
		if (value != NULL) {
			*value = entry_found->value;
		}
		if (value_size != NULL) {
			*value_size = entry_found->value_size;
		}
		return true;
	}
	else {
		return false;
	}
}

/*
 * Grows the dictionary when reducing the load factor is needed.
 */
static inline bool grow(dict_object* const dict) {
	dict_object* dict2 = dict_create(dict->size * 2u);
	if (dict2 == NULL) {
		return false;
	}

	for (size_t i = 0u; i < dict->size; i++) {
		for (entry_object* entry = dict->entries[i]; entry != NULL; entry = entry->next) {
			if (!dict_set(dict2, entry->key, entry->key_size, entry->value, entry->value_size, entry->value_destroy, entry->value_copy)) {
				dict_destroy(dict2);
				return false;
			}
		}
	}

	dict_object temp = *dict;
	*dict = *dict2;
	*dict2 = temp;
	for (size_t i = 0u; i < dict2->size; i++) {
		for (entry_object* entry = dict2->entries[i], * next = NULL; entry != NULL; entry = next) {
			next = entry->next;
			mem_free(entry->key);
			if (entry->value_destroy == NULL) {
				mem_free((void*)entry->value);
			}
			mem_free(entry);
		}
	}

	mem_free(dict2->entries);
	mem_free(dict2);

	return true;
}

bool dict_set(
	dict_object* const dict,
	const void* const key, const size_t key_size,
	void* const value, const size_t value_size,
	bool (* const value_destroy)(void* const data),
	bool (* const value_copy)(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size)
) {
	if (dict == NULL || key == NULL || key_size == 0u || (value != NULL && value_size == 0u)) {
		return false;
	}

	entry_object* entry;
	if (value == NULL) {
		for (entry_object** prev = &dict->entries[hash(key, key_size) % dict->size]; prev != NULL; prev = &(*prev)->next) {
			if (key_size == (*prev)->key_size && memcmp((*prev)->key, key, key_size) == 0) {
				entry = *prev;
				*prev = entry->next;
				mem_free(entry->key);
				if (entry->value_destroy != NULL) {
					entry->value_destroy(entry->value);
				}
				else {
					mem_free(entry->value);
				}
				mem_free(entry);
				dict->count--;
				return true;
			}
		}
		return true;
	}
	else if (get(dict, key, key_size, false, &entry), entry != NULL) {
		if (entry->value_destroy != NULL) {
			entry->value_destroy(entry->value);
		}
		else {
			mem_free(entry->value);
		}

		if (value_destroy != NULL) {
			entry->value = value;
			entry->value_size = value_size;
			entry->value_destroy = value_destroy;
			entry->value_copy = value_copy;
			return true;
		}
		else {
			entry->value = mem_malloc(value_size);
			if (entry->value == NULL) {
				mem_free(entry->key);
				mem_free(entry);
				return false;
			}
			memcpy(entry->value, value, value_size);
			entry->value_size = value_size;
			entry->value_destroy = NULL;
			entry->value_copy = NULL;
			return true;
		}
	}
	else {
		entry = mem_malloc(sizeof(entry_object));
		if (entry == NULL) {
			return false;
		}

		entry->key = mem_malloc(key_size);
		if (entry->key == NULL) {
			mem_free(entry);
			return false;
		}
		entry->key_size = key_size;
		memcpy(entry->key, key, key_size);

		if (value_destroy != NULL) {
			entry->value = value;
			entry->value_size = value_size;
			entry->value_destroy = value_destroy;
			entry->value_copy = value_copy;
		}
		else {
			entry->value = mem_malloc(value_size);
			if (entry->value == NULL) {
				mem_free(entry->key);
				mem_free(entry);
				return false;
			}
			memcpy(entry->value, value, value_size);
			entry->value_size = value_size;
			entry->value_destroy = NULL;
			entry->value_copy = NULL;
		}

		const size_t i = hash(key, key_size) % dict->size;
		entry->next = dict->entries[i];
		dict->entries[i] = entry;
		dict->count++;

		if (dict->count >= dict->size) {
			return grow(dict);
		}
		else {
			return true;
		}
	}
}

bool dict_remove(dict_object* const dict, const void* const key, const size_t key_size, void** const value, size_t* const value_size) {
	if (dict == NULL || key == NULL || key_size == 0u || value == NULL) {
		return false;
	}

	for (entry_object** prev = &dict->entries[hash(key, key_size) % dict->size]; prev != NULL; prev = &(*prev)->next) {
		if (key_size == (*prev)->key_size && memcmp((*prev)->key, key, key_size) == 0) {
			entry_object* const entry = *prev;
			*prev = entry->next;

			*value = entry->value;
			if (value_size != NULL) {
				*value_size = entry->value_size;
			}

			mem_free(entry->key);
			mem_free(entry);
			dict->count--;

			return true;
		}
	}

	return false;
}

bool dict_replace(
	dict_object* const dict,
	const void* const key, const size_t key_size,
	void* const src_value, const size_t src_value_size,
	bool (* const src_destroy_value)(void* const data),
	bool (* const src_copy_value)(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size),
	void** const dst_value, size_t* const dst_value_size,
	bool (** const dst_destroy_value)(void* const data),
	bool (** const dst_copy_value)(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size)
) {
	if (
		dict == NULL ||
		key == NULL || key_size == 0u ||
		src_value == NULL || src_value_size == 0u
	) {
		return false;
	}

	for (entry_object** prev = &dict->entries[hash(key, key_size) % dict->size]; prev != NULL; prev = &(*prev)->next) {
		if (key_size == (*prev)->key_size && memcmp((*prev)->key, key, key_size) == 0) {
			entry_object* const entry = *prev;

			if (dst_value != NULL) {
				*dst_value = entry->value;
			}
			if (dst_value_size != NULL) {
				*dst_value_size = entry->value_size;
			}
			if (dst_destroy_value != NULL) {
				*dst_destroy_value = entry->value_destroy;
			}
			if (dst_copy_value != NULL) {
				*dst_copy_value = entry->value_copy;
			}

			entry->value = src_value;
			entry->value_size = src_value_size;
			entry->value_destroy = src_destroy_value;
			entry->value_copy = src_copy_value;

			return true;
		}
	}

	return false;
}

bool dict_only(dict_object* const dict, const size_t count, const void* const* const keys, const size_t* const key_sizes) {
	if (dict == NULL || (count > 0u && (keys == NULL || key_sizes == NULL))) {
		return false;
	}

	entry_object** only_entries = NULL;
	if (count > 0u) {
		// Save an array of what should be kept.
        only_entries = (entry_object**)mem_malloc(sizeof(entry_object*) * count);
		if (only_entries == NULL) {
			return false;
		}
		for (size_t i = 0u; i < count; i++) {
			get(dict, keys[i], key_sizes[i], true, &only_entries[i]);
		}
	}

	// Delete all entries that should not be kept.
	for (size_t i = 0u; dict->count > 0u; i++) {
		assert(i < dict->size);
		if (dict->entries[i] != NULL) {
			mem_free(dict->entries[i]->key);
			if (dict->entries[i]->value_destroy != NULL) {
				if (!dict->entries[i]->value_destroy(dict->entries[i]->value)) {
					mem_free(only_entries);
					return false;
				}
			}
			else {
				mem_free((void*)dict->entries[i]->value);
			}
			mem_free(dict->entries[i]);
			dict->entries[i] = NULL;
			dict->count--;
		}
	}

	if (count > 0u) {
		// Put back the entries that should be kept.
		for (size_t i = 0u; i < count; i++) {
			const size_t j = hash(keys[i], key_sizes[i]) % dict->size;
			if (only_entries[i] != NULL) {
				only_entries[i]->next = dict->entries[j];
			}
			dict->entries[j] = only_entries[i];
			dict->count++;

			if (dict->count >= dict->size) {
				if (!grow(dict)) {
					mem_free(only_entries);
					return false;
				}
			}
		}
		mem_free(only_entries);
	}

	return true;
}

size_t dict_tokey(void* const buf, const size_t buf_size, const size_t pairs_count, ...) {
	assert((buf == NULL || (buf != NULL && buf_size > 0u)) && pairs_count > 0u);

	va_list args;
	va_start(args, pairs_count);
	size_t total = 0u;
	if (buf == NULL) {
		for (size_t i = 0u; i < pairs_count; i++) {
			(void)va_arg(args, const void* const);
			const size_t size = va_arg(args, const size_t);
			total += size;
		}
	}
	else {
		for (size_t i = 0u; i < pairs_count; i++) {
			const void* const data = va_arg(args, const void* const);
			const size_t size = va_arg(args, const size_t);
			assert(total + size <= buf_size);
			memcpy((uint8_t*)buf + total, data, size);
			total += size;
		}
	}
	va_end(args);
	return total;
}

bool dict_map(dict_object* const dict, void* const data, bool (* const map)(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size)) {
	for (size_t i = 0u; i < dict->size; i++) {
		if (dict->entries[i] != NULL) {
			for (entry_object* entry = dict->entries[i]; entry != NULL; entry = entry->next) {
				if (!map(data, entry->key, entry->key_size, entry->value, entry->value_size)) {
					return false;
				}
			}
		}
	}
	return true;
}
