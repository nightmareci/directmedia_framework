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

#include "data/data_font.h"
#include <stddef.h>

/*
 * Simple bitmap font renderer. Only supports rendering UTF-8 encoded text; that
 * limitation also requires that the font used for rendering is a Unicode font.
 *
 * print_draw must be called for the text to actually be rendered; the other
 * functions just modify data in the print_data_struct object, and do no actual
 * rendering. You could call it once for each render update.
 *
 * It requires that an OpenGL context be current while it's being used; the
 * context only has to be current before a call of print_init, then the other
 * print_* functions can be used, finishing with print_deinit, at which point
 * the context doesn't have to be current.
 */

typedef struct print_data_object print_data_object;

/*
 * Initialize the printing subsystem.
 */
bool print_init();

/*
 * Deinitialize the printing subsystem.
 */
void print_deinit();

/*
 * Create a print data object for printing.
 */
print_data_object* print_data_create();

/*
 * Destroy data object for printing.
 */
void print_data_destroy(print_data_object* const data);

/*
 * Reset the text currently to be printed in the print data to no text. Doesn't
 * change the size of the data. Call this first, then print_data_size_shrink, to
 * free the memory associated with the print data.
 */
bool print_size_reset(print_data_object* const data);

/*
 * Shrink the data associated with the print data to the size that fits only the
 * text currently to be printed.
 */
bool print_size_shrink(print_data_object* const data);

/*
 * Print the text at the requested position.
 */
bool print_text(print_data_object* const data, data_font_object* const font, const float x, const float y, const char* const text);

/*
 * Print the formatted text at the requested position.
 */
bool print_formatted(print_data_object* const data, data_font_object* const font, const float x, const float y, const char* const format, ...);

/*
 * Draw all text on screen.
 */
bool print_draw(print_data_object* const data, const float width, const float height);
