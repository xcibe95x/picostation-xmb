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
#include <stdlib.h>
#include "file_manager.h"

#define LISTING_SIZE 2324
#define MAX_FILES 4096

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
	{.x = 60, .y = 73, .width = 10, .height = 10}  //
};

#define COMMAND_NONE 0x0
#define COMMAND_GOTO_ROOT 0x1
#define COMMAND_GOTO_PARENT 0x2
#define COMMAND_GOTO_DIRECTORY 0x3
#define COMMAND_GET_NEXT_CONTENTS 0x4
#define COMMAND_MOUNT_FILE 0x5
#define COMMAND_BOOTLOADER 0xa

#define FONT_FIRST_TABLE_CHAR '!'
#define FONT_SPACE_WIDTH 4
#define FONT_TAB_WIDTH 32
#define FONT_LINE_HEIGHT 10

static void sendCommand(uint8_t command, uint16_t argument)
{
	//printf("DEBUG:X sending command %i with arg  %i\n", command, argument);
	uint8_t test[] = { CDROM_TEST_DSP_CMD, (uint8_t)(0xF0 | command), (uint8_t)((argument >> 8) & 0xFF), (uint8_t)(argument & 0xFF) };
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

extern const uint8_t fontTexture[], fontPalette[], piTexture[];

#define c_maxFilePathLength 255
#define c_maxFilePathLengthWithTerminator c_maxFilePathLength + 1
#define c_maxFileEntriesPerSector 8

int loadchecker = 0;

void wait_ms(uint32_t ms)
{
	uint32_t frequency = (GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL ? 50 : 60;
	uint32_t frames = (ms * frequency) / 1000;
	for (uint32_t i = 0; i < frames; ++i)
	{
		waitForVblank();
	}
}

bool doLookup(uint16_t* itemCount, char *sectorBuffer)
{
	uint16_t offset = 0;
	while (offset < LISTING_SIZE && *itemCount < MAX_FILES)
	{
		uint16_t length = ((uint8_t *)sectorBuffer)[offset];
		if (length == 0)
		{
			return sectorBuffer[offset + 1] == 1;
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

		// printf("hex log\n");
		// // printf("buffer %s\n",sectorBuffer);
		// for (int i = 0; i < 2340; i++)
		// {
		// 	char c = ((unsigned char *)sectorBuffer)[i];
		// 	printf("%02X ", c);
		// 	if ((i + 1) % 16 == 0)
		// 		printf("\n"); // optional: newline every 16 bytes
		// }

		hasNext = doLookup(&fileEntryCount, ((char*)sectorBuffer) + 12);
		command = COMMAND_GET_NEXT_CONTENTS;
	}

	file_manager_sort(fileEntryCount);
	printf("fileEntryCount %i\n", fileEntryCount);
	return fileEntryCount;
}

// offset + (upper 4 bits = dir or not) 2x4096=8192
// 0
// 20;
// 25...
// end = 0xffff

// strings



// when requesting a sector using size 2340, and since data is expected to be 2324 bytes long, is it correct from what i see it has 12 byte header and 4 byte footer

uint8_t test[] = {0x50, 0xfa, 0xf0, 0xf1};
int main(int argc, const char **argv)
{

	initIRQ();
	initSerialIO(115200);
	initControllerBus();
	// initFilesystem();
	initCDROM();

	file_manager_init();

	uint8_t currentCommand = COMMAND_NONE;

	printf("Hello from menu loader!\n");
	printf("fileData size %i", sizeof(fileData));

	if ((GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL)
	{
		puts("Using PAL mode");
		setupGPU(GP1_MODE_PAL, SCREEN_WIDTH, SCREEN_HEIGHT);
	}
	else
	{
		puts("Using NTSC mode");
		setupGPU(GP1_MODE_NTSC, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	DMA_DPCR |= DMA_DPCR_ENABLE << (DMA_GPU * 4);

	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);
	GPU_GP1 = gp1_dispBlank(false);

	TextureInfo font;

	uploadIndexedTexture(
		&font, fontTexture, fontPalette, SCREEN_WIDTH * 2, 0, SCREEN_WIDTH * 2,
		FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT, FONT_COLOR_DEPTH);

	DMAChain dmaChains[2];
	bool usingSecondFrame = false;

	// dummy list
	// char text[2048] = "Game\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\n";

	char sectorBuffer[2324];

	// file_load("SYSTEM.CNF;1", txtBuffer2);

	// printf("format s %s\n", txtBuffer2);
	///

	uint32_t fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_ROOT, 0);

	// printf("disk buffer %s\n", txtBuffer);
	uint16_t selectedindex = 1;

	int startnumber = 0;

	// uint16_t sectorBuffer[1024];

	int creditsmenu = 0;

	uint16_t previousButtons = getButtonPress(0);

	for (;;)
	{
		int bufferX = usingSecondFrame ? SCREEN_WIDTH : 0;
		int bufferY = 0;

		DMAChain *chain = &dmaChains[usingSecondFrame];
		usingSecondFrame = !usingSecondFrame;

		uint32_t *ptr;

		GPU_GP1 = gp1_fbOffset(bufferX, bufferY);

		chain->nextPacket = chain->data;

		ptr = allocatePacket(chain, 4);
		ptr[0] = gp0_texpage(0, true, false);
		ptr[1] = gp0_fbOffset1(bufferX, bufferY);
		ptr[2] = gp0_fbOffset2(
			bufferX + SCREEN_WIDTH - 1, bufferY + SCREEN_HEIGHT - 2);
		ptr[3] = gp0_fbOrigin(bufferX, bufferY);

		ptr = allocatePacket(chain, 3);
		ptr[0] = gp0_rgb(0x5e, 0x8b, 0xeb) | gp0_vramFill();
		ptr[1] = gp0_xy(bufferX, bufferY);
		ptr[2] = gp0_xy(SCREEN_WIDTH, SCREEN_HEIGHT);
		char controllerbuffer[256];
		// get the controller button press

		snprintf(controllerbuffer, sizeof(controllerbuffer), "%i", getButtonPress(0));
		// printString(chain, &font, 56,100, controllerbuffer);
		uint16_t buttons = getButtonPress(0);
		uint16_t pressedButtons = ~previousButtons & buttons;

		if (creditsmenu == 0)
		{
			if (pressedButtons & BUTTON_MASK_UP)
			{
				if (selectedindex > 1)
				{
					selectedindex = selectedindex - 1;
					if (startnumber - selectedindex == 0)
					{
						startnumber = startnumber - 1;
					}
				}
			}
			if (pressedButtons & BUTTON_MASK_DOWN)
			{
				printf("DEBUG:DOWN  :%d\n", selectedindex);
				if (selectedindex < fileEntryCount)
				{
					selectedindex = selectedindex + 1;
					if (selectedindex > 20)
					{
						startnumber = selectedindex - 20;
					}
				}
			}

			if (pressedButtons & BUTTON_MASK_RIGHT)
			{
				if (selectedindex < fileEntryCount - 20)
				{
					selectedindex = selectedindex + 20;
					startnumber = startnumber + 20;
				}
			}

			if (pressedButtons & BUTTON_MASK_LEFT)
			{
				if (selectedindex > 20)
				{
					selectedindex = selectedindex - 20;
					if (startnumber - 20 <= 0)
					{
						startnumber = 0;
					}
					else
					{
						startnumber = startnumber - 20;
					}
				}
			}

			if (pressedButtons & BUTTON_MASK_START)
			{
				printf("DEBUG: selectedindex :%d\n", selectedindex);
				//			 SpicyJPEG's code
				//			uint8_t test[] = {0x50, 0xd1, 0xab,0xfe} ;
				//			issueCDROMCommand(0x19,test,sizeof(test));

				//			 Rama's code
				//			StartCommand();
				//		WriteParam( 0x50 );
				//		WriteParam( 0xf2 );
				//		WriteParam( selectedindex );
				// WriteParam( 0xf1 );
				//		WriteCommand( 0x19 );
				//	AckWithTimeout(500000);
			}

			if (pressedButtons & BUTTON_MASK_X)
			{
				fileData* file = file_manager_get_file_data(selectedindex - 1);
				if (file->flag == 0)
				{
					currentCommand = COMMAND_MOUNT_FILE;
				}
				else
				{
					currentCommand = COMMAND_GOTO_DIRECTORY;
				}

				//		 Rama's code
				//		StartCommand();
				//		WriteParam( 0x50 );
				//		WriteParam( 0xd1 );
				//		WriteParam( 0xab );
				//		WriteParam( 0xfe );
				//		WriteCommand( 0x19 );
				//		AckWithTimeout(500000);
			}

			if (pressedButtons & BUTTON_MASK_SQUARE)
			{
				currentCommand = COMMAND_GOTO_PARENT;
			}

			if (pressedButtons & BUTTON_MASK_TRIANGLE)
			{
				currentCommand = COMMAND_BOOTLOADER;
			}

			if (pressedButtons & BUTTON_MASK_SELECT)
			{
				creditsmenu = 1;
			}

			// char strbuffer[1024];

			// snprintf(strbuffer, sizeof(strbuffer), "%s", txtBuffer);
			// printString(chain, &font, 12, 200, strbuffer);
			// printf(strbuffer);
			// if ( loadchecker == 0){
			//	printString(chain, &font, 12, 220, "file not found");
			// } else {
			//	printString(chain, &font, 12, 220, "file found actually");
			//	printf("found file\n");
			// }

			printString(
				chain, &font, 16, 10,
				"PicoStation Plus Menu");

			if (currentCommand != COMMAND_NONE)
			{
				printString(chain, &font, 40, 40, "Please Wait Loading...");
			}
			else
			{

				char fbuffer[32];
				snprintf(fbuffer, sizeof(fbuffer), "index: %i of %i", selectedindex, fileEntryCount);
				printString(chain, &font, 206, 10, fbuffer);

				if (fileEntryCount > 0)
				{
					for (int i = startnumber; i < startnumber + 18; i++)
					{
						fileData* file = file_manager_get_file_data(i);

						char buffer[300];
						snprintf(buffer, sizeof(buffer), "%i %s %s\n", i + 1, file->flag == 1 ? "\x92" : "\x8f", file->filename);
						printString(chain, &font, 12, 30 + (i - startnumber) * 10, buffer);

						if (i + 1 == selectedindex)
						{

							printString(
								chain, &font, 5, 30 + (i - startnumber) * 10,
								">");
						}
						if ((uint32_t)i == fileEntryCount - 1)
						{
							break;
						}
					}

					
					printString(chain, &font, 12, 220, "X Select, [] Parent Folder");
				}
				else
				{
					printString(chain, &font, 40, 40, "Empty Folder");
				}

			}

		}
		else
		{
			printString(
				chain, &font, 40, 40,
				"PicosSation Plus Menu Alpha Release");
			printString(
				chain, &font, 40, 80,
				"Huge thanks to Rama, Skitchin, Raijin, SpicyJpeg,\nDanhans42, NicholasNoble and ChatGPT");

			printString(
				chain, &font, 40, 120,
				"https://github.com/team-Resurgent/picostation-menu");

			if (pressedButtons & BUTTON_MASK_CIRCLE)
			{
				creditsmenu = 0;
			}
		}

		//	char printBuffer[1024];

		// 	sprintf(printBuffer, "%i", modelLba);
		//	printf("LBA: %i \n", modelLba);

		//	printString(chain, &font, 56,100, printBuffer);

		previousButtons = buttons;
		*(chain->nextPacket) = gp0_endTag(0);
		waitForGP0Ready();
		waitForVblank();
		sendLinkedList(chain->data);

		if (currentCommand != COMMAND_NONE)
		{
			if (currentCommand == COMMAND_GOTO_PARENT)
			{
				fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_PARENT, 0);
				selectedindex = 1;
			}
			else if (currentCommand == COMMAND_BOOTLOADER)
			{
				//sendCommand(COMMAND_BOOTLOADER, 0xBEEF);
			}
			else if (currentCommand == COMMAND_GOTO_DIRECTORY)
			{
				fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_DIRECTORY, selectedindex - 1);
				selectedindex = 1;
			}
			else if (currentCommand == COMMAND_MOUNT_FILE)
			{
				sendCommand(COMMAND_MOUNT_FILE, selectedindex - 1);
				//softReset();
				softFastReboot();
			}
			
			currentCommand = COMMAND_NONE;
		}
	}

	return 0;
}
