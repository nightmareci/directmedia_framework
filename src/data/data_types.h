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

#include "data/data_raw.h"
#include "data/data_texture.h"
#include "data/data_font.h"
#include "data/data_sound.h"
#include "data/data_music.h"
#include "SDL_mixer.h"

typedef enum data_type {
	/*
	 * Generic raw data array; interpretation is done by your code.
	 */
	DATA_TYPE_RAW,

	/*
	 * Textures.
	 */
	DATA_TYPE_TEXTURE,

	/*
	 * Bitmap fonts.
	 */
	DATA_TYPE_FONT,

	/*
	 * Sounds.
	 */
	DATA_TYPE_SOUND,

	/*
	 * Music tracks.
	 */
	DATA_TYPE_MUSIC,

	/*
	 * The number of valid data types.
	 */
	DATA_TYPE_NUM
} data_type;

// TODO: Implement the option to select any data path search order.
typedef enum data_path {
	/*
	 * The data will be loaded from the resource path.
	 */
	DATA_PATH_RESOURCE,

	/*
	 * The data will be loaded from the save path.
	 */
	DATA_PATH_SAVE,

	/*
	 * TODO: The data was created at runtime, and is not stored in the save path. Data can be moved from the runtime path to the save path, to ensure it's in persistent storage.
	 */
	DATA_PATH_RUNTIME,

	/*
	 * An attempt to load the data from the save path will be made first; if
	 * loading from the save path failed, another loading attempt will be made from the resource path.
	 */
	DATA_PATH_SAVE_THEN_RESOURCE,

	/*
	 * The number of valid data path types.
	 */
	DATA_PATH_NUM
} data_path;

typedef struct data_id {
	data_type type;
	data_path path;

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
} data_id;

typedef enum data_load_status {
	DATA_LOAD_STATUS_SUCCESS,
	DATA_LOAD_STATUS_ERROR,
	DATA_LOAD_STATUS_MISSING
} data_load_status;

typedef struct data_cache_object data_cache_object;

typedef struct data_object {
	data_id id;
	data_cache_object* cache;

	union {
		data_raw_object* raw;
		data_font_object* font;
		data_texture_object* texture;
		Mix_Chunk* sound;
		Mix_Music* music;
	};
} data_object;
