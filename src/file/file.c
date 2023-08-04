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

#include "file/file.h"
#include "file/file_type_manager.h"
#include "util/string_util.h"
#include "util/dict.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_image.h"
#include <stdio.h>
#include <assert.h>

struct file_cache_struct {
	char* asset_path;
	char* save_path;
	dict_struct* files;
};

static const file_type_manager_struct* const type_managers[FILE_TYPE_NUM] = {
	&file_type_manager_blob,
	&file_type_manager_surface,
	&file_type_manager_font,
	&file_type_manager_chunk,
	&file_type_manager_music
};

file_cache_struct* file_cache_create(const char* const asset_path, const char* const save_path) {
	assert(asset_path != NULL && save_path != NULL);

	if (
#ifdef _WIN32
		(asset_path[strlen(asset_path) - 1] != '\\' && asset_path[strlen(asset_path) != '/']) ||
		(save_path[strlen(save_path) - 1] != '\\' && save_path[strlen(save_path) != '/'])
#else
		asset_path[strlen(asset_path) - 1] != '/' ||
		save_path[strlen(save_path) - 1] != '/'
#endif
	) {
		return NULL;
	}
    
    file_cache_struct* const cache = (file_cache_struct*)malloc(sizeof(file_cache_struct));
	if (cache == NULL) {
		return NULL;
	}

	if ((cache->asset_path = alloc_sprintf("%s", asset_path)) == NULL) {
		free(cache);
		return NULL;
	}

	if ((cache->save_path = alloc_sprintf("%s", save_path)) == NULL) {
		free(cache->asset_path);
		free(cache);
		return NULL;
	}

	cache->files = dict_create(1u);
	if (cache->files == NULL) {
		free(cache->save_path);
		free(cache->asset_path);
		free(cache);
		return NULL;
	}
	
	return cache;
}

void file_cache_destroy(file_cache_struct* const cache) {
	assert(cache != NULL);

	free(cache->asset_path);
	free(cache->save_path);
	dict_destroy(cache->files);
	free(cache);
}

static size_t get_key_size(const file_description_struct* const request) {
	return dict_tokey(NULL, 0u, 3u, &request->type, sizeof(request->type), &request->path, sizeof(request->path), request->filename, strlen(request->filename));
}

static void* get_key(const file_description_struct* const request, size_t* const key_size) {
	assert(request != NULL && strlen(request->filename) > 0u && key_size != NULL);

	const size_t size = get_key_size(request);
	void* const key = malloc(size);
	if (key == NULL) {
		return NULL;
	}
	dict_tokey(key, size, 3u, &request->type, sizeof(request->type), &request->path, sizeof(request->path), request->filename, strlen(request->filename));
	*key_size = size;
	return key;
}

const file_struct* file_load(file_cache_struct* const cache, const file_type_enum type, const file_path_enum path, const char* const filename, file_status_enum* const status) {
	assert(
		cache != NULL &&
		type >= 0 && type < FILE_TYPE_NUM &&
		path >= 0 && path < FILE_PATH_NUM &&
		filename != NULL
	);

	if (path == FILE_PATH_SAVE_THEN_ASSET) {
		const file_struct* const save_file = file_load(cache, type, FILE_PATH_SAVE, filename, status);
		if (save_file != NULL) {
			return save_file;
		}
		else if (*status != FILE_STATUS_MISSING) {
			return NULL;
		}
		return file_load(cache, type, FILE_PATH_ASSET, filename, status);
	}

	bool success;

	if (status != NULL) {
		*status = FILE_STATUS_SUCCESS;
	}

	file_struct* file = NULL;

	const char* const base_path =
		path == FILE_PATH_ASSET ? cache->asset_path :
		path == FILE_PATH_SAVE ? cache->save_path :
		NULL;
	assert(base_path != NULL);

	char* const file_path = alloc_sprintf("%s%s", base_path, filename);
	if (file_path == NULL) {
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}

	SDL_RWops* const file_rwops = SDL_RWFromFile(file_path, "rb");
	if (file_rwops == NULL) {
		free(file_path);
		if (status != NULL) {
			*status = FILE_STATUS_MISSING;
		}
		return NULL;
	}
	free(file_path);
    
    file = (file_struct*)malloc(sizeof(file_struct));
	if (file == NULL) {
		SDL_RWclose(file_rwops);
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}
	file->description.type = type;
	file->description.path = path;
	file->description.filename = (const char*)alloc_sprintf("%s", filename);
	if (file->description.filename == NULL) {
		free(file);
		SDL_RWclose(file_rwops);
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}
	file->cache = cache;

	if (!type_managers[type]->create((void*)file, file_rwops)) {
		free((char*)file->description.filename);
		free(file);
		SDL_RWclose(file_rwops);
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}

	if (SDL_RWclose(file_rwops) < 0) {
		type_managers[type]->destroy(file);
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}

	return file;
}

