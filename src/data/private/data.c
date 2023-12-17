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

#include "data/data.h"
#include "data/private/data_type_manager.h"
#include "util/dict.h"
#include "util/str.h"
#include "util/mem.h"
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_image.h"
#include <stdio.h>
#include <assert.h>

struct data_cache_object {
	char* resource_path;
	char* save_path;
	dict_object* data;
};

static const data_type_manager* const type_managers[DATA_TYPE_NUM] = {
	&data_type_manager_raw,
	&data_type_manager_texture,
	&data_type_manager_font,
	&data_type_manager_sound,
	&data_type_manager_music
};

data_cache_object* data_cache_create(const char* const resource_path, const char* const save_path) {
	assert(resource_path != NULL);
	assert(save_path != NULL);

	if (
#ifdef _WIN32
		(resource_path[strlen(resource_path) - 1] != '\\' && resource_path[strlen(resource_path) != '/']) ||
		(save_path[strlen(save_path) - 1] != '\\' && save_path[strlen(save_path) != '/'])
#else
		resource_path[strlen(resource_path) - 1] != '/' ||
		save_path[strlen(save_path) - 1] != '/'
#endif
	) {
		return NULL;
	}
    
    data_cache_object* const cache = (data_cache_object*)mem_malloc(sizeof(data_cache_object));
	if (cache == NULL) {
		return NULL;
	}

	if ((cache->resource_path = alloc_sprintf("%s", resource_path)) == NULL) {
		mem_free(cache);
		return NULL;
	}

	if ((cache->save_path = alloc_sprintf("%s", save_path)) == NULL) {
		mem_free(cache->resource_path);
		mem_free(cache);
		return NULL;
	}

	cache->data = dict_create(1u);
	if (cache->data == NULL) {
		mem_free(cache->save_path);
		mem_free(cache->resource_path);
		mem_free(cache);
		return NULL;
	}
	
	return cache;
}

void data_cache_destroy(data_cache_object* const cache) {
	assert(cache != NULL);

	mem_free(cache->resource_path);
	mem_free(cache->save_path);
	dict_destroy(cache->data);
	mem_free(cache);
}

static size_t get_key_size(const data_id* const id) {
	return dict_tokey(NULL, 0u, 3u, &id->type, sizeof(id->type), &id->path, sizeof(id->path), id->filename, strlen(id->filename));
}

static void* get_key(const data_id* const id, size_t* const key_size) {
	assert(id != NULL);
	assert(strlen(id->filename) > 0u);
	assert(key_size != NULL);

	const size_t size = get_key_size(id);
	void* const key = mem_malloc(size);
	if (key == NULL) {
		return NULL;
	}
	dict_tokey(key, size, 3u, &id->type, sizeof(id->type), &id->path, sizeof(id->path), id->filename, strlen(id->filename));
	*key_size = size;
	return key;
}

const data_object* data_load(data_cache_object* const cache, const data_type type, const data_path path, const char* const filename, data_load_status* const status) {
	assert(cache != NULL);
	assert(type >= 0);
	assert(type < DATA_TYPE_NUM);
	assert(path >= 0);
	assert(path < DATA_PATH_NUM);
	assert(filename != NULL);

	if (path == DATA_PATH_SAVE_THEN_RESOURCE) {
		const data_object* const data = data_load(cache, type, DATA_PATH_SAVE, filename, status);
		if (data != NULL) {
			return data;
		}
		else if (*status != DATA_LOAD_STATUS_MISSING) {
			return NULL;
		}
		return data_load(cache, type, DATA_PATH_RESOURCE, filename, status);
	}

	if (status != NULL) {
		*status = DATA_LOAD_STATUS_SUCCESS;
	}

	data_object* data = NULL;

	const char* const base_path =
		path == DATA_PATH_RESOURCE ? cache->resource_path :
		path == DATA_PATH_SAVE ? cache->save_path :
		NULL;
	assert(base_path != NULL);

	char* const full_filename = alloc_sprintf("%s%s", base_path, filename);
	if (full_filename == NULL) {
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_ERROR;
		}
		return NULL;
	}

	SDL_RWops* const rwops = SDL_RWFromFile(full_filename, "rb");
	if (rwops == NULL) {
		mem_free(full_filename);
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_MISSING;
		}
		return NULL;
	}
	mem_free(full_filename);
    
    data = (data_object*)mem_malloc(sizeof(data_object));
	if (data == NULL) {
		SDL_RWclose(rwops);
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_ERROR;
		}
		return NULL;
	}
	data->id.type = type;
	data->id.path = path;
	data->id.filename = (const char*)alloc_sprintf("%s", filename);
	if (data->id.filename == NULL) {
		mem_free(data);
		SDL_RWclose(rwops);
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_ERROR;
		}
		return NULL;
	}
	data->cache = cache;

	if (!type_managers[type]->create((void*)data, rwops)) {
		mem_free((char*)data->id.filename);
		mem_free(data);
		SDL_RWclose(rwops);
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_ERROR;
		}
		return NULL;
	}

	return data;
}

