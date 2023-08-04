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
 * INI file loading library. This library only interprets the raw INI text
 * data, does no file handling, and is pure in-memory, portable code; file
 * management must be done externally. This library is not thread-safe.
 */

#include <stddef.h>
#include <stdbool.h>

/*
 * The object type for INIs.
 */
typedef struct ini_struct ini_struct;

/*
 * Create and return an INI object. Returns NULL if creation failed.
 */
ini_struct* ini_create(const char* const data, const size_t size);

/*
 * Destroy an INI object.
 */
void ini_destroy(ini_struct* const ini);

/*
 * Return a copy of an INI object. Returns NULL if copying failed.
 */
ini_struct* ini_copy(ini_struct* const ini);

/*
 * Copy all sections and keys from the src INI to the dst INI, overwriting keys
 * in the dst INI that are present in the src INI. Returns false if merging
 * failed.
 */
bool ini_merge(ini_struct* const dst, ini_struct* const src);

/*
 * Get and return a key's value in the INI. The returned string is only valid
 * for the lifetime of the INI object. Returns NULL if the key isn't present.
 */
const char* ini_get(ini_struct* const ini, const char* const section, const char* const key);

/*
 * Get a key's value in the INI; effectively just does a call of sscanf over
 * the key's value text. Returns the number of sscanf arguments successfully
 * parsed.
 */
int ini_getf(ini_struct* const ini, const char* const section, const char* const key, const char* const format, ...);

/*
 * Set a key to value text in the INI. Returns false if setting failed.
 */
bool ini_set(ini_struct* const ini, const char* const section, const char* const key, const char* const value);

/*
 * Set a key to value text in the INI; effectively just does a call of sprintf
 * to the key's value text. Returns false if setting failed.
 */
bool ini_setf(ini_struct* const ini, const char* const section, const char* const key, const char* const format, ...);

/*
 * Get a printout of the INI, suitable for writing to a file. The printout is
 * *NOT* a proper NUL-terminated C string, it's a raw buffer that should be
 * written to a file sized exactly the size returned by this function. Returns
 * false if creating the printout failed.
 */
bool ini_printout_get(ini_struct* const ini, void** const printout, size_t* const size);
