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
#include "opengl/opengl.h"

static bool create(void* const file_param, SDL_RWops* const rwops);
static bool destroy(void* const file_param);

const file_type_manager_struct file_type_manager_font = {
	.create = create,
	.destroy = destroy
};

static bool create(void* const file_param, SDL_RWops* const rwops) {
	file_struct* const file = (file_struct*)file_param;

	const Sint64 size = SDL_RWsize(rwops);
	if (size <= 0) {
		return false;
	}

	void* data = malloc(size);
	if (data == NULL) {
		return false;
	}

	const size_t read = SDL_RWread(rwops, data, 1u, size);
	if (read == 0u || read != size) {
		free(data);
		return false;
	}
    
    file_font_struct* const font = (file_font_struct*)malloc(sizeof(file_font_struct));
	if (font == NULL) {
		free(data);
		return false;
	}

	font->font = font_create(data, size);
	if (font->font == NULL) {
		free(font);
		free(data);
		return false;
	}

	free(data);
    
    font->textures = (GLuint*)malloc(font->font->num_pages * sizeof(GLuint));
	if (font->textures == NULL) {
		font_destroy(font->font);
		free(font);
		return false;
	}

	glGenTextures(font->font->num_pages, font->textures);
	if (opengl_error("Error from glGenTextures: ")) {
		free(font->textures);
		font_destroy(font->font);
		free(font);
		return false;
	}
	for (size_t page = 0u; page < font->font->num_pages; page++) {
		const file_struct* const page_file = file_load(file->cache, FILE_TYPE_SURFACE, file->description.path, font->font->page_names[page], NULL);
		if (page_file == NULL) {
			glDeleteTextures(font->font->num_pages, font->textures);
			free(font->textures);
			font_destroy(font->font);
			free(font);
			return false;
		}
		SDL_Surface* page_surface;
		bool free_page_surface;
		if (page_file->surface->format->format == SDL_PIXELFORMAT_RGBA32) {
			page_surface = page_file->surface;
			free_page_surface = false;
		}
		else {
			page_surface = SDL_ConvertSurfaceFormat(page_file->surface, SDL_PIXELFORMAT_RGBA32, 0);
			free_page_surface = true;
		}

		glBindTexture(GL_TEXTURE_2D, font->textures[page]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, page_surface->w, page_surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, page_surface->pixels);
		if (opengl_error("Error from glTexImage2D: ")) {
			if (free_page_surface) {
				SDL_FreeSurface(page_surface);
			}
			file_unload(page_file);
			glDeleteTextures(font->font->num_pages, font->textures);
			free(font->textures);
			font_destroy(font->font);
			free(font);
			return false;
		}

		if (free_page_surface) {
			SDL_FreeSurface(page_surface);
		}
		file_unload(page_file);
	}

	file->font = font;
	return true;
}

static bool destroy(void* const file_param) {
	file_struct* const file = (file_struct*)file_param;

	glDeleteTextures(file->font->font->num_pages, file->font->textures);
	const bool status = font_destroy(file->font->font);
	free((void*)file->font);
	free((void*)file->description.filename);
	free((void*)file);

	return status;
}