bool data_unload(const data_object* const data) {
	assert(data != NULL);

	size_t key_size;
	void* const key = get_key(&data->id, &key_size);
	if (key == NULL) {
		return false;
	}

	if (dict_get(data->cache->data, key, key_size, NULL, NULL)) {
		mem_free(key);
		return false;
	}
	mem_free(key);

	return type_managers[data->id.type]->destroy((void*)data);
}

const data_object* data_cache_get(data_cache_object* const cache, const data_type type, const data_path path, const char* const filename, data_load_status* const status, const bool always_load) {
	assert(cache != NULL);
	assert(type >= 0);
	assert(type < DATA_TYPE_NUM);
	assert(path >= 0);
	assert(path < DATA_PATH_NUM);
	assert(filename != NULL);

	bool success;

	if (status != NULL) {
		*status = DATA_LOAD_STATUS_SUCCESS;
	}

	size_t key_size;
	data_id id = {
		.type = type,
		.path = path,
		.filename = filename
	};

	void* const key = get_key(&id, &key_size);
	if (key == NULL) {
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_ERROR;
		}
		return NULL;
	}
	const data_object* data = NULL;
	success = dict_get(cache->data, key, key_size, (void**)&data, NULL);
	if (success) {
		if (!always_load) {
			mem_free(key);
			return data;
		}
		else {
			data_cache_unget(cache, type, path, filename);
		}
	}

	data = data_load(cache, type, path, filename, status);
	if (data == NULL) {
		mem_free(key);
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_ERROR;
		}
		return NULL;
	}

	success = dict_set(cache->data, key, key_size, (void*)data, sizeof(data), type_managers[type]->destroy_by_dict, NULL);
	if (!success) {
		type_managers[type]->destroy((void*)data);
		mem_free(key);
		if (status != NULL) {
			*status = DATA_LOAD_STATUS_ERROR;
		}
		return NULL;
	}

	mem_free(key);
	return data;
}

bool data_cache_unget(data_cache_object* const cache, const data_type type, const data_path path, const char* const filename) {
	assert(cache != NULL);
	assert(type >= 0);
	assert(type < DATA_TYPE_NUM);
	assert(path >= 0);
	assert(path < DATA_PATH_NUM);
	assert(filename != NULL);

	size_t key_size = 0u;
	data_id id = {
		.type = type,
		.path = path,
		.filename = filename
	};

	void* const key = get_key(&id, &key_size);
	if (key == NULL) {
		return false;
	}

	if (!dict_set(cache->data, key, key_size, NULL, 0u, NULL, NULL)) {
		return false;
	}

	mem_free(key);
	return true;
}

char* data_directory_get(const data_object* const data) {
	const char* const data_filename = data->id.filename;
	char* data_directory;
	bool has_directory = false;
	size_t end = 0u;
	const size_t len = strlen(data_filename);
	for (size_t i = 0u; i < len; i++) {
		if (data_filename[i] == '/') {
			end = i;
			has_directory = true;
		}
	}
	if (has_directory) {
		data_directory = mem_malloc(end + 1u + 1u);
		if (data_directory == NULL) {
			return NULL;
		}
		else {
			memcpy(data_directory, data_filename, end + 1u);
			data_directory[end + 1u] = '\0';
			return data_directory;
		}
	}
	else {
		data_directory = mem_malloc(1);
		if (data_directory == NULL) {
			return NULL;
		}
		else {
			data_directory[0] = '\0';
			return data_directory;
		}
	}
}

bool data_save(data_cache_object*const cache, const data_type type, const char*const filename, const void*const bytes, const size_t size, const bool add_to_cache)
{
	assert(cache != NULL);
	assert(type >= 0);
	assert(type < DATA_TYPE_NUM);
	assert(filename != NULL);
	assert(bytes != NULL);
	assert(size > 0u);

	char* const full_filename = alloc_sprintf("%s%s", cache->save_path, filename);
	if (full_filename == NULL) {
		return false;
	}
	SDL_RWops* const rwops = SDL_RWFromFile(full_filename, "wb");

	if (rwops == NULL) {
		mem_free(full_filename);
		return false;
	}

	if (SDL_RWwrite(rwops, bytes, 1u, size) < size) {
		SDL_RWclose(rwops);
		mem_free(full_filename);
		return false;
	}

	SDL_RWclose(rwops);
	mem_free(full_filename);

	if (add_to_cache) {
        data_object* const data = (data_object*)mem_malloc(sizeof(data_object));
		if (data == NULL) {
			return false;
		}
		data->id.type = type;
		data->id.path = DATA_PATH_SAVE;
		data->id.filename = (const char*)alloc_sprintf("%s", filename);
		if (data->id.filename == NULL) {
			mem_free(data);
			return false;
		}
		data->cache = cache;

		size_t key_size;
		void* const key = get_key(&data->id, &key_size);
		if (key == NULL) {
			type_managers[type]->destroy(data);
			return false;
		}

		if (!data_cache_unget(cache, type, DATA_PATH_SAVE, filename)) {
			mem_free(key);
			type_managers[type]->destroy(data);
			return false;
		}

		SDL_RWops* const temp_rwops = SDL_RWFromConstMem(bytes, size);
		if (temp_rwops == NULL) {
			mem_free(key);
			mem_free((char*)data->id.filename);
			mem_free(data);
			return false;
		}

		if (!type_managers[type]->create((void*)data, temp_rwops)) {
			mem_free(key);
			mem_free((char*)data->id.filename);
			mem_free(data);
			SDL_RWclose(temp_rwops);
			return false;
		}

		if (SDL_RWclose(temp_rwops) < 0) {
			mem_free(key);
			type_managers[type]->destroy(data);
			return false;
		}

		const bool success = dict_set(cache->data, key, key_size, (void*)data, sizeof(data), type_managers[type]->destroy_by_dict, NULL);
		if (!success) {
			mem_free(key);
			type_managers[type]->destroy((void*)data);
			return false;
		}

		mem_free(key);
	}

	return true;
}

