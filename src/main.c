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
#include <stdio.h>
#include "gpu.h"
#include "ps1/gpucmd.h"
#include "ps1/registers.h"
#include "controller.c"
#include "cdrom.c"


// In order to pick sprites (characters) out of our spritesheet, we need a table
// listing all of them (in ASCII order in this case) with their UV coordinates
// within the sheet as well as their dimensions. In this example we're going to
// hardcode the table, however in an actual game you may want to store this data
// in the same file as the image and palette data.
typedef struct {
	uint8_t x, y, width, height;
} SpriteInfo;

static const SpriteInfo fontSprites[] = {
	{ .x =  6, .y =  0, .width = 2, .height = 9 }, // !
	{ .x = 12, .y =  0, .width = 4, .height = 9 }, // "
	{ .x = 18, .y =  0, .width = 6, .height = 9 }, // #
	{ .x = 24, .y =  0, .width = 6, .height = 9 }, // $
	{ .x = 30, .y =  0, .width = 6, .height = 9 }, // %
	{ .x = 36, .y =  0, .width = 6, .height = 9 }, // &
	{ .x = 42, .y =  0, .width = 2, .height = 9 }, // '
	{ .x = 48, .y =  0, .width = 3, .height = 9 }, // (
	{ .x = 54, .y =  0, .width = 3, .height = 9 }, // )
	{ .x = 60, .y =  0, .width = 4, .height = 9 }, // *
	{ .x = 66, .y =  0, .width = 6, .height = 9 }, // +
	{ .x = 72, .y =  0, .width = 3, .height = 9 }, // ,
	{ .x = 78, .y =  0, .width = 6, .height = 9 }, // -
	{ .x = 84, .y =  0, .width = 2, .height = 9 }, // .
	{ .x = 90, .y =  0, .width = 6, .height = 9 }, // /
	{ .x =  0, .y =  9, .width = 6, .height = 9 }, // 0
	{ .x =  6, .y =  9, .width = 6, .height = 9 }, // 1
	{ .x = 12, .y =  9, .width = 6, .height = 9 }, // 2
	{ .x = 18, .y =  9, .width = 6, .height = 9 }, // 3
	{ .x = 24, .y =  9, .width = 6, .height = 9 }, // 4
	{ .x = 30, .y =  9, .width = 6, .height = 9 }, // 5
	{ .x = 36, .y =  9, .width = 6, .height = 9 }, // 6
	{ .x = 42, .y =  9, .width = 6, .height = 9 }, // 7
	{ .x = 48, .y =  9, .width = 6, .height = 9 }, // 8
	{ .x = 54, .y =  9, .width = 6, .height = 9 }, // 9
	{ .x = 60, .y =  9, .width = 2, .height = 9 }, // :
	{ .x = 66, .y =  9, .width = 3, .height = 9 }, // ;
	{ .x = 72, .y =  9, .width = 6, .height = 9 }, // <
	{ .x = 78, .y =  9, .width = 6, .height = 9 }, // =
	{ .x = 84, .y =  9, .width = 6, .height = 9 }, // >
	{ .x = 90, .y =  9, .width = 6, .height = 9 }, // ?
	{ .x =  0, .y = 18, .width = 6, .height = 9 }, // @
	{ .x =  6, .y = 18, .width = 6, .height = 9 }, // A
	{ .x = 12, .y = 18, .width = 6, .height = 9 }, // B
	{ .x = 18, .y = 18, .width = 6, .height = 9 }, // C
	{ .x = 24, .y = 18, .width = 6, .height = 9 }, // D
	{ .x = 30, .y = 18, .width = 6, .height = 9 }, // E
	{ .x = 36, .y = 18, .width = 6, .height = 9 }, // F
	{ .x = 42, .y = 18, .width = 6, .height = 9 }, // G
	{ .x = 48, .y = 18, .width = 6, .height = 9 }, // H
	{ .x = 54, .y = 18, .width = 4, .height = 9 }, // I
	{ .x = 60, .y = 18, .width = 5, .height = 9 }, // J
	{ .x = 66, .y = 18, .width = 6, .height = 9 }, // K
	{ .x = 72, .y = 18, .width = 6, .height = 9 }, // L
	{ .x = 78, .y = 18, .width = 6, .height = 9 }, // M
	{ .x = 84, .y = 18, .width = 6, .height = 9 }, // N
	{ .x = 90, .y = 18, .width = 6, .height = 9 }, // O
	{ .x =  0, .y = 27, .width = 6, .height = 9 }, // P
	{ .x =  6, .y = 27, .width = 6, .height = 9 }, // Q
	{ .x = 12, .y = 27, .width = 6, .height = 9 }, // R
	{ .x = 18, .y = 27, .width = 6, .height = 9 }, // S
	{ .x = 24, .y = 27, .width = 6, .height = 9 }, // T
	{ .x = 30, .y = 27, .width = 6, .height = 9 }, // U
	{ .x = 36, .y = 27, .width = 6, .height = 9 }, // V
	{ .x = 42, .y = 27, .width = 6, .height = 9 }, // W
	{ .x = 48, .y = 27, .width = 6, .height = 9 }, // X
	{ .x = 54, .y = 27, .width = 6, .height = 9 }, // Y
	{ .x = 60, .y = 27, .width = 6, .height = 9 }, // Z
	{ .x = 66, .y = 27, .width = 3, .height = 9 }, // [
	{ .x = 72, .y = 27, .width = 6, .height = 9 }, // Backslash
	{ .x = 78, .y = 27, .width = 3, .height = 9 }, // ]
	{ .x = 84, .y = 27, .width = 4, .height = 9 }, // ^
	{ .x = 90, .y = 27, .width = 6, .height = 9 }, // _
	{ .x =  0, .y = 36, .width = 3, .height = 9 }, // `
	{ .x =  6, .y = 36, .width = 6, .height = 9 }, // a
	{ .x = 12, .y = 36, .width = 6, .height = 9 }, // b
	{ .x = 18, .y = 36, .width = 6, .height = 9 }, // c
	{ .x = 24, .y = 36, .width = 6, .height = 9 }, // d
	{ .x = 30, .y = 36, .width = 6, .height = 9 }, // e
	{ .x = 36, .y = 36, .width = 5, .height = 9 }, // f
	{ .x = 42, .y = 36, .width = 6, .height = 9 }, // g
	{ .x = 48, .y = 36, .width = 5, .height = 9 }, // h
	{ .x = 54, .y = 36, .width = 2, .height = 9 }, // i
	{ .x = 60, .y = 36, .width = 4, .height = 9 }, // j
	{ .x = 66, .y = 36, .width = 5, .height = 9 }, // k
	{ .x = 72, .y = 36, .width = 2, .height = 9 }, // l
	{ .x = 78, .y = 36, .width = 6, .height = 9 }, // m
	{ .x = 84, .y = 36, .width = 5, .height = 9 }, // n
	{ .x = 90, .y = 36, .width = 6, .height = 9 }, // o
	{ .x =  0, .y = 45, .width = 6, .height = 9 }, // p
	{ .x =  6, .y = 45, .width = 6, .height = 9 }, // q
	{ .x = 12, .y = 45, .width = 6, .height = 9 }, // r
	{ .x = 18, .y = 45, .width = 6, .height = 9 }, // s
	{ .x = 24, .y = 45, .width = 5, .height = 9 }, // t
	{ .x = 30, .y = 45, .width = 5, .height = 9 }, // u
	{ .x = 36, .y = 45, .width = 6, .height = 9 }, // v
	{ .x = 42, .y = 45, .width = 6, .height = 9 }, // w
	{ .x = 48, .y = 45, .width = 6, .height = 9 }, // x
	{ .x = 54, .y = 45, .width = 6, .height = 9 }, // y
	{ .x = 60, .y = 45, .width = 5, .height = 9 }, // z
	{ .x = 66, .y = 45, .width = 4, .height = 9 }, // {
	{ .x = 72, .y = 45, .width = 2, .height = 9 }, // |
	{ .x = 78, .y = 45, .width = 4, .height = 9 }, // }
	{ .x = 84, .y = 45, .width = 6, .height = 9 }, // ~
	{ .x = 90, .y = 45, .width = 6, .height = 9 }  // Invalid character
};

