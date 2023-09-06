#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

typedef enum menu_input {
    MENU_INPUT_QUIT     =  INT32_MIN,
    MENU_INPUT_NONE     =  0,

    MENU_INPUT_DOWN     = -1,
    MENU_INPUT_UP       = +1,
    MENU_INPUT_LEFT     = -2,
    MENU_INPUT_RIGHT    = +2,
    MENU_INPUT_BACKWARD = -3,
    MENU_INPUT_FORWARD  = +3,

    MENU_INPUT_CHOOSE,
    MENU_INPUT_REFUSE,

    MENU_INPUT_MAX          =  INT32_MAX
} menu_input;

/*
 * Though you can't guarantee which type is chosen for an enum in C by the
 * compiler, by including both min and max constants, you guarantee at least the
 * range required is available for enum constants, which implies fully safe
 * casting between the enum type to/from the integer type corresponding to the
 * selected min and max. It does not guarantee memory equivalence, though; the
 * enum type could be a larger integer type, so the conversion between the enum
 * type and integer type must happen as read-original-type-then-cast, like:
 *
 * // Guaranteed-safe conversion to int32_t of menu_input.
 * menu_input* const input = &menu->input;
 * const int32_t input_integer = (int32_t)*input;
 * function_call(input_integer);
 *
 * Never cast-to-pointer-then-dereference like:
 *
 * // This is not guaranteed to work. Use the above style.
 * int32_t* const input_integer_pointer = (int32_t*)&menu->input;
 * function_call(*input_integer_pointer);
 */

/*
 * Returns true if an input is directional.
 */
#define MENU_INPUT_IS_DIRECTION(input) ((input) != MENU_INPUT_NONE && (input) >= MENU_INPUT_BACKWARD && (input) <= MENU_INPUT_FORWARD)

/*
 * Returns +/- 1 based on a directional input that can be used for updating a
 * coordinate value or 0 if the input is not a direction.
 */
#define MENU_INPUT_STEP(input) (MENU_INPUT_IS_DIRECTION((input)) ? INT32_C(1) - (int32_t)(((uint32_t)(input) & UINT32_C(0x80000000)) >> 30) : 0)

typedef struct menu_public {
    bool active;
    bool quit;
    menu_input input;
} menu_public;

menu_public* menu_create();
void menu_destroy(menu_public* const menu);
bool menu_update(menu_public* const menu);
