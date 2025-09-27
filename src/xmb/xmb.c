#include "xmb.h"

#include <stdio.h>
#include <string.h>

static int wrap_index(int value, int max)
{
    if (max <= 0)
    {
        return 0;
    }

    int result = value % max;
    if (result < 0)
    {
        result += max;
    }
    return result;
}

void xmb_clamp_current_item(XMBMenu* menu, int category)
{
    if (!menu || category < 0 || category >= menu->category_count)
    {
        return;
    }

    int count = menu->categories[category].item_count;
    if (count <= 0)
    {
        menu->current_item[category] = 0;
        return;
    }

    int item = menu->current_item[category];
    if (item < 0)
    {
        item = 0;
    }
    if (item >= count)
    {
        item = count - 1;
    }
    menu->current_item[category] = item;
}

int xmb_get_current_item(const XMBMenu* menu)
{
    if (!menu || menu->category_count <= 0)
    {
        return 0;
    }

    int category = menu->current_category;
    if (category < 0 || category >= menu->category_count)
    {
        return 0;
    }

    return menu->current_item[category];
}

void xmb_set_current_item(XMBMenu* menu, int item)
{
    if (!menu || menu->category_count <= 0)
    {
        return;
    }

    int category = menu->current_category;
    if (category < 0 || category >= menu->category_count)
    {
        return;
    }

    menu->current_item[category] = item;
    xmb_clamp_current_item(menu, category);
}

void xmb_handle_input(XMBMenu* menu, int input)
{
    if (!menu || menu->category_count <= 0)
    {
        return;
    }

    int category = menu->current_category;

    switch (input)
    {
        case XMB_BTN_LEFT:
            if (menu->category_count > 0)
            {
                category = wrap_index(category - 1, menu->category_count);
                menu->current_category = category;
                xmb_clamp_current_item(menu, category);
            }
            break;

        case XMB_BTN_RIGHT:
            if (menu->category_count > 0)
            {
                category = wrap_index(category + 1, menu->category_count);
                menu->current_category = category;
                xmb_clamp_current_item(menu, category);
            }
            break;

        case XMB_BTN_UP:
        {
            int count = menu->categories[category].item_count;
            if (count > 0)
            {
                int item = menu->current_item[category];
                item = wrap_index(item - 1, count);
                menu->current_item[category] = item;
            }
            break;
        }

        case XMB_BTN_DOWN:
        {
            int count = menu->categories[category].item_count;
            if (count > 0)
            {
                int item = menu->current_item[category];
                item = wrap_index(item + 1, count);
                menu->current_item[category] = item;
            }
            break;
        }

        default:
            break;
    }
}

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define HEADER_TEXT_X 16
#define HEADER_TEXT_Y 24

#define CATEGORY_SPACING 96
#define CATEGORY_BOX_WIDTH 88
#define CATEGORY_BOX_HEIGHT 24

#define LIST_PANEL_X 12
#define LIST_PANEL_Y 64
#define LIST_PANEL_WIDTH (SCREEN_WIDTH - (LIST_PANEL_X * 2))
#define LIST_PANEL_HEIGHT 160
#define LIST_ENTRY_OFFSET_X (LIST_PANEL_X + 16)
#define LIST_ENTRY_OFFSET_Y (LIST_PANEL_Y + 20)
#define LIST_ENTRY_HEIGHT 18

#define FOOTER_PRIMARY_Y (SCREEN_HEIGHT - 22)
#define FOOTER_SECONDARY_Y (SCREEN_HEIGHT - 12)

