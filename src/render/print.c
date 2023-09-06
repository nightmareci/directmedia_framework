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

#include "render/print.h"
#include "data/data_types.h"
#include "opengl/opengl.h"
#include "util/linear.h"
#include "util/dict.h"
#include "util/queue.h"
#include "util/string_util.h"
#include "util/util.h"
#include <stdlib.h>
#include <stdarg.h>
#include <float.h>
#include <assert.h>

static bool inited = false;

static const char* const vertex_src =
	"#version 330\n"
	"in vec4 v_position;"
	"out vec2 f_position;"
	"uniform mat4 v_transform;"
	"void main() {"
	"	gl_Position = v_transform * vec4(v_position.xy, 0.0, 1.0);"
	"	f_position = v_position.zw;"
	"}";
static const char* const fragment_src =
	"#version 330\n"
	"in vec2 f_position;"
	"out vec4 f_color;"
	"uniform sampler2D f_texture;"
	"void main() {"
	"	f_color = texture(f_texture, f_position);"
	"}";
static GLuint shader = 0u;

typedef struct printout_object {
	size_t length, size;
	GLuint array;
	GLushort* indices;
	GLuint indices_buffer;
	vec4* v_position;
	GLuint v_position_buffer;
	bool buffers_changed;
} printout_object;

static printout_object* printout_create() {
	printout_object* const printout = (printout_object*)mem_calloc(1, sizeof(printout_object));
	if (printout == NULL) {
		return NULL;
	}

	glGenVertexArrays(1, &printout->array);
	if (opengl_error("Error from glGenVertexArrays: ")) {
		mem_free(printout);
		return NULL;
	}

	glGenBuffers(1, &printout->v_position_buffer);
	if (opengl_error("Error from glGenVertexArrays: ")) {
		glDeleteVertexArrays(1, &printout->array);
		mem_free(printout);
		return NULL;
	}

	glGenBuffers(1, &printout->indices_buffer);
	if (opengl_error("Error from glGenVertexArrays: ")) {
		glDeleteBuffers(1, &printout->v_position_buffer);
		glDeleteVertexArrays(1, &printout->array);
		mem_free(printout);
		return NULL;
	}

	glBindVertexArray(printout->array);
	glBindBuffer(GL_ARRAY_BUFFER, printout->v_position_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, printout->indices_buffer);
	glEnableVertexAttribArray(0u);
	glVertexAttribPointer(0u, 4, GL_FLOAT, GL_FALSE, 0, 0);
	printout->buffers_changed = false;

	return printout;
}

static bool printout_destroy(void* const value) {
	printout_object* const printout = (printout_object*)value;

	if (printout->indices != NULL) {
		mem_free(printout->indices);
	}
	if (printout->v_position != NULL) {
		mem_free(printout->v_position);
	}
	if (printout->array != 0u) {
		glDeleteVertexArrays(1, &printout->array);
	}
	if (printout->indices_buffer != 0u) {
		glDeleteBuffers(1, &printout->indices_buffer);
	}
	if (printout->v_position_buffer != 0u) {
		glDeleteBuffers(1, &printout->v_position_buffer);
	}
	mem_free(printout);

	return true;
}

static bool printout_resize(printout_object* const printout, const size_t new_size) {
	assert(
		printout != NULL &&
		new_size * 6u <= UINT16_MAX
	);

	if (new_size == printout->size) {
		return true;
	}
	else if (new_size == 0u) {
		if (printout->size > 0u) {
			mem_free(printout->v_position);
			mem_free(printout->indices);
			printout->v_position = NULL;
			printout->indices = NULL;
		}

		printout->length = 0u;
		printout->size = 0u;
		return true;
	}
	else {
		vec4* const v_position = (vec4*)mem_realloc(printout->v_position, new_size * 4u * sizeof(vec4));
		if (v_position == NULL) {
			return false;
		}
		printout->v_position = v_position;

		GLushort* const indices = mem_realloc(printout->indices, new_size * 6u * sizeof(GLushort));
		if (indices == NULL) {
			return false;
		}
		printout->indices = indices;

		printout->size = new_size;

		if (new_size < printout->length) {
			printout->length = new_size;
		}
		return true;
	}
}

struct print_data_object {
	dict_object* printouts;
};

bool print_init() {
	assert(!inited);

	shader = opengl_program_create(vertex_src, fragment_src);
	if (shader == 0u) {
		fprintf(stderr, "Error: Failed to create the text printing shader\n");
		fflush(stderr);
		return false;
	}
	glUseProgram(shader);

	glBindAttribLocation(shader, 0u, "v_position");
	glBindFragDataLocation(shader, 0u, "f_color");
	glUniform1i(glGetUniformLocation(shader, "f_texture"), 0);

	inited = true;
	return true;
}

