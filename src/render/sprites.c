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

#include "render/sprites.h"
#include "opengl/opengl.h"
#include "util/mathematics.h"
#include "util/mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <assert.h>

static const char* const vertex_src = "\
#version 330\n\
in vec4 dst;\
in vec4 src;\
out vec2 f_position;\
uniform vec2 screen_dimensions;\
uniform vec2 sheet_dimensions;\
const vec2 vertices[6] = vec2[] (\
	vec2(0.0, 1.0),\
	vec2(1.0, 0.0),\
	vec2(0.0, 0.0),\
	vec2(0.0, 1.0),\
	vec2(1.0, 1.0),\
	vec2(1.0, 0.0)\
);\
void main() {\
	gl_Position = vec4(((dst.xy + dst.zw * vertices[gl_VertexID % 6]) * screen_dimensions) * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);\
	f_position = (src.xy + vertices[gl_VertexID % 6] * src.zw) * sheet_dimensions;\
}\
";

static const char* const fragment_src = "\
#version 330\n\
in vec2 f_position;\
out vec4 out_color;\
uniform sampler2D sheet;\
void main() {\
	out_color = texture(sheet, f_position);\
}\
";

typedef struct sprites_sequence {
	data_texture_object* sheet;
	size_t start;
	size_t num_sprites;
} sprites_sequence;

struct sprites_object {
	sprites_sequence* sequences;
	size_t sequences_length, sequences_size;

	sprite_type* sprites;
	size_t sprites_length, sprites_size;

	GLuint array;
	GLuint buffer;
	size_t new_sprites_start;
	bool buffer_changed;

	GLuint shader;
	vec2 last_screen;
	GLint screen_dimensions_location;
	GLint sheet_dimensions_location;
	GLint sheet_location;
};

sprites_object* sprites_create(const size_t initial_size) {
	sprites_object* const sprites = (sprites_object*)mem_malloc(sizeof(sprites_object));
	if (sprites == NULL) {
		return NULL;
	}

	sprites->sequences = NULL;
	sprites->sequences_length = 0u;
	sprites->sequences_size = 0u;

	if (initial_size == 0u) {
		sprites->sprites = NULL;
	}
	else {
		sprites->sprites = (sprite_type*)mem_malloc(sizeof(sprite_type) * initial_size);
		if (sprites->sprites == NULL) {
			mem_free(sprites);
		}
	}

	sprites->sprites_length = 0u;
	sprites->sprites_size = initial_size;

	glGenVertexArrays(1, &sprites->array);
	if (opengl_error("Error from glGenVertexArrays in sprites_create: ")) {
		if (sprites->sprites != NULL) {
			mem_free(sprites->sprites);
		}
		mem_free(sprites);
		return NULL;
	}
	glBindVertexArray(sprites->array);

	glGenBuffers(1, &sprites->buffer);
	if (opengl_error("Error from glGenBuffers in sprites_create: ")) {
		glDeleteVertexArrays(1, &sprites->array);
		if (sprites->sprites != NULL) {
			mem_free(sprites->sprites);
		}
		mem_free(sprites);
		return NULL;
	}
	glBindBuffer(GL_ARRAY_BUFFER, sprites->buffer);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(initial_size * sizeof(sprite_type)), NULL, GL_DYNAMIC_DRAW);
	if (opengl_error("Error from glBufferData in sprites_create: ")) {
		glDeleteBuffers(1, &sprites->buffer);
		glDeleteVertexArrays(1, &sprites->array);
		if (sprites->sprites != NULL) {
			mem_free(sprites->sprites);
		}
		mem_free(sprites);
		return NULL;
	}

	sprites->new_sprites_start = 0u;
	sprites->buffer_changed = false;

	sprites->shader = opengl_program_create(vertex_src, fragment_src);
	if (sprites->shader == 0u) {
		fprintf(stderr, "Error in sprites_create: Failed to create the sprite shader\n");
		fflush(stderr);
		glDeleteBuffers(1, &sprites->buffer);
		glDeleteVertexArrays(1, &sprites->array);
		if (sprites->sprites != NULL) {
			mem_free(sprites->sprites);
		}
		mem_free(sprites);
		return NULL;
	}
	glUseProgram(sprites->shader);

	const GLint dst_location = glGetAttribLocation(sprites->shader, "dst");
	const GLint src_location = glGetAttribLocation(sprites->shader, "src");
	glBindFragDataLocation(sprites->shader, 0u, "out_color");
	sprites->sheet_location = glGetUniformLocation(sprites->shader, "sheet");
	glUniform1i(sprites->sheet_location, 0);
	sprites->last_screen[0] = -1.0f;
	sprites->last_screen[1] = -1.0f;
	sprites->screen_dimensions_location = glGetUniformLocation(sprites->shader, "screen_dimensions");
	sprites->sheet_dimensions_location = glGetUniformLocation(sprites->shader, "sheet_dimensions");

	glBindBuffer(GL_ARRAY_BUFFER, sprites->buffer);
	glEnableVertexAttribArray(dst_location);
	glEnableVertexAttribArray(src_location);
	glVertexAttribPointer(dst_location, 4, GL_FLOAT, GL_FALSE, sizeof(sprite_type), (void*)offsetof(sprite_type, dst));
	glVertexAttribPointer(src_location, 4, GL_FLOAT, GL_FALSE, sizeof(sprite_type), (void*)offsetof(sprite_type, src));
	glVertexAttribDivisor(dst_location, 1u);
	glVertexAttribDivisor(src_location, 1u);
	sprites->new_sprites_start = 0u;
	sprites->buffer_changed = false;

	if (initial_size > 0u && !sprites_resize(sprites, initial_size)) {
		glDeleteBuffers(1, &sprites->buffer);
		glDeleteVertexArrays(1, &sprites->array);
		if (sprites->sprites != NULL) {
			mem_free(sprites->sprites);
		}
		mem_free(sprites);
		return NULL;
	}

	sprites_screen_reset(sprites);
	glBindBuffer(GL_ARRAY_BUFFER, 0u);

	return sprites;
}

