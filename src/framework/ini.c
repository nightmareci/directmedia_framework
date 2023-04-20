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

#include "framework/ini.h"
#include "framework/dict.h"
#include "framework/string_util.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <assert.h>

static void next_line(const char** line, size_t* const len, const char* const end) {
	assert(
		line != NULL && *line != NULL &&
		len != NULL &&
		end != NULL
	);

	while (*line != end && (**line == ' ' || **line == '\t' || **line == '\n' || **line == '\r')) {
		(*line)++;
	}

	if (*line == end) {
		*len = 0u;
		return;
	}

	const char* line_end = *line;
	while (line_end != end && *line_end != '\n' && *line_end != '\r') {
		line_end++;
	}
	if (line_end != end) {
		while (*line_end == ' ' || *line_end == '\t') {
			line_end--;
		}
	}
	*len = line_end - *line;
}

static bool destroy_section(void* const data) {
    dict_struct* const section = (dict_struct*)data;
	return dict_destroy(section);
}

static bool copy_section(void* const src_value, const size_t src_size, void** const dst_value, size_t* const dst_size) {
	*dst_value = dict_copy((dict_struct*)src_value);
	return *dst_value != NULL;
}

static void toupper_str(char* const str, const size_t size) {
	for (size_t i = 0u; i < size; i++) {
		str[i] = toupper(str[i]);
	}
}

ini_struct* ini_create(const char* const data, const size_t size) {
	dict_struct* const ini_sections = dict_create(1u);
	if (ini_sections == NULL) {
		return NULL;
	}
	if (data == NULL || size == 0u) {
		return (ini_struct*)ini_sections;
	}

	char* section_str = NULL;
	size_t section_len = 0u;

	const char* line = data;
	size_t len;
	const char* end = data + size;

	next_line(&line, &len, end);
	while (line < end) {
		if (line[0] == '[' && line[len - 1] == ']' && len >= 3u) {
			if (section_str != NULL) {
				free(section_str);
			}

			if (len == 3u && (line[1] == ' ' || line[1] == '\t')) {
				fprintf(stderr, "Invalid empty section\n");
				fflush(stderr);
				return NULL;
			}

			size_t start = 1u;
			while (line[start] == ' ' || line[start] == '\t') {
				start++;
			}
			size_t end = len - 2u;
			while (line[end] == ' ' || line[end] == '\t') {
				end--;
			}
			if (start > end) {
				fprintf(stderr, "Invalid empty section\n");
				fflush(stderr);
				return NULL;
			}

			section_len = end - start + 1u;
			section_str = (char*)malloc(len);
			memcpy(section_str, line + start, section_len);
			toupper_str(section_str, section_len);

			dict_struct* const section = dict_create(1u);
			if (section == NULL) {
				free(section_str);
				return NULL;
			}

			if (!dict_set(ini_sections, section_str, section_len, section, sizeof(section), destroy_section, copy_section)) {
				free(section_str);
				return NULL;
			}
		}
		else if (line[0] != '=') {
			if (section_str == NULL) {
				fprintf(stderr, "Key-value line present without a section preceding it\n");
				fflush(stderr);
				return NULL;
			}

			size_t end_key;
			size_t start_value;

			end_key = 0u;
			bool found_name = false;
			while (end_key < len && line[end_key] != '=') {
				if (line[end_key] != ' ' && line[end_key] != '\t') {
					found_name = true;
				}
				end_key++;
			}
			if (!found_name || end_key == len || end_key == len - 1u) {
				fprintf(stderr, "Invalid key-value line\n");
				fflush(stderr);
				return NULL;
			}
			start_value = end_key + 1u;

			do {
				end_key--;
			} while (line[end_key] == ' ' || line[end_key] == '\t');

			char* const key_str = (char*)malloc(end_key + 1u);
			memcpy(key_str, line, end_key + 1u);
			toupper_str(key_str, end_key + 1u);

			while (start_value < len && (line[start_value] == ' ' || line[start_value] == '\t')) {
				start_value++;
			}
			if (start_value == len) {
				fprintf(stderr, "Invalid key-value line\n");
				fflush(stderr);
				free(key_str);
				return NULL;
			}

			size_t end_value = len;
			if (line[start_value] == '"') {
				if (line[start_value + 1u] == '"') {
					fprintf(stderr, "Invalid empty string for key's value\n");
					fflush(stderr);
					free(key_str);
					return NULL;
				}

				start_value++;
				do {
					end_value--;
				} while (end_value >= start_value && (line[end_value] == ' ' || line[end_value] == '\t'));
				if (line[end_value] != '"') {
					fprintf(stderr, "Invalid quoted value for key; no ending quotation mark\n");
					fflush(stderr);
					free(key_str);
					return NULL;
				}

				dict_struct* section = NULL;
				if (!dict_get(ini_sections, section_str, section_len, (void**)&section, NULL)) {
					free(key_str);
					return NULL;
				}

				char* const value_str = (char*)malloc(end_value - start_value + 1u);
				if (value_str == NULL) {
					free(key_str);
					return NULL;
				}
				memcpy(value_str, line + start_value, end_value - start_value);
				value_str[end_value - start_value] = '\0';

				if (!dict_set(section, key_str, end_key + 1u, value_str, end_value - start_value + 1u, NULL, NULL)) {
					free(value_str);
					free(key_str);
					return NULL;
				}
				free(value_str);
			}
			else {
				do {
					end_value--;
				} while (end_value >= start_value && (line[end_value] == ' ' || line[end_value] == '\t'));

				dict_struct* section = NULL;
				if (!dict_get(ini_sections, section_str, section_len, (void**)&section, NULL)) {
					free(key_str);
					return NULL;
				}

				char* const value_str = (char*)malloc(end_value - start_value + 2u);
				if (value_str == NULL) {
					free(key_str);
					return NULL;
				}
				memcpy(value_str, line + start_value, end_value - start_value + 1u);
				value_str[end_value - start_value + 1u] = '\0';

				if (!dict_set(section, key_str, end_key + 1u, value_str, end_value - start_value + 2u, NULL, NULL)) {
					free(value_str);
					free(key_str);
					return NULL;
				}
				free(value_str);
			}
			free(key_str);
		}
		else {
			fprintf(stderr, "Invalid key-value line starting with =\n");
			fflush(stderr);
			return NULL;
		}

		line += len;
		next_line(&line, &len, end);
	}
	if (section_str != NULL) {
		free(section_str);
	}

	return (ini_struct*)ini_sections;
}