void print_deinit() {
	assert(inited);

	glDeleteProgram(shader);
	shader = 0u;

	inited = false;
}

print_data_object* print_data_create() {
	assert(inited);

	print_data_object* const data = mem_malloc(sizeof(print_data_object));
	if (data == NULL) {
		return NULL;
	}

	data->printouts = dict_create(1u);
	if (data->printouts == NULL) {
		mem_free(data);
		return NULL;
	}

	return data;
}

void print_data_destroy(print_data_object* const data) {
	assert(data != NULL);

	dict_destroy(data->printouts);
	mem_free(data);
}

static bool size_reset(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	printout_object* const printout = (printout_object*)value;

	printout->length = 0u;

	return true;
}

bool print_size_reset(print_data_object* const data) {
	assert(inited && data != NULL);

	return dict_map(data->printouts, NULL, size_reset);
}

static bool size_shrink(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	printout_object* const printout = (printout_object*)value;

	return printout_resize(printout, printout->length);
}

bool print_size_shrink(print_data_object* const data) {
	assert(inited && data != NULL);

	return dict_map(data->printouts, NULL, size_shrink);
}

bool print_text(print_data_object* const data, data_font_object* const font, const float x, const float y, const char* const text) {
	assert(
		inited &&
		data != NULL &&
		font != NULL &&
		x >= -FLT_MAX && x <= FLT_MAX &&
		y >= -FLT_MAX && y <= FLT_MAX &&
		text != NULL
	);

	/*
	 * TODO: Create a new font generator that sets the Unicode bit properly. It
	 * seems the AngelCode generator has a bug, where it doesn't set the
	 * Unicode bit for Unicode binary format fonts.
	 */
	//if (!(font->font->bits1 & FONT_BITS1_UNICODE)) {
	if (font->font->format != FONT_FORMAT_BINARY && !(font->font->bits1 & FONT_BITS1_UNICODE)) {
		fprintf(stderr, "Error: Font used for printing text is not a Unicode font\n");
		fflush(stderr);
		return false;
	}

	if (strlen(text) == 0u) {
		return true;
	}

	float print_x = x, print_y = y;
	const size_t len = utf8_strlen(text);
	size_t bytes;
	uint32_t c = utf8_get(text, &bytes);
	const char* s = text + bytes;
	for (uint32_t next_c = 0u; c != 0u; c = next_c, s += bytes) {
		next_c = utf8_get(s, &bytes);
		if (c == '\n' || c == '\r') {
			print_x = x;
			print_y += font->font->line_h;
			if (
				(c == '\n' && next_c == '\r') ||
				(c == '\r' && next_c == '\n')
			) {
				s += bytes;
				next_c = utf8_get(s, &bytes);
			}
			continue;
		}

		const font_char_struct* font_c;
		if (!font_char_get(font->font, c, &font_c)) {
			continue;
		}

		printout_object* printout = NULL;
		if (!dict_get(data->printouts, &font->textures[font_c->page]->texture->name, sizeof(GLuint), (void**)&printout, NULL)) {
			printout = printout_create();
			if (printout == NULL) {
				fprintf(stderr, "Error: Failed allocating printout\n");
				fflush(stderr);
				return false;
			}
			if (!dict_set(data->printouts, &font->textures[font_c->page]->texture->name, sizeof(GLuint), printout, sizeof(printout), printout_destroy, NULL)) {
				fprintf(stderr, "Error: Failed adding a printout to the print data\n");
				fflush(stderr);
				return false;
			}
		}

		if (printout->size == 0u) {
			printout_resize(printout, len);
		}
		else if (printout->size == printout->length) {
			printout_resize(printout, printout->length * 2u);
		}

		// left
		// right
		// top
		// bottom
		const vec4 draw_bounds = {
			print_x + font_c->x_offset,
			print_x + font_c->x_offset + font_c->w,
			print_y + font_c->y_offset,
			print_y + font_c->y_offset + font_c->h
		};
		const vec4 sample_bounds = {
			font_c->x / (float)font->font->scale_w,
			(font_c->x + font_c->w) / (float)font->font->scale_w,
			font_c->y / (float)font->font->scale_h,
			(font_c->y + font_c->h) / (float)font->font->scale_h
		};

		const size_t start = printout->length * 4u;

		vec4* const vertices = printout->v_position + start;
		// upper left
		vertices[0][0] = draw_bounds[0];
		vertices[0][1] = draw_bounds[2];
		vertices[0][2] = sample_bounds[0];
		vertices[0][3] = sample_bounds[2];
		// upper right
		vertices[1][0] = draw_bounds[1];
		vertices[1][1] = draw_bounds[2];
		vertices[1][2] = sample_bounds[1];
		vertices[1][3] = sample_bounds[2];
		// lower left
		vertices[2][0] = draw_bounds[0];
		vertices[2][1] = draw_bounds[3];
		vertices[2][2] = sample_bounds[0];
		vertices[2][3] = sample_bounds[3];
		// lower right
		vertices[3][0] = draw_bounds[1];
		vertices[3][1] = draw_bounds[3];
		vertices[3][2] = sample_bounds[1];
		vertices[3][3] = sample_bounds[3];

		GLushort* const indices = printout->indices + printout->length * 6u;
		// upper left triangle
		// lower left
		indices[0] = start + 2u;
		// upper right
		indices[1] = start + 1u;
		// upper left
		indices[2] = start + 0u;

		// lower right triangle
		// lower left
		indices[3] = start + 2u;
		// lower right
		indices[4] = start + 3u;
		// upper right
		indices[5] = start + 1u;

		printout->length++;
		printout->buffers_changed = true;

		print_x += font_c->x_advance;
		ptrdiff_t amount;
		if (font_kerning_amount_get(font->font, c, next_c, &amount)) {
			print_x += amount;
		}
	}
	return true;
}

