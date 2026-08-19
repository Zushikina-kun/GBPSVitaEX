#pragma once
#include <stdint.h>
typedef enum {
    MENU_ACTION_NONE = 0,
    MENU_ACTION_RESUME,
    MENU_ACTION_RESET,
    MENU_ACTION_SAVE_STATE,
    MENU_ACTION_LOAD_STATE,
    MENU_ACTION_LOAD_ROM,
    MENU_ACTION_SETTINGS,
    MENU_ACTION_SCREENSHOT,
    MENU_ACTION_RFU_HOST,    /* start RFU WiFi multiplayer as host */
    MENU_ACTION_RFU_CLIENT,  /* join RFU WiFi multiplayer as client */
    MENU_ACTION_RFU_STOP,    /* stop RFU multiplayer */
    MENU_ACTION_LINK_START,  /* start GB link cable (prompts for P2 ROM) */
    MENU_ACTION_LINK_STOP,   /* stop GB link cable */
    MENU_ACTION_EXIT,
} MenuAction;

MenuAction menu_update(uint32_t buttons);
void       menu_draw(void);