static void truncate_with_ellipsis(const char* source, char* buffer, size_t bufferSize, size_t maxChars)
{
    if (!source || bufferSize == 0)
    {
        return;
    }

    size_t length = strlen(source);
    if (length <= maxChars || maxChars < 4)
    {
        strncpy(buffer, source, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }

    size_t copyLen = maxChars - 3;
    if (copyLen >= bufferSize)
    {
        copyLen = bufferSize - 4;
    }

    strncpy(buffer, source, copyLen);
    buffer[copyLen] = '\0';
    strncat(buffer, "...", bufferSize - strlen(buffer) - 1);
}

static void draw_categories(const XMBMenu* menu,
                            DMAChain* chain,
                            const TextureInfo* font,
                            const TextureInfo* logo,
                            void (*draw_panel)(DMAChain*, int, int, int, int, uint8_t, uint8_t, uint8_t, bool),
                            void (*draw_text)(DMAChain*, const TextureInfo*, int, int, const char*))
{
    int startX = HEADER_TEXT_X + (logo ? (logo->width + 20) : 32);
    int boxY = HEADER_TEXT_Y - 10;

    for (int i = 0; i < menu->category_count; ++i)
    {
        const XMBCategory* category = &menu->categories[i];
        bool active = (i == menu->current_category);
        int boxX = startX + (i * CATEGORY_SPACING);

        if (active)
        {
            draw_panel(chain, boxX - 18, boxY, CATEGORY_BOX_WIDTH, CATEGORY_BOX_HEIGHT, 26, 26, 52, true);
        }

        draw_text(chain, font, boxX, HEADER_TEXT_Y, category->name);
    }
}

static void draw_footer(const XMBCategory* category,
                        DMAChain* chain,
                        const TextureInfo* font,
                        uint32_t fileCount,
                        uint16_t pageSize,
                        void (*draw_text)(DMAChain*, const TextureInfo*, int, int, const char*))
{
    (void)fileCount;
    (void)pageSize;

    const char* line1 = "< / > Categories  UP/DOWN Move";
    const char* line2 = "SELECT Credits  CIRCLE Back";

    switch (category->type)
    {
        case XMB_CATEGORY_BROWSER:
            line1 = "< / > Categories  UP/DOWN Browse  L1/R1 Page";
            line2 = "X Quick Boot  START Full Boot  [] Parent  TRIANGLE Bootloader";
            break;

        case XMB_CATEGORY_ACTIONS:
            line1 = "< / > Categories  UP/DOWN Options";
            line2 = "X Apply  [] Parent  TRIANGLE Bootloader  SELECT Credits";
            break;

        case XMB_CATEGORY_INFO:
            line1 = "< / > Categories  UP/DOWN Scroll";
            line2 = "CIRCLE Back  SELECT Close";
            break;
    }

    draw_text(chain, font, HEADER_TEXT_X, FOOTER_PRIMARY_Y, line1);
    draw_text(chain, font, HEADER_TEXT_X, FOOTER_SECONDARY_Y, line2);
}

static void draw_game_browser(const XMBMenu* menu,
                              DMAChain* chain,
                              const TextureInfo* font,
                              uint32_t fileCount,
                              uint16_t pageSize,
                              uint8_t highlight,
                              void (*draw_panel)(DMAChain*, int, int, int, int, uint8_t, uint8_t, uint8_t, bool),
                              void (*draw_text)(DMAChain*, const TextureInfo*, int, int, const char*),
                              fileData* (*get_file)(uint16_t index))
{
    draw_panel(chain, LIST_PANEL_X, LIST_PANEL_Y, LIST_PANEL_WIDTH, LIST_PANEL_HEIGHT, 18, 18, 52, true);

    draw_text(chain, font, LIST_PANEL_X + 12, LIST_PANEL_Y + 8, "Game Library");

    char infoBuffer[32];
    snprintf(infoBuffer, sizeof(infoBuffer), "%u of %u", fileCount > 0 ? (unsigned)(xmb_get_current_item(menu) + 1) : 0, (unsigned)fileCount);
    draw_text(chain, font, LIST_PANEL_X + 12, LIST_PANEL_Y + 22, infoBuffer);

    if (fileCount == 0)
    {
        draw_text(chain, font, LIST_ENTRY_OFFSET_X, LIST_ENTRY_OFFSET_Y, "Empty Folder");
        return;
    }

    uint32_t selectedIndex = (uint32_t)xmb_get_current_item(menu);

    uint32_t start = 0;
    if (fileCount > pageSize)
    {
        uint32_t half = pageSize / 2;
        if (selectedIndex > half)
        {
            start = selectedIndex - half;
        }
        if (start + pageSize > fileCount)
        {
            start = fileCount - pageSize;
        }
    }

    uint32_t visible = fileCount - start;
    if (visible > pageSize)
    {
        visible = pageSize;
    }

    for (uint32_t i = 0; i < visible; ++i)
    {
        uint32_t index = start + i;
        int entryY = LIST_ENTRY_OFFSET_Y + (int)(i * LIST_ENTRY_HEIGHT);
        bool isSelected = (index == selectedIndex);

        if (isSelected)
        {
            uint8_t color = (uint8_t)(40 + (highlight & 0x1F));
            draw_panel(chain, LIST_PANEL_X + 4, entryY - 4, LIST_PANEL_WIDTH - 8, LIST_ENTRY_HEIGHT + 6, color, color + 18, color + 48, true);
        }

        fileData* file = get_file((uint16_t)index);
        if (!file)
        {
            continue;
        }

        char truncated[48];
        truncate_with_ellipsis(file->filename, truncated, sizeof(truncated), 28);
        if (file->flag == 1)
        {
            size_t len = strlen(truncated);
            if (len < sizeof(truncated) - 2)
            {
                truncated[len] = '/';
                truncated[len + 1] = '\0';
            }
        }

        char label[72];
        snprintf(label, sizeof(label), "%02u %c %s", (unsigned)(index + 1), file->flag ? '>' : ' ', truncated);
        draw_text(chain, font, LIST_ENTRY_OFFSET_X, entryY, label);
    }
}

static void draw_action_list(const XMBMenu* menu,
                             DMAChain* chain,
                             const TextureInfo* font,
                             uint8_t highlight,
                             void (*draw_panel)(DMAChain*, int, int, int, int, uint8_t, uint8_t, uint8_t, bool),
                             void (*draw_text)(DMAChain*, const TextureInfo*, int, int, const char*))
{
    const XMBCategory* category = &menu->categories[menu->current_category];

    draw_panel(chain, LIST_PANEL_X, LIST_PANEL_Y, LIST_PANEL_WIDTH, LIST_PANEL_HEIGHT / 2, 18, 18, 52, true);
    draw_text(chain, font, LIST_PANEL_X + 12, LIST_PANEL_Y + 8, "System Options");

    for (int i = 0; i < category->item_count; ++i)
    {
        int entryY = LIST_ENTRY_OFFSET_Y + (i * (LIST_ENTRY_HEIGHT + 2));
        bool isSelected = (i == xmb_get_current_item(menu));

        if (isSelected)
        {
            uint8_t color = (uint8_t)(34 + (highlight & 0x1F));
            draw_panel(chain, LIST_PANEL_X + 4, entryY - 4, LIST_PANEL_WIDTH - 8, LIST_ENTRY_HEIGHT + 6, color, color + 18, color + 40, true);
        }

        draw_text(chain, font, LIST_ENTRY_OFFSET_X, entryY, category->items[i].label);
    }
}

static void draw_info_panel(const XMBMenu* menu,
                            DMAChain* chain,
                            const TextureInfo* font,
                            uint8_t highlight,
                            void (*draw_panel)(DMAChain*, int, int, int, int, uint8_t, uint8_t, uint8_t, bool),
                            void (*draw_text)(DMAChain*, const TextureInfo*, int, int, const char*))
{
    const XMBCategory* category = &menu->categories[menu->current_category];

    draw_panel(chain, LIST_PANEL_X, LIST_PANEL_Y, LIST_PANEL_WIDTH, LIST_PANEL_HEIGHT, 14, 14, 36, true);
    draw_panel(chain, LIST_PANEL_X + 6, LIST_PANEL_Y + 6, LIST_PANEL_WIDTH - 12, LIST_PANEL_HEIGHT - 12, 6, 6, 20, true);

    draw_text(chain, font, LIST_PANEL_X + 18, LIST_PANEL_Y + 18, "Picostation Menu");

    for (int i = 0; i < category->item_count; ++i)
    {
        int entryY = LIST_ENTRY_OFFSET_Y + (i * (LIST_ENTRY_HEIGHT + 2));
        bool isSelected = (i == xmb_get_current_item(menu));

        if (isSelected)
        {
            uint8_t color = (uint8_t)(24 + (highlight & 0x1F));
            draw_panel(chain, LIST_PANEL_X + 10, entryY - 4, LIST_PANEL_WIDTH - 20, LIST_ENTRY_HEIGHT + 6, color, color + 16, color + 28, true);
        }

        draw_text(chain, font, LIST_ENTRY_OFFSET_X, entryY, category->items[i].label);
    }
}

void xmb_init(XMBMenu* menu)
{
    if (!menu)
    {
        return;
    }

    xmb_init(menu);
    menu->category_count = UI_CATEGORY_COUNT;
    menu->current_category = UI_CATEGORY_GAMES;

    for (int i = 0; i < UI_CATEGORY_COUNT; ++i)
    {
        menu->current_item[i] = 0;
    }

    XMBCategory* games = &menu->categories[UI_CATEGORY_GAMES];
    games->name = "Games";
    games->icon_path = NULL;
    games->type = XMB_CATEGORY_BROWSER;
    games->item_count = 0;

    XMBCategory* system = &menu->categories[UI_CATEGORY_SYSTEM];
    system->name = "System";
    system->icon_path = NULL;
    system->type = XMB_CATEGORY_ACTIONS;
    system->item_count = 0;
    system->items[system->item_count++] = (XMBItem){ XMB_ITEM_COMMAND, "Refresh Root", MENU_COMMAND_GOTO_ROOT };
    system->items[system->item_count++] = (XMBItem){ XMB_ITEM_COMMAND, "Open Parent Directory", MENU_COMMAND_GOTO_PARENT };
    system->items[system->item_count++] = (XMBItem){ XMB_ITEM_COMMAND, "Launch Bootloader", MENU_COMMAND_BOOTLOADER };

    XMBCategory* credits = &menu->categories[UI_CATEGORY_CREDITS];
    credits->name = "Credits";
    credits->icon_path = NULL;
    credits->type = XMB_CATEGORY_INFO;
    credits->item_count = 0;
    credits->items[credits->item_count++] = (XMBItem){ XMB_ITEM_TEXT, "Picostation Menu Alpha" };
    credits->items[credits->item_count++] = (XMBItem){ XMB_ITEM_TEXT, "Thanks to:" };
    credits->items[credits->item_count++] = (XMBItem){ XMB_ITEM_TEXT, "Rama, Skitchin, Raijin" };
    credits->items[credits->item_count++] = (XMBItem){ XMB_ITEM_TEXT, "SpicyJpeg, Danhans42" };
    credits->items[credits->item_count++] = (XMBItem){ XMB_ITEM_TEXT, "NicholasNoble, ChatGPT" };
    credits->items[credits->item_count++] = (XMBItem){ XMB_ITEM_TEXT, "github.com/megavolt85/picostation-menu" };
}

void xmb_draw(const XMBMenu* menu,
             DMAChain* chain,
             const TextureInfo* font,
             const TextureInfo* logo,
             uint32_t file_count,
             uint16_t page_size,
             uint8_t highlight,
             void (*draw_panel)(DMAChain*, int, int, int, int, uint8_t, uint8_t, uint8_t, bool),
             void (*draw_text)(DMAChain*, const TextureInfo*, int, int, const char*),
             fileData* (*get_file)(uint16_t index))
{
    if (!menu || menu->category_count == 0)
    {
        return;
    }

    draw_categories(menu, chain, font, logo, draw_panel, draw_text);

    const XMBCategory* category = &menu->categories[menu->current_category];

    switch (category->type)
    {
        case XMB_CATEGORY_BROWSER:
            draw_game_browser(menu, chain, font, file_count, page_size, highlight, draw_panel, draw_text, get_file);
            break;

        case XMB_CATEGORY_ACTIONS:
            draw_action_list(menu, chain, font, highlight, draw_panel, draw_text);
            break;

        case XMB_CATEGORY_INFO:
            draw_info_panel(menu, chain, font, highlight, draw_panel, draw_text);
            break;
    }

    draw_footer(category, chain, font, file_count, page_size, draw_text);
}