void sprites_destroy(sprites_object* const sprites) {
	assert(sprites != NULL);

	if (sprites->sequences != NULL) {
		mem_free(sprites->sequences);
	}
	if (sprites->sprites != NULL) {
		mem_free(sprites->sprites);
	}
	if (sprites->array != 0u) {
		glDeleteVertexArrays(1, &sprites->array);
	}
	if (sprites->buffer != 0u) {
		glDeleteBuffers(1, &sprites->buffer);
	}
	mem_free(sprites);
}

bool sprites_resize(sprites_object* const sprites, const size_t num_sprites) {
	assert(sprites != NULL);

	if (num_sprites == sprites->sprites_size) {
		return true;
	}
	else if (num_sprites == 0u) {
		if (sprites->sequences_size > 0u) {
			mem_free(sprites->sequences);
			sprites->sequences = NULL;
		}

		if (sprites->sprites_size > 0u) {
			mem_free(sprites->sprites);
			sprites->sprites = NULL;
		}

		sprites->sequences_length = 0u;
		sprites->sequences_size = 0u;

		sprites->sprites_length = 0u;
		sprites->sprites_size = 0u;

		sprites->new_sprites_start = 0u;
		sprites->buffer_changed = true;

		return true;
	}
	else {
		if (sprites->sequences_length > 0u && num_sprites < sprites->sprites_length) {
			sprites_sequence* sequence = &sprites->sequences[0u];
			size_t new_sequences_length = 1u;
			while(sequence->start + sequence->num_sprites < num_sprites) {
				sequence++;
				new_sequences_length++;
			}
			sequence->num_sprites = num_sprites - sequence->start;
			if (new_sequences_length < sprites->sequences_length) {
				sprites_sequence* const new_sequences = (sprites_sequence*)mem_realloc(sprites->sequences, new_sequences_length * sizeof(sprites_sequence));
				if (new_sequences == NULL) {
					mem_free(sprites->sequences);
					sprites->sequences = NULL;
					return false;
				}
				sprites->sequences = new_sequences;
			}
			sprites->sequences_length = new_sequences_length;
			sprites->sequences_size = new_sequences_length;
		}

		sprite_type* const new_sprites = (sprite_type*)mem_realloc(sprites->sprites, num_sprites * sizeof(sprite_type));
		if (new_sprites == NULL) {
			return false;
		}
		sprites->sprites = new_sprites;

		sprites->sprites_size = num_sprites;
		if (num_sprites < sprites->sprites_length) {
			sprites->sprites_length = num_sprites;
		}
		return true;
	}
}

bool sprites_shrink(sprites_object* const sprites) {
	assert(sprites != NULL);

	return sprites_resize(sprites, sprites->sprites_length);
}

void sprites_screen_reset(sprites_object* const sprites) {
	assert(sprites != NULL);

	GLfloat viewport[4];
	glGetFloatv(GL_VIEWPORT, viewport);
	assert(
		viewport[2] > 0.0f &&
		viewport[2] <= FLT_MAX &&
		viewport[3] > 0.0f &&
		viewport[3] <= FLT_MAX
	);
	sprites->last_screen[0] = 1.0f / viewport[2];
	sprites->last_screen[1] = 1.0f / viewport[3];
	glUseProgram(sprites->shader);
	glUniform2fv(sprites->screen_dimensions_location, 1, sprites->last_screen);
}

void sprites_screen_set(sprites_object* const sprites, const float screen_width, const float screen_height) {
	assert(
		sprites != NULL &&
		screen_width >= 0.0f &&
		screen_width <= FLT_MAX &&
		screen_height >= 0.0f &&
		screen_height <= FLT_MAX
	);

	if (screen_width == 0.0f || screen_height == 0.0f) {
		sprites_screen_reset(sprites);
	}
	else if (sprites->last_screen[0] != screen_height || sprites->last_screen[1] != screen_width) {
		sprites->last_screen[0] = screen_width;
		sprites->last_screen[1] = screen_height;
		glUseProgram(sprites->shader);
		glUniform2f(sprites->screen_dimensions_location, 1.0f / screen_width, 1.0f / screen_height);
	}
}

