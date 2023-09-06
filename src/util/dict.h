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

// TODO: Add a custom value_copy function for copying values alongside the current value_destroy function.

/*
 * The keys are interpreted as a basic array of data, and should not contain
 * any references to external data; it should be possible to directly do a
 * memcpy of the key in order to fully copy it. But, values can either be basic
 * data, just like keys, or a more complicated data structure that is copied by
 * reference and freed with a destructor function. When setting a dictionary
 * entry, provide a value destructor function when you want the value copied by
 * reference and destructed when the dictionary entry containing the value is
 * deleted.
 */

/*
 * Generic associative array ("dictionary") library. This library is not
 * thread-safe.
 */

#include <stddef.h>
#include <stdbool.h>

/*
 * The type used for dictionaries.
 */
typedef struct dict_object dict_object;

/*
 * Create a dictionary. The size argument sets an initial number of hash table
 * entries, and must be at least one; choosing a larger initial number of hash
 * table entries can improve performance, by choosing a number corresponding to
 * how many you expect to put into the dictionary.
 */
dict_object* dict_create(const size_t size);

/*
 * Destroy a dictionary. Always returns true if all the entries don't have a
 * value destructor; if there are one or more entries with a value destructor,
 * returns false if the value destructor of an entry failed, otherwise true.
 */
bool dict_destroy(dict_object* const dict);

/*
 * Create a copy of an entire dictionary.
 */
dict_object* dict_copy(dict_object* const dict);

/*
 * Get a value in the dictionary. If you don't want to get the size of the
 * value, you can pass NULL for the value_size parameter. Returns true if the
 * value was found, in which case the value and value_size parameters are set;
 * otherwise, returns false, and the value and value_size parameters remain
 * unchanged.
 */
bool dict_get(
	dict_object* const dict,
	const void* const key, const size_t key_size,
	void** const value, size_t* const value_size
);

/*
 * Set an entry in the dictionary. Set an entry to NULL to delete it; the
 * value_size and value_destroy arguments are ignored when the value argument
 * is NULL. Returns true if the set operation was successful, returns false
 * otherwise. If a request to delete an entry with a NULL value is made, and
 * the requested key doesn't exist, returns true and makes no change to the
 * dictionary.
 */
bool dict_set(
	dict_object* const dict,
	const void* const key, const size_t key_size,
	void* const value, const size_t value_size,
	bool (* const value_destroy)(void* const data),
	bool (* const value_copy)(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size)
);

/*
 * Remove a value from the dictionary and return that value, without destroying
 * it. Returns true if the value was found and returned in the value pointer
 * successfully; otherwise false.
 */
bool dict_remove(
	dict_object* const dict,
	const void* const key, const size_t key_size,
	void** const value, size_t* const value_size
);

/*
 * Replace the value of the key in-place, without changing the structure of the
 * dictionary. Returns true if the replacement was successful, otherwise false.
 * The previous value's relevant data is returned in the dst_* parameters; the
 * dst_* parameters can be NULL, like for a previous dict_get of the value,
 * mem_realloc of the value, and dict_replace of the value. If you retrieve the
 * previous data in the dst_* parameters, it's your responsibility to manage
 * said data.
 */
bool dict_replace(
	dict_object* const dict,
	const void* const key, const size_t key_size,
	void* const src_value, const size_t src_value_size,
	bool (* const src_value_destroy)(void* const data),
	bool (* const src_value_copy)(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size),
	void** const dst_value, size_t* const dst_value_size,
	bool (** const dst_value_destroy)(void* const data),
	bool (** const dst_value_copy)(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size)
);

/*
 * Deletes all entries in the dictionary not in the provided keys list. If
 * count is zero, it deletes all entries, while keeping the same internal table
 * size.
 */
bool dict_only(dict_object* const dict, const size_t count, const void* const* const keys, const size_t* const key_sizes);

/*
 * Calls the function for each entry in the dictionary, providing a functional
 * programming map operation.
 */
bool dict_map(dict_object* const dict, void* const data, bool (* const map)(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size));

/*
 * Helper function that assists with creating keys from a list of pairs_count pairs like:
 * const void* const value, const size_t size, ...
 * If the buf argument is NULL, this only calculates the size and returns the
 * size. If the buf argument isn't NULL, the key data is written to the buf
 * argument; after return, the buf is then ready to be used as a key. The
 * return value is the total size of the data written, less than or equal to
 * buf_size.
 */
size_t dict_tokey(void* const buf, const size_t buf_size, const size_t pairs_count, ...);