void ini_destroy(ini_struct* const ini) {
	dict_destroy((dict_struct*)ini);
}

static bool map_keys_copy(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	return dict_set((dict_struct*)data, key, key_size, value, value_size, NULL, NULL);
}

static bool map_sections_copy(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	dict_struct* const sections_copy = (dict_struct*)data;
	dict_struct* const section_copy = dict_create(1u);

	if (!dict_set(sections_copy, key, key_size, section_copy, sizeof(section_copy), destroy_section, copy_section)) {
		dict_destroy(section_copy);
		return false;
	}

	return dict_map(value, section_copy, map_keys_copy);
}

ini_struct* ini_copy(ini_struct* const ini) {
	ini_struct* const copy = ini_create(NULL, 0u);

	if (!dict_map((dict_struct*)ini, copy, map_sections_copy)) {
		ini_destroy(copy);
		return NULL;
	}
	else {
		return copy;
	}
}

static bool map_keys_merge(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	return dict_set((dict_struct*)data, key, key_size, value, value_size, NULL, NULL);
}

static bool map_sections_merge(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	dict_struct* const dst_ini = (dict_struct*)data;
	dict_struct* dst_section = NULL;

	if (!dict_get(dst_ini, key, key_size, (void**)&dst_section, NULL)) {
		if (
			(dst_section = dict_create(1u)) == NULL ||
			!dict_set(dst_ini, key, key_size, dst_section, sizeof(dst_section), destroy_section, copy_section)
		) {
			return false;
		}
	}

	return dict_map(value, dst_section, map_keys_merge);
}

bool ini_merge(ini_struct* const dst, ini_struct* const src) {
	return dict_map((dict_struct*)src, dst, map_sections_merge);
}