bool print_formatted(print_data_object* const data, data_font_object* const font, const float x, const float y, const char* const format, ...) {
	assert(
		inited &&
		data != NULL &&
		font != NULL &&
		x >= -FLT_MAX && x <= FLT_MAX &&
		y >= -FLT_MAX && y <= FLT_MAX &&
		format != NULL
	);

	va_list args;
	va_start(args, format);
	char* const text = alloc_vsprintf(format, args);
	if (text == NULL) {
		va_end(args);
		return false;
	}
	const bool success = print_text(data, font, x, y, text);
	mem_free(text);
	va_end(args);

	return success;
}

static bool printout_draw(void* const data, const void* const key, const size_t key_size, void* const value, const size_t value_size) {
	const GLuint texture_name = *(GLuint*)key;
	printout_object* const printout = (printout_object*)value;

	glBindVertexArray(0u);
	glBindBuffer(GL_ARRAY_BUFFER, printout->v_position_buffer);
	GLint buffer_size;
	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &buffer_size);
	if (printout->size != buffer_size / (4u * sizeof(vec4))) {
		glBufferData(GL_ARRAY_BUFFER, printout->size * 4u * sizeof(vec4), NULL, GL_DYNAMIC_DRAW);
		if (opengl_error("Error from glBufferData: ")) {
			return false;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, printout->size * 4u * sizeof(vec4), printout->v_position);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, printout->indices_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, printout->size * 6u * sizeof(GLushort), NULL, GL_DYNAMIC_DRAW);
		if (opengl_error("Error from glBufferData: ")) {
			return false;
		}
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, printout->size * 6u * sizeof(GLushort), printout->indices);
	}
	else if (printout->buffers_changed) {
		glBindBuffer(GL_ARRAY_BUFFER, printout->v_position_buffer);
		glBufferSubData(GL_ARRAY_BUFFER, 0, printout->length * 4u * sizeof(vec4), printout->v_position);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, printout->indices_buffer);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, printout->length * 6u * sizeof(GLushort), printout->indices);
		printout->buffers_changed = false;
	}

	if (printout->length == 0u) {
		return true;
	}

	glBindVertexArray(printout->array);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_name);
	glDrawElements(GL_TRIANGLES, printout->length * 6u, GL_UNSIGNED_SHORT, 0);
	return !opengl_error("Error from glDrawElements in printout_draw: ");
}

bool print_draw(print_data_object* const data, const float width, const float height) {
	assert(
		inited &&
		data != NULL &&
		width > 0.0f && width <= FLT_MAX &&
		height > 0.0f && height <= FLT_MAX
	);

	const bool reenable_depth = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
	if (reenable_depth) {
		glDisable(GL_DEPTH_TEST);
	}

	glUseProgram(shader);
	mat4 v_transform;
	mat4_ortho(v_transform, 0.0f, width, height, 0.0f, 1.0f, -1.0f);
	glUniformMatrix4fv(glGetUniformLocation(shader, "v_transform"), 1, GL_FALSE, v_transform);

	const bool status = dict_map(data->printouts, NULL, printout_draw);

	if (reenable_depth) {
		glEnable(GL_DEPTH_TEST);
	}
	return status;
}
