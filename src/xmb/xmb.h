#ifndef XMB_H
#define XMB_H

#include <stdbool.h>
#include <stdint.h>

#include "../gpu.h"
#include "../file_manager.h"

#define XMB_MAX_CATEGORIES 8
#define XMB_MAX_ITEMS      16

typedef enum {
    MENU_COMMAND_NONE = 0x0,
    MENU_COMMAND_GOTO_ROOT = 0x1,
    MENU_COMMAND_GOTO_PARENT = 0x2,
    MENU_COMMAND_GOTO_DIRECTORY = 0x3,
    MENU_COMMAND_MOUNT_FILE_FAST = 0x4,
    MENU_COMMAND_MOUNT_FILE_SLOW = 0x5,
    MENU_COMMAND_BOOTLOADER = 0x6
} MENU_COMMAND;

typedef enum {
    XMB_BTN_LEFT = 0,
    XMB_BTN_RIGHT,
    XMB_BTN_UP,
    XMB_BTN_DOWN,
    XMB_BTN_X,
    XMB_BTN_O,
    XMB_BTN_SQUARE,
    XMB_BTN_TRIANGLE,
    XMB_BTN_L1,
    XMB_BTN_R1,
    XMB_BTN_SELECT,
    XMB_BTN_START
} XMBButton;

typedef enum {
    XMB_ITEM_TEXT = 0,
    XMB_ITEM_COMMAND
} XMBItemType;

typedef struct {
    XMBItemType type;
    const char* label;
    MENU_COMMAND command;
} XMBItem;

typedef enum {
    XMB_CATEGORY_BROWSER = 0,
    XMB_CATEGORY_ACTIONS,
    XMB_CATEGORY_INFO
} XMBCategoryType;

typedef enum {
    UI_CATEGORY_GAMES = 0,
    UI_CATEGORY_SYSTEM,
    UI_CATEGORY_CREDITS,
    UI_CATEGORY_COUNT
} UICategoryId;

typedef struct {
    const char* name;
    const char* icon_path;
    XMBCategoryType type;
    XMBItem items[XMB_MAX_ITEMS];
    int item_count;
} XMBCategory;

typedef struct {
    XMBCategory categories[XMB_MAX_CATEGORIES];
    int category_count;
    int current_category;
    int current_item[XMB_MAX_CATEGORIES];
} XMBMenu;

void xmb_init(XMBMenu* menu);
void xmb_handle_input(XMBMenu* menu, int input);
int  xmb_get_current_item(const XMBMenu* menu);
void xmb_set_current_item(XMBMenu* menu, int item);
void xmb_clamp_current_item(XMBMenu* menu, int category);

void xmb_init(XMBMenu* menu);
void xmb_draw(const XMBMenu* menu,
             DMAChain* chain,
             const TextureInfo* font,
             const TextureInfo* logo,
             uint32_t file_count,
             uint16_t page_size,
             uint8_t highlight,
             void (*draw_panel)(DMAChain*, int, int, int, int, uint8_t, uint8_t, uint8_t, bool),
             void (*draw_text)(DMAChain*, const TextureInfo*, int, int, const char*),
             fileData* (*get_file)(uint16_t index));

#endif // XMB_H