const char* ini_get(ini_struct* const ini, const char* const section, const char* const key) {
	char* const section_upper = (char*)malloc(strlen(section));
	if (section_upper == NULL) {
		abort();
	}
	memcpy(section_upper, section, strlen(section));
	toupper_str(section_upper, strlen(section));

	char* const key_upper = (char*)malloc(strlen(key));
	if (key_upper == NULL) {
		free(section_upper);
		abort();
	}
	memcpy(key_upper, key, strlen(key));
	toupper_str(key_upper, strlen(key));

	dict_struct* section_dict = NULL;
	char* value_str = NULL;

	if (dict_get((dict_struct*)ini, section_upper, strlen(section), (void**)&section_dict, NULL)) {
		dict_get(section_dict, key_upper, strlen(key), (void**)&value_str, NULL);
	}

	free(key_upper);
	free(section_upper);
	return value_str;
}

int ini_getf(ini_struct* const ini, const char* const section, const char* const key, const char* const format, ...) {
	const char* const value = ini_get(ini, section, key);
	if (value == NULL) {
		return EOF;
	}

	va_list args;
	va_start(args, format);
	const int n = vsscanf(value, format, args);
	va_end(args);
	return n;
}

bool ini_set(ini_struct* const ini, const char* const section, const char* const key, const char* const value) {
	char* const section_upper = (char*)malloc(strlen(section));
	if (section_upper == NULL) {
		return false;
	}
	memcpy(section_upper, section, strlen(section));
	toupper_str(section_upper, strlen(section));

	char* const key_upper = (char*)malloc(strlen(key));
	if (key_upper == NULL) {
		free(section_upper);
		return false;
	}
	memcpy(key_upper, key, strlen(key));
	toupper_str(key_upper, strlen(key));

	dict_struct* section_dict = NULL;
	if (!dict_get((dict_struct*)ini, section_upper, strlen(section), (void**)&section_dict, NULL)) {
		section_dict = dict_create(1u);
		if (section_dict == NULL) {
			free(key_upper);
			free(section_upper);
			return false;
		}
		if (!dict_set((dict_struct*)ini, section_upper, strlen(section), section_dict, sizeof(section_dict), destroy_section, copy_section)) {
			dict_destroy(section_dict);
			free(key_upper);
			free(section_upper);
			return false;
		}
	}

	if (!dict_set(section_dict, key_upper, strlen(key), (void*)value, strlen(value) + 1u, NULL, NULL)) {
		free(key_upper);
		free(section_upper);
		return false;
	}

	free(key_upper);
	free(section_upper);
	return true;
}

bool ini_setf(ini_struct* const ini, const char* const section, const char* const key, const char* const format, ...) {
	va_list args;
	va_start(args, format);
	char* const value = alloc_vsprintf(format, args);
	va_end(args);

	bool status = ini_set(ini, section, key, value);
	free(value);
	return status;
}

static bool map_keys_size(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	size_t* const size = (size_t*)data;
	*size += key_size + 4u + value_size + 1u;

	return true;
}

static bool map_sections_size(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	size_t* const size = (size_t*)data;
	*size += 1u + key_size + 2u;

	bool status = dict_map((dict_struct*)value, data, map_keys_size);

	(*size)++;

	return status;
}

static bool map_keys_print(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	char** const printout = (char**)data;

	memcpy(*printout, key, key_size);
	strcpy(*printout + key_size, " = \"");
	memcpy(*printout + key_size + 4u, value, value_size - 1u);
	strcpy(*printout + key_size + 4u + value_size - 1u, "\"\n");

	*printout += key_size + 4u + value_size + 1u;

	return true;
}

static bool map_sections_print(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	char** const printout = (char**)data;

	**printout = '[';
	memcpy(*printout + 1u, key, key_size);
	strcpy(*printout + 1u + key_size, "]\n");

	*printout += 1u + key_size + 2u;

	dict_map((dict_struct*)value, data, map_keys_print);

	**printout = '\n';
	(*printout)++;

	return true;
}

bool ini_printout_get(ini_struct* const ini, void** const printout, size_t* const size) {
	if (ini == NULL || printout == NULL || size == NULL) {
		return false;
	}

	*size = 0u;
	dict_map((dict_struct*)ini, size, map_sections_size);

	*printout = malloc(*size);
	if (*printout == NULL) {
		return false;
	}

	void* map_printout = *printout;
	dict_map((dict_struct*)ini, &map_printout, map_sections_print);

	return true;
}