#define FONT_FIRST_TABLE_CHAR '!'
#define FONT_SPACE_WIDTH      4
#define FONT_TAB_WIDTH        32
#define FONT_LINE_HEIGHT      10

static void printString(
	DMAChain *chain, const TextureInfo *font, int x, int y, const char *str
) {
	int currentX = x, currentY = y;

	uint32_t *ptr;

	// Start by sending a texpage command to tell the GPU to use the font's
	// spritesheet. Note that the texpage command before a drawing command can
	// be omitted when reusing the same texture, so sending it here just once is
	// enough.
	ptr    = allocatePacket(chain, 1);
	ptr[0] = gp0_texpage(font->page, false, false);

	// Iterate over every character in the string.
	for (; *str; str++) {
		char ch = *str;

		// Check if the character is "special" and shall be handled without
		// drawing any sprite, or if it's invalid and should be rendered as a
		// box with a question mark (character code 127).
		switch (ch) {
			case '\t':
				currentX += FONT_TAB_WIDTH - 1;
				currentX -= currentX % FONT_TAB_WIDTH;
				continue;

			case '\n':
				currentX  = x;
				currentY += FONT_LINE_HEIGHT;
				continue;

			case ' ':
				currentX += FONT_SPACE_WIDTH;
				continue;

			case '\x80' ... '\xff':
				ch = '\x7f';
				break;
		}

		// If the character was not a tab, newline or space, fetch its
		// respective entry from the sprite coordinate table.
		const SpriteInfo *sprite = &fontSprites[ch - FONT_FIRST_TABLE_CHAR];

		// Draw the character, summing the UV coordinates of the spritesheet in
		// VRAM to those of the sprite itself within the sheet. Enable blending
		// to make sure any semitransparent pixels in the font get rendered
		// correctly.
		ptr    = allocatePacket(chain, 4);
		ptr[0] = gp0_rectangle(true, true, true);
		ptr[1] = gp0_xy(currentX, currentY);
		ptr[2] = gp0_uv(font->u + sprite->x, font->v + sprite->y, font->clut);
		ptr[3] = gp0_xy(sprite->width, sprite->height);

		// Move onto the next character.
		currentX += sprite->width;
	}
}