bool sprites_add(sprites_object* const sprites, data_texture_object* const sheet, const size_t num_added, sprite_type* const added_sprites) {
	assert(
		sprites != NULL &&
		sheet != NULL &&
		num_added <= UINT16_MAX &&
		sprites->sprites_length <= UINT16_MAX - num_added &&
		added_sprites != NULL
	);
	
	if (num_added == 0u) {
		return true;
	}

	const size_t new_sequences_length = sprites->sequences_length + 1u;
	if (new_sequences_length > sprites->sequences_size) {
		const size_t new_sequences_size = new_sequences_length * 2u;
		sprites_sequence* const new_sequences = (sprites_sequence*)mem_realloc(sprites->sequences, new_sequences_size * sizeof(sprites_sequence));
		if (new_sequences == NULL) {
			return false;
		}
		sprites->sequences = new_sequences;
		sprites->sequences_size = new_sequences_size;
	}

	sprites_sequence* const new_sequence = &sprites->sequences[sprites->sequences_length];
	new_sequence->sheet = sheet;
	new_sequence->start = sprites->sprites_length;
	new_sequence->num_sprites = num_added;

	sprites->sequences_length++;

	const size_t new_sprites_length = sprites->sprites_length + num_added;
	if (new_sprites_length > sprites->sprites_size && !sprites_resize(sprites, new_sprites_length * 2u)) {
		return false;
	}

	memcpy(sprites->sprites + sprites->sprites_length, added_sprites, num_added * sizeof(sprite_type));

	sprites->sprites_length += num_added;
	if (!sprites->buffer_changed) {
		sprites->new_sprites_start = new_sequence->start;
		sprites->buffer_changed = true;
	}

	return true;
}

bool sprites_draw(sprites_object* const sprites) {
	assert(sprites != NULL);

	if (
		sprites->sprites_length == 0u ||
		sprites->last_screen[0] <= 0.0f ||
		sprites->last_screen[1] <= 0.0f
	) {
		return true;
	}

	glBindVertexArray(0u);
	GLint buffer_size;
	glBindBuffer(GL_ARRAY_BUFFER, sprites->buffer);
	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &buffer_size);

	if (sprites->sprites_size != buffer_size / sizeof(sprite_type)) {
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sprites->sprites_size * sizeof(sprite_type)), NULL, GL_DYNAMIC_DRAW);
		if (opengl_error("Error from glBufferData in sprites_draw: ")) {
			return false;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, sprites->sprites_length * sizeof(sprite_type), sprites->sprites);
		sprites->new_sprites_start = sprites->sprites_length;
		sprites->buffer_changed = false;
	}
	else if (sprites->buffer_changed && sprites->new_sprites_start < sprites->sprites_length) {
		glBufferSubData(GL_ARRAY_BUFFER, 0, (sprites->sprites_length - sprites->new_sprites_start) * sizeof(sprite_type), sprites->sprites + sprites->new_sprites_start);
		sprites->new_sprites_start = sprites->sprites_length;
		sprites->buffer_changed = false;
	}

	glDisable(GL_DEPTH_TEST);
	glUseProgram(sprites->shader);
	glBindVertexArray(sprites->array);
	glActiveTexture(GL_TEXTURE0);
	const GLint dst_location = glGetAttribLocation(sprites->shader, "dst");
	const GLint src_location = glGetAttribLocation(sprites->shader, "src");

	for (size_t i = 0u; i < sprites->sequences_length; i++) {
		size_t start_sprites_batched = sprites->sequences[i].start;
		size_t num_sprites_batched = sprites->sequences[i].num_sprites;
		data_texture_object* current_sheet = sprites->sequences[i].sheet;
		while (i + 1u < sprites->sequences_length) {
			if (sprites->sequences[i + 1u].sheet == current_sheet) {
				i++;
				num_sprites_batched += sprites->sequences[i].num_sprites;
			}
			else {
				break;
			}
		}

		glBindTexture(GL_TEXTURE_2D, current_sheet->name);
		glUniform2f(sprites->sheet_dimensions_location, 1.0f / current_sheet->width, 1.0f / current_sheet->height);
		glVertexAttribPointer(dst_location, 4, GL_FLOAT, GL_FALSE, sizeof(sprite_type), (void*)(offsetof(sprite_type, dst) + start_sprites_batched * sizeof(sprite_type)));
		glVertexAttribPointer(src_location, 4, GL_FLOAT, GL_FALSE, sizeof(sprite_type), (void*)(offsetof(sprite_type, src) + start_sprites_batched * sizeof(sprite_type)));
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, num_sprites_batched);
		if (opengl_error("Error from glDrawArraysInstanced in sprites_draw: ")) {
			return false;
		}
	}

	return true;
}

void sprites_restart(sprites_object* const sprites) {
	assert(sprites != NULL);

	sprites->sequences_length = 0u;
	sprites->sprites_length = 0u;
	sprites->new_sprites_start = 0u;
	sprites->buffer_changed = true;
}
