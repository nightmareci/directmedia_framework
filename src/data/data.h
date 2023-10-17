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

/*
 * File and data management library. This library is not thread-safe.
 */

#include "data/data_types.h"
#include <stddef.h>
#include <stdbool.h>

/*
 * TODO: Implement a "set properties" option for data objects. Textures should
 * have the option to set their sampling coordinate dimensions, which would make
 * them work with the same hard-coded coordinates regardless of the source
 * file's original resolution.
 */

/*
 * Create a data cache object. The resource_path and save_path arguments must be
 * valid paths; resource_path must be readable and save_path must be readable and
 * writable. Both arguments must be terminated in the current platform's path
 * separator ("/" or "\" on Windows, generally "/" on other platforms).
 */
data_cache_object* data_cache_create(const char* const resource_path, const char* const save_path);

/*
 * Destroy a data cache object.
 */
void data_cache_destroy(data_cache_object* const cache);

/*
 * Load data. Always loads the requested data, and doesn't add the data to
 * the cache; the data can be added to the cache later with data_cache_add. The
 * loaded data must be manually unloaded with data_unload, or added to the
 * cache for automatic unloading with data_cache_add.
 */
const data_object* data_load(data_cache_object* const cache, const data_type type, const data_path path, const char* const filename, data_load_status* const status);

/*
 * Unloads uncached data. Returns true if unloading the data was successful,
 * otherwise false. Cached data can't be unloaded, it must be removed first
 * with data_cache_remove.
 */
bool data_unload(const data_object* const data);

/*
 * Get data from the cache. Loads the data if it's not already cached. Will
 * return previously cached data if it's already cached, unless always_load
 * is true, in which case the data will be ungotten if already cached, then
 * cached in any case. If caching failed, NULL will be returned.
 */
const data_object* data_cache_get(data_cache_object* const cache, const data_type type, const data_path path, const char* const filename, data_load_status* const status, const bool always_load);

/*
 * Frees cached data. Returns true if there was no error freeing the data;
 * returns false otherwise. It isn't erroneous to attempt freeing data that
 * isn't currently cached; true will still be returned for unget requests of
 * data not currently cached.
 */
bool data_cache_unget(data_cache_object* const cache, const data_type type, const data_path path, const char* const filename);

/*
 * Returns the directory of the previously-loaded data object, or NULL in the
 * case of fatal errors. The returned string is suitable to be directly
 * prepended to a base filename; if that directory-prepended full path is used
 * to load another file, it's guaranteed that file will be loaded from the same
 * directory as that of the data object passed into this function. The caller
 * must free the returned string.
 */
char* data_directory_get(const data_object* const data);

/*
 * Save raw data to a file into the save path. The type argument only really
 * matters when add_to_cache is true. The data can be loaded later with a load
 * or get out of the save path; you maybe should have always_load of
 * data_cache_get be true for repeatedly modified files, or even only use
 * data_load, like for configuration files. Returns true if saving succeeded,
 * otherwise false.
 */
bool data_save(data_cache_object* const cache, const data_type type, const char* const filename, const void* const bytes, const size_t size, const bool add_to_cache);

/*
 * Adds the data to the cache originally used with a previous load of the data.
 * Returns true if adding was successful, otherwise false; if data with the
 * same path was already cached, it's replaced by the passed in data.
 */
bool data_cache_add(const data_object* const data);

/*
 * Removes data that's currently cached from the cache the data was loaded
 * from. Returns true if removal was successful, otherwise false. Uncached
 * data must be manually unloaded with data_unload.
 */
bool data_cache_remove(const data_object* const data);

/*
 * Ungets data not in the list, gets data not yet gotten in the list, and
 * keeps data that has already been gotten in the list. The intended use case
 * for this function is for optimizing memory usage between contexts in the
 * application, ungetting everything not used in the upcoming context,
 * pregetting everything the upcoming context requires, and keeping data
 * common between the contexts to reduce the amount of processing.
 */
bool data_cache_only(data_cache_object* const cache, const data_id* const ids, const size_t count);
