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
 * File and asset management library. This library is not thread-safe.
 */

#include "file/file_blob.h"
#include "file/file_surface.h"
#include "file/file_font.h"
#include "file/file_chunk.h"
#include "file/file_music.h"
#include "SDL_mixer.h"
#include <stddef.h>
#include <stdbool.h>

typedef enum file_type_enum {
	/*
	 * Generic data array; interpretation is done by your code.
	 */
	FILE_TYPE_BLOB,

	/*
	 * Pixel surfaces (images).
	 */
	FILE_TYPE_SURFACE,

	/*
	 * Bitmap fonts.
	 */
	FILE_TYPE_FONT,

	/*
	 * Audio chunks (sound effects).
	 */
	FILE_TYPE_CHUNK, 

	/*
	 * Music tracks.
	 */
	FILE_TYPE_MUSIC,

	/*
	 * The number of valid file types.
	 */
	FILE_TYPE_NUM
} file_type_enum;

typedef enum file_path_enum {
	/*
	 * The file will be loaded from the asset path.
	 */
	FILE_PATH_ASSET,

	/*
	 * The file will be loaded from the save path.
	 */
	FILE_PATH_SAVE,

	/*
	 * An attempt to load the file from the save path will be made first; if
	 * loading from the save path failed, another loading attempt will be made,
	 * from the asset path.
	 */
	FILE_PATH_SAVE_THEN_ASSET,

	/*
	 * The number of valid file path types.
	 */
	FILE_PATH_NUM
} file_path_enum;

typedef struct file_description_struct {
	file_type_enum type;
	file_path_enum path;

	/*
	 * Though some filesystems are case insensitive, filenames should always be
	 * handled as if they're case-sensitive, for case-sensitive filesystems.
	 * Always use forward slashes for subdirectories, never include
	 * backslashes, and stick to only UTF-8 encoded ASCII characters, further
	 * limiting to just letters, numbers, periods, and spaces, for
	 * guaranteed-portability. File extensions must be included, but the type
	 * field is what selects the file type.
	 */
	const char* filename;
} file_description_struct;

typedef enum file_status_enum {
	FILE_STATUS_SUCCESS,
	FILE_STATUS_ERROR,
	FILE_STATUS_MISSING
} file_status_enum;

typedef struct file_cache_struct file_cache_struct;

typedef struct file_struct {
	file_description_struct description;
	file_cache_struct* cache;

	union {
		void* data;
		file_blob_struct* blob;
		file_font_struct* font;
		SDL_Surface* surface;
		Mix_Chunk* chunk;
		Mix_Music* music;
	};
} file_struct;

/*
 * Create a file cache object. The asset_path and save_path arguments must be
 * valid paths; asset_path must be readable and save_path must be readable and
 * writable. Both arguments must be terminated in the current platform's path
 * separator ("/" or "\" on Windows, generally "/" on other platforms).
 */
file_cache_struct* file_cache_create(const char* const asset_path, const char* const save_path);

/*
 * Destroy a file cache object. Also frees all files still cached.
 */
void file_cache_destroy(file_cache_struct* const cache);

/*
 * Load a file. Always loads the requested file, and doesn't add the file to
 * the cache; the file can be added to the cache later with file_cache. The
 * loaded file must be manually unloaded with file_unload, or added to the
 * cache for automatic unloading with file_cache_add.
 */
const file_struct* file_load(file_cache_struct* const cache, const file_type_enum type, const file_path_enum path, const char* const filename, file_status_enum* const status);

/*
 * Unloads an uncached file. Returns true if unloading the file was successful,
 * otherwise false. Cached files can't be unloaded, they must be removed first
 * with file_cache_remove.
 */
bool file_unload(const file_struct* const file);

/*
 * Get a file from the cache. Loads the file if it's not already cached. Will
 * return a previously cached file if it's already cached, unless always_load
 * is true, in which case the file will be ungotten if already cached, then
 * cached in any case. If caching failed, NULL will be returned.
 */
const file_struct* file_cache_get(file_cache_struct* const cache, const file_type_enum type, const file_path_enum path, const char* const filename, file_status_enum* const status, const bool always_load);

/*
 * Frees a cached file. Returns true if there was no error freeing the file;
 * returns false otherwise. It isn't erroneous to attempt freeing a file that
 * isn't currently cached; true will still be returned for unget requests of
 * files not currently cached.
 */
bool file_cache_unget(file_cache_struct* const cache, const file_type_enum type, const file_path_enum path, const char* const filename);

/*
 * Save data to a file into the save path. The type argument only really
 * matters when add_to_cache is true. The data can be loaded later with a load
 * or get out of the save path; you maybe should have always_load of file_get
 * be true for repeatedly modified files, or even only use file_load, like for
 * configuration files. Returns true if saving succeeded, otherwise false.
 */
bool file_save(file_cache_struct* const cache, const file_type_enum type, const char* const filename, const void* const data, const size_t size, const bool add_to_cache);

/*
 * Adds the file to the cache originally used with a previous load of the file.
 * Returns true if adding was successful, otherwise false; if a file with the
 * same path was already cached, it's replaced by the passed in file.
 */
bool file_cache_add(const file_struct* const file);

/*
 * Removes a file that's currently cached from the cache the file was loaded
 * from. Returns true if removal was successful, otherwise false. Uncached
 * files must be manually unloaded with file_unload.
 */
bool file_cache_remove(const file_struct* const file);

/*
 * Ungets files not in the list, gets files not yet gotten in the list, and
 * keeps files that have already been gotten in the list. The intended use case
 * for this function is for optimizing memory usage between contexts in the
 * application, ungetting everything not used in the upcoming context,
 * pregetting everything the upcoming context requires, and keeping files
 * common between the contexts, to reduce the amount of processing.
 */
bool file_cache_only(file_cache_struct* const cache, const file_description_struct* const descriptions, const size_t count);
