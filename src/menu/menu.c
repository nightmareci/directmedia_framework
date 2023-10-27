#include "menu/menu.h"
#include "util/mem.h"

typedef struct menu_private {
    menu_public data;

    SDL_GameController* controller;
} menu_private;


// TODO
menu_public* menu_create() {
    return mem_calloc(1u, sizeof(menu_private));
}

// TODO
void menu_destroy(menu_public* const menu) {
    mem_free((menu_private*)menu);
    return;
}

// TODO
bool menu_update(menu_public* const menu) {
    return true;
}
