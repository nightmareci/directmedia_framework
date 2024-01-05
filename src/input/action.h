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
 * TODO: Support adding custom action sets and action set layers and write the
 * guide explaining how new action sets and action set layers should be
 * defined.
 */

#include <stddef.h>
#include <stdbool.h>

/*
 * Action sets are a means to support inputs with semantics, where the engine
 * manages the mappings from semantic actions to device-specific inputs, rather
 * than requiring the game manually handle all device-specific inputs in each
 * input context.
 *
 * Always opt for these builtin action sets first, only opting for customizing
 * inputs where the builtin action sets aren't appropriate or sufficient for
 * the input context that needs to be supported, following the guide on how to
 * best define new inputs following established conventions/players'
 * expectations.
 */
typedef enum action_set_type {
	/*
	 * For basic menus, like the main menu shown at startup with menu
	 * entries like START GAME and SETTINGS, or the pause menu with similar
	 * such menu entries.
	 */
	ACTION_SET_BASIC_MENU,

	ACTION_SET_NUM_BUILTIN
} action_set_type;

/*
 * UP, DOWN, LEFT, and RIGHT are for navigating the menu and selecting among a
 * set of options at a particular entry in the menu, such as selecting a
 * graphics preset (e.g., quality of low, medium, or high). The directional
 * actions are restricted such that either all directions are inactive per game
 * tick, or only one of the four directions per game tick is active.
 *
 * POSITIVE and NEGATIVE are for making immediate state changes in a menu, not
 * navigation nor selection. E.g., the player presses POSITIVE on a save entry
 * in a menu, then a DO YOU WANT TO SAVE? YES/NO confirmation dialog would be
 * shown, POSITIVE selecting YES, NEGATIVE selecting NO.
 *
 * Carefully consider the in-game labeling that corresponds to POSITIVE and
 * NEGATIVE; POSITIVE and NEGATIVE are generic abstractions used by the
 * developer, with context-specific labeling of them shown in-game to the
 * player. Always showing POSITIVE and NEGATIVE labels regardless of context
 * will confuse players where players aren't expecting those specific labels.
 * POSITIVE and NEGATIVE are restricted such that neither or only one of them
 * is active per game tick; both will never be simultaneously active.
 *
 * All basic menu actions are boolean.
 */
typedef enum basic_menu_type {
	BASIC_MENU_UP,
	BASIC_MENU_DOWN,
	BASIC_MENU_LEFT,
	BASIC_MENU_RIGHT,
	BASIC_MENU_POSITIVE,
	BASIC_MENU_NEGATIVE,
	BASIC_MENU_NUM_ACTIONS
} basic_menu_type;

/*
 * Get the current state of an action set's boolean action. false is always
 * returned if a smoothly-ranged action is requested, unless the
 * smoothly-ranged action defines under what conditions true or false are
 * returned.
 */
bool action_bool_get(const size_t set, const size_t action);

/*
 * Get the current value of an action set's smoothly-ranged action, such as the
 * current position of an analog axis. Smoothly-ranged actions define their
 * particular range, so check the documentation of smoothly-ranged actions to
 * know how to interpret their values. If used for boolean actions, 1.0f is
 * returned when action_bool_get would return true, 0.0f if action_bool_get
 * would return false.
 */
float action_float_get(const size_t set, const size_t action);
