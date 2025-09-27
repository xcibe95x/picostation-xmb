/*
 * ps1-bare-metal - (C) 2023 spicyjpeg
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * We saw how to load a single texture and display it in the last two examples.
 * Textures, however, are not always simple images displayed in their entirety:
 * sometimes they hold more than one image (e.g. all frames of a character's
 * animation in a 2D game) but are "cropped out" on the fly during rendering to
 * only draw a single frame at a time. These textures are known as spritesheets
 * and the PS1's GPU fully supports them, as it allows for arbitrary UV
 * coordinates to be used.
 *
 * This example is going to show how to implement a simple font system for text
 * rendering, since that's one of the most common use cases for spritesheets. We
 * are going to load a single texture containing all our font's characters, as
 * having hundreds of tiny textures for each character would be extremely
 * inefficient, and then use a lookup table to obtain the UV coordinates, width
 * and height of each character in a string.
 *
 * NOTE: in order to make the code easier to read, I have moved all the
 * GPU-related functions from previous examples to a separate source file.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "ps1/gpucmd.h"
#include "ps1/registers.h"
#include "ps1/cdrom.h"
#include "psxproject/cdrom.h"
#include "psxproject/filesystem.h"
#include "psxproject/irq.h"
#include "gpu.h"
#include "controller.h"
#include "psxproject/system.h"
#include "psxproject/spu.h"
#include <stdlib.h>
#include "file_manager.h"
#include "counters.h"
#include "logging.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define LISTING_SIZE 2324
#define MAX_FILES 4096

#if DEBUG_MAIN
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) while (0)
#endif

#define SFX_VOL	10922 // 2/3 of maximal volume

// In order to pick sprites (characters) out of our spritesheet, we need a table
// listing all of them (in ASCII order in this case) with their UV coordinates
// within the sheet as well as their dimensions. In this example we're going to
// hardcode the table, however in an actual game you may want to store this data
// in the same file as the image and palette data.
typedef struct
{
	uint8_t x, y, width, height;
} SpriteInfo;

static const SpriteInfo fontSprites[] = {
	{.x = 6, .y = 0, .width = 2, .height = 9},	   // !
	{.x = 12, .y = 0, .width = 4, .height = 9},	   // "
	{.x = 18, .y = 0, .width = 6, .height = 9},	   // #
	{.x = 24, .y = 0, .width = 6, .height = 9},	   // $
	{.x = 30, .y = 0, .width = 6, .height = 9},	   // %
	{.x = 36, .y = 0, .width = 6, .height = 9},	   // &
	{.x = 42, .y = 0, .width = 2, .height = 9},	   // '
	{.x = 48, .y = 0, .width = 3, .height = 9},	   // (
	{.x = 54, .y = 0, .width = 3, .height = 9},	   // )
	{.x = 60, .y = 0, .width = 4, .height = 9},	   // *
	{.x = 66, .y = 0, .width = 6, .height = 9},	   // +
	{.x = 72, .y = 0, .width = 3, .height = 9},	   // ,
	{.x = 78, .y = 0, .width = 6, .height = 9},	   // -
	{.x = 84, .y = 0, .width = 2, .height = 9},	   // .
	{.x = 90, .y = 0, .width = 6, .height = 9},	   // /
	{.x = 0, .y = 9, .width = 6, .height = 9},	   // 0
	{.x = 6, .y = 9, .width = 6, .height = 9},	   // 1
	{.x = 12, .y = 9, .width = 6, .height = 9},	   // 2
	{.x = 18, .y = 9, .width = 6, .height = 9},	   // 3
	{.x = 24, .y = 9, .width = 6, .height = 9},	   // 4
	{.x = 30, .y = 9, .width = 6, .height = 9},	   // 5
	{.x = 36, .y = 9, .width = 6, .height = 9},	   // 6
	{.x = 42, .y = 9, .width = 6, .height = 9},	   // 7
	{.x = 48, .y = 9, .width = 6, .height = 9},	   // 8
	{.x = 54, .y = 9, .width = 6, .height = 9},	   // 9
	{.x = 60, .y = 9, .width = 2, .height = 9},	   // :
	{.x = 66, .y = 9, .width = 3, .height = 9},	   // ;
	{.x = 72, .y = 9, .width = 6, .height = 9},	   // <
	{.x = 78, .y = 9, .width = 6, .height = 9},	   // =
	{.x = 84, .y = 9, .width = 6, .height = 9},	   // >
	{.x = 90, .y = 9, .width = 6, .height = 9},	   // ?
	{.x = 0, .y = 18, .width = 6, .height = 9},	   // @
	{.x = 6, .y = 18, .width = 6, .height = 9},	   // A
	{.x = 12, .y = 18, .width = 6, .height = 9},   // B
	{.x = 18, .y = 18, .width = 6, .height = 9},   // C
	{.x = 24, .y = 18, .width = 6, .height = 9},   // D
	{.x = 30, .y = 18, .width = 6, .height = 9},   // E
	{.x = 36, .y = 18, .width = 6, .height = 9},   // F
	{.x = 42, .y = 18, .width = 6, .height = 9},   // G
	{.x = 48, .y = 18, .width = 6, .height = 9},   // H
	{.x = 54, .y = 18, .width = 4, .height = 9},   // I
	{.x = 60, .y = 18, .width = 5, .height = 9},   // J
	{.x = 66, .y = 18, .width = 6, .height = 9},   // K
	{.x = 72, .y = 18, .width = 6, .height = 9},   // L
	{.x = 78, .y = 18, .width = 6, .height = 9},   // M
	{.x = 84, .y = 18, .width = 6, .height = 9},   // N
	{.x = 90, .y = 18, .width = 6, .height = 9},   // O
	{.x = 0, .y = 27, .width = 6, .height = 9},	   // P
	{.x = 6, .y = 27, .width = 6, .height = 9},	   // Q
	{.x = 12, .y = 27, .width = 6, .height = 9},   // R
	{.x = 18, .y = 27, .width = 6, .height = 9},   // S
	{.x = 24, .y = 27, .width = 6, .height = 9},   // T
	{.x = 30, .y = 27, .width = 6, .height = 9},   // U
	{.x = 36, .y = 27, .width = 6, .height = 9},   // V
	{.x = 42, .y = 27, .width = 6, .height = 9},   // W
	{.x = 48, .y = 27, .width = 6, .height = 9},   // X
	{.x = 54, .y = 27, .width = 6, .height = 9},   // Y
	{.x = 60, .y = 27, .width = 6, .height = 9},   // Z
	{.x = 66, .y = 27, .width = 3, .height = 9},   // [
	{.x = 72, .y = 27, .width = 6, .height = 9},   // Backslash
	{.x = 78, .y = 27, .width = 3, .height = 9},   // ]
	{.x = 84, .y = 27, .width = 4, .height = 9},   // ^
	{.x = 90, .y = 27, .width = 6, .height = 9},   // _
	{.x = 0, .y = 36, .width = 3, .height = 9},	   // `
	{.x = 6, .y = 36, .width = 6, .height = 9},	   // a
	{.x = 12, .y = 36, .width = 6, .height = 9},   // b
	{.x = 18, .y = 36, .width = 6, .height = 9},   // c
	{.x = 24, .y = 36, .width = 6, .height = 9},   // d
	{.x = 30, .y = 36, .width = 6, .height = 9},   // e
	{.x = 36, .y = 36, .width = 5, .height = 9},   // f
	{.x = 42, .y = 36, .width = 6, .height = 9},   // g
	{.x = 48, .y = 36, .width = 5, .height = 9},   // h
	{.x = 54, .y = 36, .width = 2, .height = 9},   // i
	{.x = 60, .y = 36, .width = 4, .height = 9},   // j
	{.x = 66, .y = 36, .width = 5, .height = 9},   // k
	{.x = 72, .y = 36, .width = 2, .height = 9},   // l
	{.x = 78, .y = 36, .width = 6, .height = 9},   // m
	{.x = 84, .y = 36, .width = 5, .height = 9},   // n
	{.x = 90, .y = 36, .width = 6, .height = 9},   // o
	{.x = 0, .y = 45, .width = 6, .height = 9},	   // p
	{.x = 6, .y = 45, .width = 6, .height = 9},	   // q
	{.x = 12, .y = 45, .width = 6, .height = 9},   // r
	{.x = 18, .y = 45, .width = 6, .height = 9},   // s
	{.x = 24, .y = 45, .width = 5, .height = 9},   // t
	{.x = 30, .y = 45, .width = 5, .height = 9},   // u
	{.x = 36, .y = 45, .width = 6, .height = 9},   // v
	{.x = 42, .y = 45, .width = 6, .height = 9},   // w
	{.x = 48, .y = 45, .width = 6, .height = 9},   // x
	{.x = 54, .y = 45, .width = 6, .height = 9},   // y
	{.x = 60, .y = 45, .width = 5, .height = 9},   // z
	{.x = 66, .y = 45, .width = 4, .height = 9},   // {
	{.x = 72, .y = 45, .width = 2, .height = 9},   // |
	{.x = 78, .y = 45, .width = 4, .height = 9},   // }
	{.x = 84, .y = 45, .width = 6, .height = 9},   // ~
	{.x = 90, .y = 45, .width = 6, .height = 9},   // Invalid character
	{.x = 0, .y = 54, .width = 6, .height = 9},	   //
	{.x = 6, .y = 54, .width = 6, .height = 9},	   //
	{.x = 12, .y = 54, .width = 4, .height = 9},   //
	{.x = 18, .y = 54, .width = 4, .height = 9},   //
	{.x = 24, .y = 54, .width = 6, .height = 9},   //
	{.x = 30, .y = 54, .width = 6, .height = 9},   //
	{.x = 36, .y = 54, .width = 6, .height = 9},   //
	{.x = 42, .y = 54, .width = 6, .height = 9},   //
	{.x = 0, .y = 63, .width = 7, .height = 9},	   //
	{.x = 12, .y = 63, .width = 7, .height = 9},   //
	{.x = 24, .y = 63, .width = 9, .height = 9},   //
	{.x = 36, .y = 63, .width = 8, .height = 10},  //
	{.x = 48, .y = 63, .width = 11, .height = 10}, //
	{.x = 60, .y = 63, .width = 12, .height = 10}, //
	{.x = 72, .y = 63, .width = 14, .height = 9},  //
	{.x = 0, .y = 73, .width = 10, .height = 10},  //
	{.x = 12, .y = 73, .width = 10, .height = 10}, //
	{.x = 24, .y = 73, .width = 10, .height = 10}, //
	{.x = 36, .y = 73, .width = 10, .height = 9},  //
	{.x = 48, .y = 73, .width = 10, .height = 9},  //
	{.x = 60, .y = 73, .width = 10, .height = 10},  //
	{.x = 72, .y = 73, .width = 10, .height = 10},  //
	{.x = 85, .y = 73, .width = 8, .height = 8}  //
};

typedef enum
{
	MENU_COMMAND_NONE = 0x0,
	MENU_COMMAND_GOTO_ROOT = 0x1,
	MENU_COMMAND_GOTO_PARENT = 0x2,
	MENU_COMMAND_GOTO_DIRECTORY = 0x3,
	MENU_COMMAND_MOUNT_FILE_FAST = 0x4,
	MENU_COMMAND_MOUNT_FILE_SLOW = 0x5,
	MENU_COMMAND_BOOTLOADER = 0x6
} MENU_COMMAND;

typedef enum
{
	COMMAND_GOTO_ROOT = 0x1,
	COMMAND_GOTO_PARENT = 0x2,
	COMMAND_GOTO_DIRECTORY = 0x3,
	COMMAND_GET_NEXT_CONTENTS = 0x4,
	COMMAND_MOUNT_FILE = 0x5,
	COMMAND_IO_COMMAND = 0x6,
	COMMAND_IO_DATA = 0x7,
	COMMAND_BOOTLOADER = 0xA
} COMMAND;

typedef enum
{
	IO_COMMAND_NONE = 0x0,
	IO_COMMAND_GAMEID = 0x1,
} IO_COMMAND;

#define FONT_FIRST_TABLE_CHAR '!'
#define FONT_SPACE_WIDTH 4
#define FONT_TAB_WIDTH 32
#define FONT_LINE_HEIGHT 10

static void sendCommand(uint8_t command, uint16_t argument)
{
	uint8_t test[] = {CDROM_TEST_DSP_CMD, (uint8_t)(0xF0 | command), (uint8_t)((argument >> 8) & 0xFF), (uint8_t)(argument & 0xFF)};
	issueCDROMCommand(CDROM_CMD_TEST, test, sizeof(test));
}

static void printString(
	DMAChain *chain, const TextureInfo *font, int x, int y, const char *str)
{
	int currentX = x, currentY = y;

	uint32_t *ptr;

	// Start by sending a texpage command to tell the GPU to use the font's
	// spritesheet. Note that the texpage command before a drawing command can
	// be omitted when reusing the same texture, so sending it here just once is
	// enough.
	ptr = allocatePacket(chain, 1);
	ptr[0] = gp0_texpage(font->page, false, false);

	// Iterate over every character in the string.
	for (; *str; str++)
	{
		uint8_t ch = (uint8_t)*str;

		// Check if the character is "special" and shall be handled without
		// drawing any sprite, or if it's invalid and should be rendered as a
		// box with a question mark (character code 127).
		switch (ch)
		{
		case '\t':
			currentX += FONT_TAB_WIDTH - 1;
			currentX -= currentX % FONT_TAB_WIDTH;
			continue;

		case '\n':
			currentX = x;
			currentY += FONT_LINE_HEIGHT;
			continue;

		case ' ':
			currentX += FONT_SPACE_WIDTH;
			continue;
		}
		if (ch >= 0x99 && ch <= 0xFF)
		{
			ch = '\x7f';
		}

		// If the character was not a tab, newline or space, fetch its
		// respective entry from the sprite coordinate table.
		const SpriteInfo *sprite = &fontSprites[ch - FONT_FIRST_TABLE_CHAR];

		// Draw the character, summing the UV coordinates of the spritesheet in
		// VRAM to those of the sprite itself within the sheet. Enable blending
		// to make sure any semitransparent pixels in the font get rendered
		// correctly.
		ptr = allocatePacket(chain, 4);
		ptr[0] = gp0_rectangle(true, true, true);
		ptr[1] = gp0_xy(currentX, currentY);
		ptr[2] = gp0_uv(font->u + sprite->x, font->v + sprite->y, font->clut);
		ptr[3] = gp0_xy(sprite->width, sprite->height);

		// Move onto the next character.
		currentX += sprite->width;
	}
}

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_WIDTH 96
#define FONT_HEIGHT 84
#define FONT_COLOR_DEPTH GP0_COLOR_4BPP

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 20
#define TEXTURE_COLOR_DEPTH GP0_COLOR_4BPP

#define BACKGROUND_TEX_X (SCREEN_WIDTH * 2)
#define BACKGROUND_TEX_Y 256
#define ICON_TEX_BASE_X (SCREEN_WIDTH * 2 + 320)
#define ICON_TEX_BASE_Y 0
#define ICON_SIZE 64
#define NAV_ICON_SCALE_FOCUSED 120
#define NAV_ICON_SCALE_IDLE 90
#define NAV_ICON_SCALE_DIM 80
#define NAV_Y 52
#define NAV_SPACING 96
#define GAME_LIST_VISIBLE 6
#define SETTINGS_VISIBLE 5
#define PANEL_OPACITY_BASE 18

typedef enum
{
	MENU_TAB_GAMES = 0,
	MENU_TAB_SETTINGS,
	MENU_TAB_FOLDERS,
	MENU_TAB_COUNT
} MenuTab;

typedef enum
{
	MENU_STATE_SPLASH = 0,
	MENU_STATE_MAIN
} MenuState;

typedef enum
{
	BOOT_MODE_REGULAR = 0,
	BOOT_MODE_FAST
} BootMode;

typedef struct
{
	const TextureInfo *texture;
	const char *label;
} NavItem;

enum
{
	SETTING_BOOT_MODE = 0,
	SETTING_RELOAD_LIST,
	SETTING_RESERVED,
};

typedef struct
{
	const char *label;
	const char *description;
} SettingDefinition;

extern const uint8_t fontTexture[], fontPalette[], logoTexture[], logoPalette[];
extern const uint8_t backgroundTexture[], backgroundPalette[];
extern const uint8_t iconDiscTexture[], iconDiscPalette[];
extern const uint8_t iconSettingsTexture[], iconSettingsPalette[];
extern const uint8_t iconFolderTexture[], iconFolderPalette[];
extern const uint8_t click_sfx[], slide_sfx[];

#define c_maxFilePathLength 255
#define c_maxFilePathLengthWithTerminator c_maxFilePathLength + 1
#define c_maxFileEntriesPerSector 8

int loadchecker = 0;

static MenuState menuState = MENU_STATE_SPLASH;
static MenuTab currentTab = MENU_TAB_GAMES;
static BootMode bootMode = BOOT_MODE_REGULAR;

static uint16_t selectedGame = 0;
static uint16_t selectedSetting = 0;
static uint16_t selectedFolder = 0;

static uint16_t gameIndices[MAX_FILES];
static uint16_t folderIndices[MAX_FILES];
static uint16_t gameCount = 0;
static uint16_t folderCount = 0;
static uint16_t directoryDepth = 0;
static bool libraryLoaded = false;
static uint16_t splashTimer = 0;
static bool resetSelectionOnLoad = true;
static uint8_t highlightTick = 0;

static NavItem navItems[MENU_TAB_COUNT];
static const SettingDefinition settingsDefinitions[] = {
	{ "Boot Mode", "Switch between regular and fast boot." },
	{ "Reload Game List", "Refresh the detected disc images." },
	{ "Reserved", "Additional options coming soon." }
};
#define SETTINGS_COUNT (sizeof(settingsDefinitions) / sizeof(settingsDefinitions[0]))

void wait_ms(uint32_t ms)
{
	uint32_t frequency = (GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL ? 50 : 60;
	uint32_t frames = (ms * frequency) / 1000;
	for (uint32_t i = 0; i < frames; ++i)
	{
		waitForVblank();
	}
}

static void formatDisplayName(const fileData *file, char *output, size_t maxLen)
{
    const char *name = file ? file->filename : "";
    size_t out = 0;
    for (size_t i = 0; name[i] != '\0' && out + 1 < maxLen; ++i)
    {
        char c = name[i];
        if (c == ';' || c == '.')
        {
            break;
        }
        if (c == '_' || c == '-')
        {
            c = ' ';
        }
        output[out++] = c;
    }
    if (out == 0)
    {
        for (size_t i = 0; name[i] != '\0' && out + 1 < maxLen; ++i)
        {
            char c = name[i];
            if (c == ';')
            {
                break;
            }
            output[out++] = c;
        }
    }
    output[out] = '\0';
}

static void rebuildFileLists(uint16_t total, bool resetSelection)
{
    gameCount = 0;
    folderCount = 0;
    for (uint16_t i = 0; i < total && i < MAX_FILES; ++i)
    {
        fileData *entry = file_manager_get_file_data(i);
        if (!entry)
        {
            continue;
        }
        if (entry->flag == 0)
        {
            gameIndices[gameCount++] = i;
        }
        else
        {
            folderIndices[folderCount++] = i;
        }
    }

    if (resetSelection || gameCount == 0 || selectedGame >= gameCount)
    {
        selectedGame = (gameCount > 0) ? 0 : 0;
    }

    uint16_t folderSelectable = folderCount + (directoryDepth > 0 ? 1 : 0);
    if (resetSelection || folderSelectable == 0 || selectedFolder >= folderSelectable)
    {
        selectedFolder = 0;
    }

    if (SETTINGS_COUNT > 0 && selectedSetting >= SETTINGS_COUNT)
    {
        selectedSetting = SETTINGS_COUNT - 1;
    }

}

static void drawPanel(
    DMAChain *chain,
    int x,
    int y,
    int width,
    int height,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    bool blend
)
{
    uint32_t *ptr = allocatePacket(chain, 3);
    ptr[0] = gp0_rgb(r, g, b) | gp0_rectangle(false, false, blend);
    ptr[1] = gp0_xy(x, y);
    ptr[2] = gp0_xy(width, height);
}

static void drawTexturedQuad(DMAChain *chain, const TextureInfo *texture, int x, int y, int width, int height)
{
    uint32_t *packet = allocatePacket(chain, 5);
    packet[0] = gp0_texpage(texture->page, false, false);
    packet[1] = gp0_rectangle(true, true, true);
    packet[2] = gp0_xy(x, y);
    packet[3] = gp0_uv(texture->u, texture->v, texture->clut);
    packet[4] = gp0_xy(width, height);
}

static void drawBackgroundTexture(DMAChain *chain, const TextureInfo *texture)
{
    drawTexturedQuad(chain, texture, 0, 0, texture->width, texture->height);
}

static void drawNavIcon(DMAChain *chain, const TextureInfo *texture, int centerX, int centerY, int scalePercent, bool highlight)
{
    int width = (texture->width * scalePercent) / 100;
    int height = (texture->height * scalePercent) / 100;
    if (width < 8)
    {
        width = 8;
    }
    if (height < 8)
    {
        height = 8;
    }
    int drawX = centerX - (width / 2);
    int drawY = centerY - (height / 2);

    if (highlight)
    {
        drawPanel(chain, drawX - 6, drawY - 6, width + 12, height + 12, 28, 28, 72, true);
    }

    drawTexturedQuad(chain, texture, drawX, drawY, width, height);
}

static void drawTopNavigation(DMAChain *chain, const TextureInfo *font, MenuTab activeTab)
{
    const char *labels[MENU_TAB_COUNT] = { "Games", "Settings", "Folders" };
    int totalSpan = (MENU_TAB_COUNT - 1) * NAV_SPACING;
    int firstX = (SCREEN_WIDTH / 2) - (totalSpan / 2);

    for (int tab = 0; tab < MENU_TAB_COUNT; ++tab)
    {
        const NavItem *item = &navItems[tab];
        if (!item->texture)
        {
            continue;
        }
        int centerX = firstX + (tab * NAV_SPACING);
        int scale = (tab == activeTab) ? NAV_ICON_SCALE_FOCUSED : NAV_ICON_SCALE_IDLE;
        drawNavIcon(chain, item->texture, centerX, NAV_Y, scale, tab == activeTab);

        char label[32];
        strncpy(label, item->label ? item->label : labels[tab], sizeof(label));
        label[sizeof(label) - 1] = '\0';
        int textWidth = (int)strlen(label) * 6;
        int textX = centerX - (textWidth / 2);
        printString(chain, font, textX, NAV_Y + 40, label);
    }
}

static void drawGameLibraryPane(
    DMAChain *chain,
    const TextureInfo *font,
    const TextureInfo *discIcon
)
{
    const int panelX = 32;
    const int panelY = 96;
    const int panelWidth = SCREEN_WIDTH - (panelX * 2);
    const int panelHeight = 120;

    drawPanel(chain, panelX, panelY, panelWidth, panelHeight, 18, 18, 52, true);
    drawPanel(chain, panelX + 2, panelY + 2, panelWidth - 4, panelHeight - 4, 10, 10, 30, true);

    if (gameCount == 0)
    {
        printString(chain, font, panelX + 16, panelY + 48, "No titles found. Use Folders tab.");
    }
    else
    {
        int visible = gameCount < GAME_LIST_VISIBLE ? (int)gameCount : GAME_LIST_VISIBLE;
        int start = 0;
        if (gameCount > GAME_LIST_VISIBLE)
        {
            int half = GAME_LIST_VISIBLE / 2;
            int target = (int)selectedGame - half;
            if (target < 0)
            {
                target = 0;
            }
            int maxStart = (int)gameCount - GAME_LIST_VISIBLE;
            if (target > maxStart)
            {
                target = maxStart;
            }
            start = target;
        }

        const int rowHeight = 18;
        for (int i = 0; i < visible; ++i)
        {
            int index = start + i;
            bool isSelected = (index == selectedGame);
            int itemY = panelY + 12 + (i * rowHeight);

            if (isSelected)
            {
                drawPanel(chain, panelX + 6, itemY - 2, panelWidth - 12, rowHeight + 4, 42, 42, 88, true);
            }

            drawNavIcon(chain, discIcon, panelX + 24, itemY + 8, isSelected ? 90 : 75, false);

            const fileData *file = file_manager_get_file_data(gameIndices[index]);
            char nameBuffer[64];
            formatDisplayName(file, nameBuffer, sizeof(nameBuffer));
            printString(chain, font, panelX + 48, itemY + 4, nameBuffer);
        }

        char counter[32];
        snprintf(counter, sizeof(counter), "%u of %u", (unsigned)(selectedGame + 1), (unsigned)gameCount);
        printString(chain, font, panelX + panelWidth - 120, panelY - 14, counter);
    }
}

static void drawSettingsPane(
    DMAChain *chain,
    const TextureInfo *font
)
{
    const int panelX = 32;
    const int panelY = 96;
    const int panelWidth = SCREEN_WIDTH - (panelX * 2);
    const int panelHeight = 120;

    drawPanel(chain, panelX, panelY, panelWidth, panelHeight, 18, 18, 52, true);
    drawPanel(chain, panelX + 2, panelY + 2, panelWidth - 4, panelHeight - 4, 10, 10, 30, true);

    int visible = SETTINGS_COUNT < SETTINGS_VISIBLE ? SETTINGS_COUNT : SETTINGS_VISIBLE;
    int start = 0;
    if (SETTINGS_COUNT > SETTINGS_VISIBLE)
    {
        int half = SETTINGS_VISIBLE / 2;
        int target = (int)selectedSetting - half;
        if (target < 0)
        {
            target = 0;
        }
        int maxStart = SETTINGS_COUNT - SETTINGS_VISIBLE;
        if (target > maxStart)
        {
            target = maxStart;
        }
        start = target;
    }

    const int rowHeight = 18;
    for (int i = 0; i < visible; ++i)
    {
        int index = start + i;
        bool isSelected = (index == selectedSetting);
        int itemY = panelY + 12 + (i * rowHeight);

        if (isSelected)
        {
            drawPanel(chain, panelX + 6, itemY - 2, panelWidth - 12, rowHeight + 4, 42, 42, 88, true);
        }

        const SettingDefinition *setting = &settingsDefinitions[index];
        printString(chain, font, panelX + 12, itemY + 4, setting->label);

        if (index == SETTING_BOOT_MODE)
        {
            printString(chain, font, panelX + panelWidth - 120, itemY + 4, (bootMode == BOOT_MODE_FAST) ? "Fast" : "Regular");
        }
    }

    if (SETTINGS_COUNT > 0)
    {
        const SettingDefinition *active = &settingsDefinitions[selectedSetting];
        printString(chain, font, panelX + 12, panelY + panelHeight - 20, active->description);
    }
}

static void drawFoldersPane(
    DMAChain *chain,
    const TextureInfo *font,
    const TextureInfo *folderIcon
)
{
    const int panelX = 32;
    const int panelY = 96;
    const int panelWidth = SCREEN_WIDTH - (panelX * 2);
    const int panelHeight = 120;

    drawPanel(chain, panelX, panelY, panelWidth, panelHeight, 18, 18, 52, true);
    drawPanel(chain, panelX + 2, panelY + 2, panelWidth - 4, panelHeight - 4, 10, 10, 30, true);

    uint16_t parentOffset = (directoryDepth > 0) ? 1 : 0;
    uint16_t totalEntries = folderCount + parentOffset;

    if (totalEntries == 0)
    {
        printString(chain, font, panelX + 16, panelY + 48, "No folders available.");
        return;
    }

    int visible = totalEntries < GAME_LIST_VISIBLE ? totalEntries : GAME_LIST_VISIBLE;
    int start = 0;
    if (totalEntries > GAME_LIST_VISIBLE)
    {
        int half = GAME_LIST_VISIBLE / 2;
        int target = (int)selectedFolder - half;
        if (target < 0)
        {
            target = 0;
        }
        int maxStart = totalEntries - GAME_LIST_VISIBLE;
        if (target > maxStart)
        {
            target = maxStart;
        }
        start = target;
    }

    const int rowHeight = 18;
    for (int i = 0; i < visible; ++i)
    {
        int index = start + i;
        bool isSelected = (index == selectedFolder);
        int itemY = panelY + 12 + (i * rowHeight);

        if (isSelected)
        {
            drawPanel(chain, panelX + 6, itemY - 2, panelWidth - 12, rowHeight + 4, 42, 42, 88, true);
        }

        if (directoryDepth > 0 && index == 0)
        {
            printString(chain, font, panelX + 48, itemY + 4, "Parent Directory");
        }
        else
        {
            uint16_t fileIndex = folderIndices[index - parentOffset];
            const fileData *entry = file_manager_get_file_data(fileIndex);
            char nameBuffer[64];
            formatDisplayName(entry, nameBuffer, sizeof(nameBuffer));
            drawNavIcon(chain, folderIcon, panelX + 24, itemY + 8, isSelected ? 90 : 75, false);
            printString(chain, font, panelX + 48, itemY + 4, nameBuffer);
        }
    }
}

static void drawSplashScreen(
    DMAChain *chain,
    const TextureInfo *font,
    const TextureInfo *logo,
    bool showEmptyMessage
)
{
    int logoX = SCREEN_WIDTH - logo->width - 32;
    int logoY = 40;
    drawTexturedQuad(chain, logo, logoX, logoY, logo->width, logo->height);

    printString(chain, font, 32, SCREEN_HEIGHT - 48, showEmptyMessage ? "No titles detected." : "Loading...");
    printString(chain, font, 32, SCREEN_HEIGHT - 32, "Please wait while the library is prepared.");
}


bool doLookup(uint16_t *itemCount, char *sectorBuffer)
{
	uint16_t offset = 0;
	while (offset < LISTING_SIZE && *itemCount < MAX_FILES)
	{
		uint16_t length = ((uint8_t *)sectorBuffer)[offset];
		if (length == 0)
		{
			return sectorBuffer[offset + 1] == 1 || (sectorBuffer[offset + 2] == 0 && sectorBuffer[offset + 3] == 0);
		}
		file_manager_init_file_data(*itemCount, sectorBuffer[offset + 1], &sectorBuffer[offset + 2], length);
		offset += length + 2;
		*itemCount = *itemCount + 1;
	}

	return false;
}

uint32_t list_load(void *sectorBuffer, uint8_t command, uint16_t argument)
{
	uint16_t fileEntryCount = 0;

	bool hasNext = true;
	while (hasNext)
	{
		sendCommand(command, argument);
		startCDROMRead(
			100,
			sectorBuffer,
			1,
			2340,
			true,
			true);

		hasNext = doLookup(&fileEntryCount, ((char *)sectorBuffer) + 12);
		command = COMMAND_GET_NEXT_CONTENTS;
		argument = fileEntryCount;
	}

	file_manager_sort(fileEntryCount);
	file_manager_clean_list(&fileEntryCount);
	return fileEntryCount;
}

int main(int argc, const char **argv)
{
    static uint8_t MCPpresent;
    COUNTERS[1].mode = 0x0100;

    initIRQ();
#if DEBUG_LOGGING_ENABLED
    initSerialIO(115200);
#endif
    initControllerBus();
    initCDROM();
    initSPU();

    MCPpresent = checkMCPpresent();

    static Sound sfx_click;
    static Sound sfx_slide;
    sound_loadSoundFromBinary(click_sfx, &sfx_click);
    sound_loadSoundFromBinary(slide_sfx, &sfx_slide);

    file_manager_init();

    if ((GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL)
    {
        setupGPU(GP1_MODE_PAL, SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    else
    {
        setupGPU(GP1_MODE_NTSC, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    DMA_DPCR |= DMA_DPCR_CH_ENABLE(DMA_GPU);
    GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);
    GPU_GP1 = gp1_dispBlank(false);

    TextureInfo font;
    TextureInfo logo;
    TextureInfo background;
    TextureInfo iconDisc;
    TextureInfo iconSettings;
    TextureInfo iconFolder;

    uploadIndexedTexture(
        &font,
        fontTexture,
        fontPalette,
        SCREEN_WIDTH * 2,
        0,
        SCREEN_WIDTH * 2,
        FONT_HEIGHT,
        FONT_WIDTH,
        FONT_HEIGHT,
        FONT_COLOR_DEPTH
    );

    uploadIndexedTexture(
        &logo,
        logoTexture,
        logoPalette,
        SCREEN_WIDTH * 2,
        FONT_WIDTH,
        SCREEN_WIDTH * 2,
        FONT_WIDTH + TEXTURE_HEIGHT,
        TEXTURE_WIDTH,
        TEXTURE_HEIGHT,
        TEXTURE_COLOR_DEPTH
    );

    uploadIndexedTexture(
        &background,
        backgroundTexture,
        backgroundPalette,
        BACKGROUND_TEX_X,
        BACKGROUND_TEX_Y,
        BACKGROUND_TEX_X,
        BACKGROUND_TEX_Y,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        GP0_COLOR_4BPP
    );

    uploadIndexedTexture(
        &iconDisc,
        iconDiscTexture,
        iconDiscPalette,
        ICON_TEX_BASE_X,
        ICON_TEX_BASE_Y,
        ICON_TEX_BASE_X,
        ICON_TEX_BASE_Y,
        ICON_SIZE,
        ICON_SIZE,
        GP0_COLOR_4BPP
    );

    uploadIndexedTexture(
        &iconSettings,
        iconSettingsTexture,
        iconSettingsPalette,
        ICON_TEX_BASE_X,
        ICON_TEX_BASE_Y + ICON_SIZE,
        ICON_TEX_BASE_X,
        ICON_TEX_BASE_Y + ICON_SIZE,
        ICON_SIZE,
        ICON_SIZE,
        GP0_COLOR_4BPP
    );

    uploadIndexedTexture(
        &iconFolder,
        iconFolderTexture,
        iconFolderPalette,
        ICON_TEX_BASE_X,
        ICON_TEX_BASE_Y + (ICON_SIZE * 2),
        ICON_TEX_BASE_X,
        ICON_TEX_BASE_Y + (ICON_SIZE * 2),
        ICON_SIZE,
        ICON_SIZE,
        GP0_COLOR_4BPP
    );

    navItems[MENU_TAB_GAMES] = (NavItem){ &iconDisc, "Games" };
    navItems[MENU_TAB_SETTINGS] = (NavItem){ &iconSettings, "Settings" };
    navItems[MENU_TAB_FOLDERS] = (NavItem){ &iconFolder, "Folders" };

    DMAChain dmaChains[2];
    bool usingSecondFrame = false;
    char sectorBuffer[LISTING_SIZE];

    uint32_t fileEntryCount = 0;
    uint16_t selectedindex = 0;
    uint16_t previousButtons = getButtonPress(0);
    uint8_t currentCommand = MENU_COMMAND_GOTO_ROOT;

    menuState = MENU_STATE_SPLASH;
    libraryLoaded = false;
    splashTimer = 0;
    resetSelectionOnLoad = true;
    directoryDepth = 0;

    for (;;)
    {
        int bufferX = usingSecondFrame ? SCREEN_WIDTH : 0;
        int bufferY = 0;
        DMAChain *chain = &dmaChains[usingSecondFrame];
        usingSecondFrame = !usingSecondFrame;

        GPU_GP1 = gp1_fbOffset(bufferX, bufferY);
        chain->nextPacket = chain->data;

        uint32_t *ptr = allocatePacket(chain, 4);
        ptr[0] = gp0_texpage(0, true, false);
        ptr[1] = gp0_fbOffset1(bufferX, bufferY);
        ptr[2] = gp0_fbOffset2(bufferX + SCREEN_WIDTH - 1, bufferY + SCREEN_HEIGHT - 1);
        ptr[3] = gp0_fbOrigin(bufferX, bufferY);

        highlightTick = (highlightTick + 1) & 0x3F;

        uint16_t buttons = getButtonPress(0);
        uint16_t pressedButtons = (~previousButtons) & buttons;
        static uint8_t hold = 0;

        if ((buttons & BUTTON_MASK_UP) && (previousButtons & BUTTON_MASK_UP))
        {
            if (++hold > 24)
            {
                pressedButtons |= BUTTON_MASK_UP;
                hold = 18;
            }
        }
        else if ((buttons & BUTTON_MASK_DOWN) && (previousButtons & BUTTON_MASK_DOWN))
        {
            if (++hold > 24)
            {
                pressedButtons |= BUTTON_MASK_DOWN;
                hold = 18;
            }
        }
        else
        {
            hold = 0;
        }

        bool allowInput = (menuState == MENU_STATE_MAIN) && (currentCommand == MENU_COMMAND_NONE);

        if (menuState == MENU_STATE_SPLASH)
        {
            if (libraryLoaded)
            {
                if (splashTimer < 90)
                {
                    ++splashTimer;
                }
                else
                {
                    menuState = MENU_STATE_MAIN;
                }
            }
        }
        else if (allowInput)
        {
            if (pressedButtons & BUTTON_MASK_LEFT)
            {
                currentTab = (MenuTab)((currentTab + MENU_TAB_COUNT - 1) % MENU_TAB_COUNT);
                sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
            }
            else if (pressedButtons & BUTTON_MASK_RIGHT)
            {
                currentTab = (MenuTab)((currentTab + 1) % MENU_TAB_COUNT);
                sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
            }

            switch (currentTab)
            {
                case MENU_TAB_GAMES:
                    if (gameCount > 0)
                    {
                        if (pressedButtons & BUTTON_MASK_UP)
                        {
                            selectedGame = (selectedGame > 0) ? selectedGame - 1 : gameCount - 1;
                            sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
                        }
                        else if (pressedButtons & BUTTON_MASK_DOWN)
                        {
                            selectedGame = (selectedGame + 1 < gameCount) ? selectedGame + 1 : 0;
                            sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
                        }

                        if ((pressedButtons & BUTTON_MASK_X) || (pressedButtons & BUTTON_MASK_START))
                        {
                            uint16_t listIndex = gameIndices[selectedGame];
                            selectedindex = listIndex;
                            currentCommand = (pressedButtons & BUTTON_MASK_START) ? MENU_COMMAND_MOUNT_FILE_SLOW : (bootMode == BOOT_MODE_FAST ? MENU_COMMAND_MOUNT_FILE_FAST : MENU_COMMAND_MOUNT_FILE_SLOW);
                            menuState = MENU_STATE_SPLASH;
                            libraryLoaded = false;
                            splashTimer = 0;
                            sound_playOnChannel(&sfx_slide, SFX_VOL, SFX_VOL, 1);
                        }
                    }
                    else if (pressedButtons & BUTTON_MASK_DOWN)
                    {
                        sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
                    }
                    break;

                case MENU_TAB_SETTINGS:
                {
                    if (SETTINGS_COUNT > 0)
                    {
                        if (pressedButtons & BUTTON_MASK_UP)
                        {
                            selectedSetting = (selectedSetting > 0) ? (selectedSetting - 1) : (SETTINGS_COUNT - 1);
                            sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
                        }
                        else if (pressedButtons & BUTTON_MASK_DOWN)
                        {
                            selectedSetting = (selectedSetting + 1 < SETTINGS_COUNT) ? (selectedSetting + 1) : 0;
                            sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
                        }

                        if (pressedButtons & BUTTON_MASK_X)
                        {
                            sound_playOnChannel(&sfx_slide, SFX_VOL, SFX_VOL, 1);
                            if (selectedSetting == SETTING_BOOT_MODE)
                            {
                                bootMode = (bootMode == BOOT_MODE_REGULAR) ? BOOT_MODE_FAST : BOOT_MODE_REGULAR;
                            }
                            else if (selectedSetting == SETTING_RELOAD_LIST)
                            {
                                currentCommand = MENU_COMMAND_GOTO_ROOT;
                                menuState = MENU_STATE_SPLASH;
                                libraryLoaded = false;
                                splashTimer = 0;
                                resetSelectionOnLoad = true;
                                directoryDepth = 0;
                            }
                        }
                    }
                    break;
                }

                case MENU_TAB_FOLDERS:
                {
                    uint16_t extraParent = (directoryDepth > 0) ? 1 : 0;
                    uint16_t totalEntries = folderCount + extraParent;
                    if (totalEntries > 0)
                    {
                        if (pressedButtons & BUTTON_MASK_UP)
                        {
                            selectedFolder = (selectedFolder > 0) ? selectedFolder - 1 : (totalEntries - 1);
                            sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
                        }
                        else if (pressedButtons & BUTTON_MASK_DOWN)
                        {
                            selectedFolder = (selectedFolder + 1 < totalEntries) ? selectedFolder + 1 : 0;
                            sound_playOnChannel(&sfx_click, SFX_VOL, SFX_VOL, 0);
                        }

                        if (pressedButtons & BUTTON_MASK_X)
                        {
                            sound_playOnChannel(&sfx_slide, SFX_VOL, SFX_VOL, 1);
                            if (directoryDepth > 0 && selectedFolder == 0)
                            {
                                currentCommand = MENU_COMMAND_GOTO_PARENT;
                                menuState = MENU_STATE_SPLASH;
                                libraryLoaded = false;
                                splashTimer = 0;
                                if (directoryDepth > 0)
                                {
                                    directoryDepth--;
                                }
                                resetSelectionOnLoad = true;
                            }
                            else
                            {
                                uint16_t entryIndex = folderIndices[selectedFolder - extraParent];
                                selectedindex = entryIndex;
                                currentCommand = MENU_COMMAND_GOTO_DIRECTORY;
                                menuState = MENU_STATE_SPLASH;
                                libraryLoaded = false;
                                splashTimer = 0;
                                directoryDepth++;
                                resetSelectionOnLoad = true;
                            }
                        }
                    }
                    break;
                }
            }
        }

        drawBackgroundTexture(chain, &background);

        if (menuState == MENU_STATE_SPLASH)
        {
            bool showEmptyMessage = libraryLoaded && (gameCount == 0);
            drawSplashScreen(chain, &font, &logo, showEmptyMessage);
        }
        else
        {
            drawTopNavigation(chain, &font, currentTab);
            switch (currentTab)
            {
                case MENU_TAB_GAMES:
                    drawGameLibraryPane(chain, &font, &iconDisc);
                    break;
                case MENU_TAB_SETTINGS:
                    drawSettingsPane(chain, &font);
                    break;
                case MENU_TAB_FOLDERS:
                    drawFoldersPane(chain, &font, &iconFolder);
                    break;
                case MENU_TAB_COUNT:
                default:
                    break;
            }
        }

        previousButtons = buttons;

        *(chain->nextPacket) = gp0_endTag(0);
        waitForGP0Ready();
        waitForVblank();
        sendLinkedList(chain->data);

        if (currentCommand != MENU_COMMAND_NONE)
        {
            if (currentCommand == MENU_COMMAND_GOTO_ROOT)
            {
                fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_ROOT, 0);
                resetSelectionOnLoad = true;
                directoryDepth = 0;
            }
            else if (currentCommand == MENU_COMMAND_GOTO_PARENT)
            {
                fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_PARENT, 0);
                resetSelectionOnLoad = true;
            }
            else if (currentCommand == MENU_COMMAND_GOTO_DIRECTORY)
            {
                uint16_t index = file_manager_get_file_index(selectedindex);
                fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_DIRECTORY, index);
                resetSelectionOnLoad = true;
            }
            else if (currentCommand == MENU_COMMAND_BOOTLOADER)
            {
                // Placeholder for bootloader entry
            }
            else if ((currentCommand == MENU_COMMAND_MOUNT_FILE_FAST) || (currentCommand == MENU_COMMAND_MOUNT_FILE_SLOW))
            {
                uint16_t index = file_manager_get_file_index(selectedindex);
                sendCommand(COMMAND_MOUNT_FILE, index);
                delayMicroseconds(400000);
                updateCDROM_TOC();
                delayMicroseconds(400000);
                if (is_playstation_cd())
                {
                    if (MCPpresent && !initFilesystem())
                    {
                        char gameId[2048];
                        strcpy(gameId, "cdrom:\\PS.EXE;1");
                        char configBuffer[2048];
                        if (file_load("SYSTEM.CNF;1", configBuffer) > 0)
                        {
                            int i = 0;
                            int j = 0;
                            char tempBuffer[500];
                            memset(tempBuffer, 0, sizeof(tempBuffer));
                            while (configBuffer[i] != '\0' && configBuffer[i] != '\n' && i < 499)
                            {
                                if (configBuffer[i] != ' ' && configBuffer[i] != '\t')
                                {
                                    tempBuffer[j++] = configBuffer[i];
                                }
                                i++;
                            }
                            const char *bootStr = tempBuffer;
                            if (strncmp(tempBuffer, "BOOT=", 5) == 0)
                            {
                                bootStr = tempBuffer + 5;
                            }
                            sendGameID(bootStr, MCPpresent);
                        }
                    }
                }
                if (currentCommand == MENU_COMMAND_MOUNT_FILE_FAST)
                {
                    softFastReboot();
                }
                else
                {
                    softReset();
                }
            }

            if (currentCommand == MENU_COMMAND_GOTO_ROOT || currentCommand == MENU_COMMAND_GOTO_PARENT || currentCommand == MENU_COMMAND_GOTO_DIRECTORY)
            {
                rebuildFileLists((uint16_t)fileEntryCount, resetSelectionOnLoad);
                libraryLoaded = true;
                splashTimer = 0;
                resetSelectionOnLoad = false;
            }

            currentCommand = MENU_COMMAND_NONE;
        }
    }

    return 0;
}