bool data_recreate(data_cache_object* const cache, const char* const filename) {
	assert(cache != NULL);
	assert(filename != NULL);

	char* const full_filename = alloc_sprintf("%s%s", cache->save_path, filename);
	if (full_filename == NULL) {
		return false;
	}
	SDL_RWops* const rwops = SDL_RWFromFile(full_filename, "wb");

	if (rwops == NULL) {
		mem_free(full_filename);
		return false;
	}

	SDL_RWclose(rwops);
	mem_free(full_filename);

	return true;
}

bool data_append(data_cache_object* const cache, const char* const filename, const void* const bytes, const size_t size) {
	assert(cache != NULL);
	assert(filename != NULL);
	assert(bytes != NULL);
	assert(size > 0u);

	char* const full_filename = alloc_sprintf("%s%s", cache->save_path, filename);
	if (full_filename == NULL) {
		return false;
	}
	SDL_RWops* const rwops = SDL_RWFromFile(full_filename, "ab");

	if (rwops == NULL) {
		mem_free(full_filename);
		return false;
	}

	if (SDL_RWwrite(rwops, bytes, 1u, size) < size) {
		SDL_RWclose(rwops);
		mem_free(full_filename);
		return false;
	}

	SDL_RWclose(rwops);
	mem_free(full_filename);

	return true;
}

bool data_cache_add(const data_object* const data) {
	size_t key_size;
	void* const key = get_key(&data->id, &key_size);
	if (key == NULL) {
		return false;
	}

	if (!data_cache_unget(data->cache, data->id.type, data->id.path, data->id.filename)) {
		mem_free(key);
		return false;
	}

	const bool success = dict_set(data->cache->data, key, key_size, (void*)data, sizeof(data), type_managers[data->id.type]->destroy_by_dict, NULL);
	if (!success) {
		mem_free(key);
		return false;
	}

	mem_free(key);
	return true;
}

bool data_cache_remove(const data_object* const data) {
	assert(data != NULL);

	size_t key_size;
	void* const key = get_key(&data->id, &key_size);
	if (key == NULL) {
		return false;
	}

	void* value = NULL;
	if (!dict_remove(data->cache->data, key, key_size, &value, NULL) || data != value) {
		mem_free(key);
		return false;
	}

	mem_free(key);
	return true;
}

bool data_cache_only(data_cache_object* const cache, const data_id* const ids, const size_t count) {
	assert(cache != NULL);
	assert(count == 0u || (ids != NULL && count > 0u));

	bool success;

	if (count == 0u) {
		const bool status = dict_only(cache->data, 0u, NULL, NULL);
		return status;
	}
    
    void** const keys = (void**)mem_malloc(count * sizeof(void*));
	if (keys == NULL) {
		return false;
	}
    size_t* const key_sizes = (size_t*)mem_malloc(count * sizeof(size_t));
	if (key_sizes == NULL) {
		mem_free(keys);
		return false;
	}
	for (size_t i = 0u; i < count; i++) {
		keys[i] = get_key(&ids[i], &key_sizes[i]);
	}

	success = dict_only(cache->data, count, (const void* const*)keys, key_sizes);
	if (!success) {
		for (size_t i = 0u; i < count; i++) {
			mem_free(keys[i]);
		}
		mem_free(key_sizes);
		mem_free(keys);
		return false;
	}

	for (size_t i = 0u; i < count; i++) {
		data_load_status status;
		(void)data_cache_get(cache, ids[i].type, ids[i].path, ids[i].filename, &status, false);
		if (status != DATA_LOAD_STATUS_SUCCESS) {
			for (size_t i = 0u; i < count; i++) {
				mem_free(keys[i]);
			}
			mem_free(key_sizes);
			mem_free(keys);
			return false;
		}
	}

	for (size_t i = 0u; i < count; i++) {
		mem_free(keys[i]);
	}
	mem_free(key_sizes);
	mem_free(keys);
	return true;
}