#define SCREEN_WIDTH     320
#define SCREEN_HEIGHT    240
#define FONT_WIDTH       96
#define FONT_HEIGHT      56
#define FONT_COLOR_DEPTH GP0_COLOR_4BPP

extern const uint8_t fontTexture[], fontPalette[], piTexture[];

#define MAX_LINES 3000   // Maksimum satır sayısı
#define MAX_LENGTH 31


void parseLines(char *dataBuffer, char lines[MAX_LINES][MAX_LENGTH], int *lineCount) {
    if (!dataBuffer) {
        return;
    }

    *lineCount = 0;  // Satır sayısını sıfırla
    char *start = dataBuffer;
    char *end = dataBuffer;

    while (*start != '\0' && *lineCount < MAX_LINES) {
        // Satır sonunu bul
        while (*end != '\n' && *end != '\0') {
            end++;
        }

        // Satırın uzunluğunu hesapla
        int length = end - start;
        if (length >= MAX_LENGTH) {
            length = MAX_LENGTH - 1;
        }

        // Satırı kopyala
        for (int i = 0; i < length; i++) {
            lines[*lineCount][i] = start[i];
        }
        lines[*lineCount][length] = '\0'; // Null-terminator ekle

        (*lineCount)++;

        // Sonraki satıra geç
        if (*end == '\n') {
            end++;
        }
        start = end;
    }
}

