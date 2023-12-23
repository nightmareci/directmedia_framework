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

#include "util/mem.h"

/*
 * TODO: Change from using a compile-time option for enabling/disabling memory
 * debugging to a runtime option, that must be passed in before startup. That
 * way, the prebuilt release builds only capable of running Lua script code can
 * have memory debugging enabled by users, without having to rebuild with memory
 * debugging enabled. And use separate functions for no-debugging and debugging,
 * so that the unnecessary overhead of conditional checks aren't necessary when
 * debugging is off; just use function pointers to choose the debugging mode.
 */

/*
 * Initializes the memory subsystem. Call first thing at startup, before any
 * dynamic memory allocations, in the main thread, before any threads have been
 * created.
 */
bool mem_init();

/*
 * Deinitializes the memory subsystem. This is entirely optional, but if
 * called, a previous successful call of mem_init must have been made.
 */
bool mem_deinit();

/*
 * Allocation function suitable for passing to lua_newstate().
 */
void* mem_lua_alloc(void* userdata, void* mem, size_t old_size, size_t new_size);
