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

#include "data/data_types.h"
#include "util/log.h"
#include "util/mem.h"
#include "SDL_image.h"
#include "SDL_surface.h"

static bool create(data_object* const data, SDL_RWops* const rwops) {
	data->texture = mem_calloc(1u, sizeof(data_texture_object));
	if (data->texture == NULL) {
		log_printf("Error allocating data while loading a texture\n");
		return false;
	}

	SDL_Surface* surface = IMG_Load_RW(rwops, 0);
	if (surface == NULL) {
		return false;
	}

	if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
		SDL_Surface* const old_surface = surface;
		surface = SDL_ConvertSurfaceFormat(old_surface, SDL_PIXELFORMAT_RGBA32, 0);
		SDL_FreeSurface(old_surface);
		if (surface == NULL) {
			return false;
		}
	}
	
	GLuint name;
	glGenTextures(1, &name);
	if (opengl_error("Error from glGenTextures while loading a texture: ")) {
		SDL_FreeSurface(surface);
		return false;
	}

	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
	if (opengl_error("Error from glTexImage2D while loading a texture: ")) {
		SDL_FreeSurface(surface);
		glDeleteTextures(1, &name);
		return false;
	}

	data->texture->name = name;
	data->texture->width = surface->w;
	data->texture->height = surface->h;
	return true;
}

static bool destroy(data_object* const data) {
	glDeleteTextures(1, &data->texture->name);
	mem_free((void*)data->id.filename);
	mem_free((void*)data->texture);
	mem_free((void*)data);

	return true;
}

static bool create(data_object* const data, SDL_RWops* const rwops);
static bool destroy(data_object* const data);

DATA_TYPE_MANAGER_DEFINITION(data_type_manager_texture, create, destroy);