bool file_unload(const file_struct* const file) {
	assert(file != NULL);

	size_t key_size;
	void* const key = get_key(&file->description, &key_size);
	if (key == NULL) {
		return false;
	}

	if (dict_get(file->cache->files, key, key_size, NULL, NULL)) {
		return false;
	}

	return type_managers[file->description.type]->destroy((void*)file);
}

const file_struct* file_cache_get(file_cache_struct* const cache, const file_type_enum type, const file_path_enum path, const char* const filename, file_status_enum* const status, const bool always_load) {
	assert(
		cache != NULL &&
		type >= 0 && type < FILE_TYPE_NUM &&
		path >= 0 && path < FILE_PATH_NUM &&
		filename != NULL
	);

	bool success;

	if (status != NULL) {
		*status = FILE_STATUS_SUCCESS;
	}

	size_t key_size;
	file_description_struct request = {
		.type = type,
		.path = path,
		.filename = filename
	};

	void* const key = get_key(&request, &key_size);
	if (key == NULL) {
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}
	const file_struct* file = NULL;
	success = dict_get(cache->files, key, key_size, (void**)&file, NULL);
	if (success) {
		if (!always_load) {
			free(key);
			return file;
		}
		else {
			file_cache_unget(cache, type, path, filename);
		}
	}

	file = file_load(cache, type, path, filename, status);
	if (file == NULL) {
		free(key);
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}

	success = dict_set(cache->files, key, key_size, (void*)file, sizeof(file), type_managers[type]->destroy, NULL);
	if (!success) {
		type_managers[type]->destroy((void*)file);
		free(key);
		if (status != NULL) {
			*status = FILE_STATUS_ERROR;
		}
		return NULL;
	}

	free(key);
	return file;
}

bool file_cache_unget(file_cache_struct* const cache, const file_type_enum type, const file_path_enum path, const char* const filename) {
	assert(
		cache != NULL &&
		type >= 0 && type < FILE_TYPE_NUM &&
		path >= 0 && path < FILE_PATH_NUM &&
		filename != NULL
	);

	size_t key_size = 0u;
	file_description_struct request = {
		.type = type,
		.path = path,
		.filename = filename
	};

	void* const key = get_key(&request, &key_size);
	if (key == NULL) {
		return false;
	}

	if (!dict_set(cache->files, key, key_size, NULL, 0u, NULL, NULL)) {
		return false;
	}

	free(key);
	return true;
}