char lines[MAX_LINES][MAX_LENGTH];
int lineCount = 0;
uint8_t test[] = {0x50, 0xfa, 0xf0,0xf1} ;
int main(int argc, const char **argv) {
	initSerialIO(115200);
	initControllerBus();

	if ((GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL) {
		puts("Using PAL mode");
		setupGPU(GP1_MODE_PAL, SCREEN_WIDTH, SCREEN_HEIGHT);
	} else {
		puts("Using NTSC mode");
		setupGPU(GP1_MODE_NTSC, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	DMA_DPCR |= DMA_DPCR_ENABLE << (DMA_GPU * 4);

	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);
	GPU_GP1 = gp1_dispBlank(false);

	TextureInfo font;

	uploadIndexedTexture(
		&font, fontTexture, fontPalette, SCREEN_WIDTH * 2, 0, SCREEN_WIDTH * 2,
		FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT, FONT_COLOR_DEPTH
	);


	DMAChain dmaChains[2];
	bool     usingSecondFrame = false;

	uint8_t test[] = {0x50, 0xf1,0x00,0x00} ;
	issueCDROMCommand(0x19,test,sizeof(test));

	char text[200] = "Tekken 3\nTime Crisis\nDriver\nSpyro 1\nTomb Raider\nStreet Fighter Alpha 3";

    int selectedindex = 1;

    char lines[MAX_LINES][MAX_LENGTH];
    int lineCount = 0;
    
    parseLines((char *)text, lines, &lineCount);
    int startnumber = 0;

    

	uint16_t previousbuttons = getButtonPress(0);
	for (;;) {
		int bufferX = usingSecondFrame ? SCREEN_WIDTH : 0;
		int bufferY = 0;

		DMAChain *chain  = &dmaChains[usingSecondFrame];
		usingSecondFrame = !usingSecondFrame;

		uint32_t *ptr;

		GPU_GP1 = gp1_fbOffset(bufferX, bufferY);

		chain->nextPacket = chain->data;

		ptr    = allocatePacket(chain, 4);
		ptr[0] = gp0_texpage(0, true, false);
		ptr[1] = gp0_fbOffset1(bufferX, bufferY);
		ptr[2] = gp0_fbOffset2(
			bufferX + SCREEN_WIDTH - 1, bufferY + SCREEN_HEIGHT - 2
		);
		ptr[3] = gp0_fbOrigin(bufferX, bufferY);

		ptr    = allocatePacket(chain, 3);
		ptr[0] = gp0_rgb(64, 64, 64) | gp0_vramFill();
		ptr[1] = gp0_xy(bufferX, bufferY);
		ptr[2] = gp0_xy(SCREEN_WIDTH, SCREEN_HEIGHT);

		char controllerbuffer[256];
		//get the controller button press

		snprintf(controllerbuffer, sizeof(controllerbuffer), "%i", getButtonPress(0));
		printString(chain, &font, 56,100, controllerbuffer);
		uint16_t buttons = getButtonPress(0);

		if((!((previousbuttons >> 4) & 1)) && ((buttons >> 4) & 1))   {
            if (selectedindex > 1){
                selectedindex = selectedindex - 1;
                if(startnumber - selectedindex == 0){
                    startnumber = startnumber - 1;
                }
				for (int i = 0; i < 5; i++)
    				waitForVSync();
            }
        }
        if((!((previousbuttons >> 6) & 1)) && ((buttons >> 6) & 1))    {
            if (selectedindex < lineCount){
                selectedindex = selectedindex + 1;
                if(selectedindex > 25){
                    startnumber = selectedindex - 25;
                }
				for (int i = 0; i < 5; i++)
    				waitForVSync();
            }
        }

        if((!((previousbuttons >> 5) & 1)) && ((buttons >> 5) & 1))    {
            if (selectedindex < lineCount - 25){
                selectedindex = selectedindex +25;
                startnumber = startnumber+25;
				for (int i = 0; i < 5; i++)
    				waitForVSync();
            }
        }


        if((!((previousbuttons >> 7) & 1)) && ((buttons >> 7) & 1))    {
            if (selectedindex > 25){
                selectedindex = selectedindex - 25;
                if (startnumber-25 <= 0){
                    startnumber = 0;
                } else {
                    startnumber = startnumber - 25;
                }
				for (int i = 0; i < 5; i++)
    				waitForVSync();
            }
        }

        if((!((previousbuttons >> 3) & 1)) && ((buttons >> 3) & 1))    {
                printf("DEBUG: selectedindex :%d\n", selectedindex);
				/* SpicyJPEG's code 
				uint8_t test[] = {0x50, 0xd1, 0xab,0xfe} ;
				issueCDROMCommand(0x19,test,sizeof(test));
				*/
				/* Rama's code */
				StartCommand();
				WriteParam( 0x50 );
				WriteParam( 0xf2 );
				WriteParam( selectedindex );
				//WriteParam( 0xf1 );
				WriteCommand( 0x19 );
				AckWithTimeout(500000);
        }

		if((!((previousbuttons >> 14) & 1)) && ((buttons >> 14) & 1))    {
			printf("DEBUG:X selectedindex  :%d\n", selectedindex);
			/* SpicyJPEG's code */
			uint8_t test[] = {0x50, 0xf2, selectedindex} ;
			issueCDROMCommand(0x19,test,sizeof(test));
			
			/* Rama's code 
			StartCommand();
			WriteParam( 0x50 );
			WriteParam( 0xd1 );
			WriteParam( 0xab );
			WriteParam( 0xfe );
			WriteCommand( 0x19 );
			AckWithTimeout(500000);*/
		}

		if((!((previousbuttons >> 12) & 1)) && ((buttons >> 12) & 1))    {
			printf("DEBUG:X selectedindex  :%d\n", selectedindex);
			/* reset */
			uint8_t test[] = {0x50, 0xfa, 0xBE, 0xEF} ;
			issueCDROMCommand(0x19,test,sizeof(test));
			
		}


		previousbuttons = buttons;

		printString(
			chain, &font, 16, 10,
			"Picostation Game Loader"
		);

		for (int i = startnumber; i < startnumber + 25; i++) {
        
			char buffer[32];

			snprintf(buffer, sizeof(buffer), "%i -%s", i+1, lines[i]);
			printString(chain, &font, 12, 30+i*10, buffer);	

			if(i + 1 == selectedindex){

				printString(
					chain, &font, 5, 30+i*10,
					">"
				);

			}
			if(i == lineCount- 1 ){
				break;
			}
		}





		// Show the current frame number by formatting some text into a
		// temporary buffer then printing it.
		

		*(chain->nextPacket) = gp0_endTag(0);

		waitForGP0Ready();
		waitForVSync();
		sendLinkedList(chain->data);
	}

	return 0;
}
