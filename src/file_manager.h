#pragma once

#include <stdint.h>

#define MAX_FILE_LENGTH 255
#define MAX_FILE_ITEMS 4096

typedef struct
{
	uint8_t flag;
	char filename[MAX_FILE_LENGTH + 1];
} fileData;

void file_manager_init();
void file_manager_init_file_data(uint16_t index, uint8_t flag, char* filename, uint16_t filename_length);
fileData* file_manager_get_file_data(uint16_t index);
void file_manager_sort(int count);