bool file_save(file_cache_struct* const cache, const file_type_enum type, const char* const filename, const void* const data, const size_t size, const bool add_to_cache) {
	assert(
		cache != NULL &&
		type >= 0 && type < FILE_TYPE_NUM &&
		filename != NULL &&
		data != NULL &&
		size > 0u
	);

	char* const file_path = alloc_sprintf("%s%s", cache->save_path, filename);
	if (file_path == NULL) {
		return false;
	}
	SDL_RWops* const file_rwops = SDL_RWFromFile(file_path, "wb");

	if (file_rwops == NULL) {
		free(file_path);
		return false;
	}

	if (SDL_RWwrite(file_rwops, data, 1u, size) < size) {
		SDL_RWclose(file_rwops);
		free(file_path);
		return false;
	}

	SDL_RWclose(file_rwops);
	free(file_path);

	if (add_to_cache) {
        file_struct* const file = (file_struct*)malloc(sizeof(file_struct));
		if (file == NULL) {
			return false;
		}
		file->description.type = type;
		file->description.path = FILE_PATH_SAVE;
		file->description.filename = (const char*)alloc_sprintf("%s", filename);
		if (file->description.filename == NULL) {
			free(file);
			return false;
		}
		file->cache = cache;

		size_t key_size;
		void* const key = get_key(&file->description, &key_size);
		if (key == NULL) {
			type_managers[type]->destroy(file);
			return false;
		}

		if (!file_cache_unget(cache, type, FILE_PATH_SAVE, filename)) {
			free(key);
			type_managers[type]->destroy(file);
			return false;
		}

		SDL_RWops* const temp_rwops = SDL_RWFromConstMem(data, size);
		if (temp_rwops == NULL) {
			free((char*)file->description.filename);
			free(file);
			return false;
		}

		if (!type_managers[type]->create((void*)file, temp_rwops)) {
			free((char*)file->description.filename);
			free(file);
			SDL_RWclose(temp_rwops);
			return false;
		}

		if (SDL_RWclose(temp_rwops) < 0) {
			type_managers[type]->destroy(file);
			return false;
		}

		const bool success = dict_set(cache->files, key, key_size, (void*)file, sizeof(file), type_managers[type]->destroy, NULL);
		if (!success) {
			free(key);
			type_managers[type]->destroy((void*)file);
			return false;
		}

		free(key);
	}

	return true;
}

bool file_cache_add(const file_struct* const file) {
	size_t key_size;
	void* const key = get_key(&file->description, &key_size);
	if (key == NULL) {
		return false;
	}

	if (!file_cache_unget(file->cache, file->description.type, file->description.path, file->description.filename)) {
		free(key);
		return false;
	}

	const bool success = dict_set(file->cache->files, key, key_size, (void*)file, sizeof(file), type_managers[file->description.type]->destroy, NULL);
	if (!success) {
		free(key);
		return false;
	}

	free(key);
	return true;
}

bool file_cache_remove(const file_struct* const file) {
	assert(file != NULL);

	size_t key_size;
	void* const key = get_key(&file->description, &key_size);
	if (key == NULL) {
		return false;
	}

	void* value = NULL;
	if (!dict_remove(file->cache->files, key, key_size, &value, NULL) || file != value) {
		free(key);
		return false;
	}

	free(key);
	return true;
}

bool file_cache_only(file_cache_struct* const cache, const file_description_struct* const descriptions, const size_t count) {
	assert(
		cache != NULL &&
		(count == 0u || (descriptions != NULL && count > 0u))
	);

	bool success;

	if (count == 0u) {
		const bool status = dict_only(cache->files, 0u, NULL, NULL);
		return status;
	}
    
    void** const keys = (void**)malloc(count * sizeof(void*));
	if (keys == NULL) {
		return false;
	}
    size_t* const key_sizes = (size_t*)malloc(count * sizeof(size_t));
	if (key_sizes == NULL) {
		free(keys);
		return false;
	}
	for (size_t i = 0u; i < count; i++) {
		keys[i] = get_key(&descriptions[i], &key_sizes[i]);
	}

	success = dict_only(cache->files, count, (const void* const*)keys, key_sizes);
	if (!success) {
		for (size_t i = 0u; i < count; i++) {
			free(keys[i]);
		}
		free(key_sizes);
		free(keys);
		return false;
	}

	for (size_t i = 0u; i < count; i++) {
		file_status_enum status;
		(void)file_cache_get(cache, descriptions[i].type, descriptions[i].path, descriptions[i].filename, &status, false);
		if (status != FILE_STATUS_SUCCESS) {
			for (size_t i = 0u; i < count; i++) {
				free(keys[i]);
			}
			free(key_sizes);
			free(keys);
			return false;
		}
	}

	for (size_t i = 0u; i < count; i++) {
		free(keys[i]);
	}
	free(key_sizes);
	free(keys);
	return true;
}
