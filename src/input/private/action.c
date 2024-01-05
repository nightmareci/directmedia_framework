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

#include "input/action.h"
#include "SDL.h"
#include "SDL_keyboard.h"
#include <assert.h>

bool action_bool_get(const size_t set, const size_t action) {
	const Uint8* const keys = SDL_GetKeyboardState(NULL);
	SDL_Scancode scancode;

	switch (set) {
	case ACTION_SET_BASIC_MENU:
		switch (action) {
		case BASIC_MENU_UP:       scancode = SDL_GetScancodeFromKey(SDLK_UP); break;
		case BASIC_MENU_DOWN:     scancode = SDL_GetScancodeFromKey(SDLK_DOWN); break;
		case BASIC_MENU_LEFT:     scancode = SDL_GetScancodeFromKey(SDLK_LEFT); break;
		case BASIC_MENU_RIGHT:    scancode = SDL_GetScancodeFromKey(SDLK_RIGHT); break;
		case BASIC_MENU_POSITIVE: scancode = SDL_GetScancodeFromKey(SDLK_RETURN); break;
		case BASIC_MENU_NEGATIVE: scancode = SDL_GetScancodeFromKey(SDLK_ESCAPE); break;
		default: return false;
		}
		if (scancode == SDL_SCANCODE_UNKNOWN) return false;
		else return !!keys[scancode];
	default:
		return false;
	}
}

float action_float_get(const size_t set, const size_t action) {
	const Uint8* const keys = SDL_GetKeyboardState(NULL);
	SDL_Scancode scancode;

	switch (set) {
	case ACTION_SET_BASIC_MENU:
		switch (action) {
		case BASIC_MENU_UP:       scancode = SDL_GetScancodeFromKey(SDLK_UP); break;
		case BASIC_MENU_DOWN:     scancode = SDL_GetScancodeFromKey(SDLK_DOWN); break;
		case BASIC_MENU_LEFT:     scancode = SDL_GetScancodeFromKey(SDLK_LEFT); break;
		case BASIC_MENU_RIGHT:    scancode = SDL_GetScancodeFromKey(SDLK_RIGHT); break;
		case BASIC_MENU_POSITIVE: scancode = SDL_GetScancodeFromKey(SDLK_RETURN); break;
		case BASIC_MENU_NEGATIVE: scancode = SDL_GetScancodeFromKey(SDLK_ESCAPE); break;
		default: return 0.0f;
		}
		if (scancode == SDL_SCANCODE_UNKNOWN) return 0.0f;
		else return keys[scancode] ? 1.0f : 0.0f;
	default:
		return 0.0f;
	}
}
