#include "file_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint16_t* fileIndexBuffer;
fileData* fileDataBuffer;

void file_manager_quicksort(uint16_t left, uint16_t right) 
{
    if (left >= right)
    {
        return;
    }

    int pivotIndex = fileIndexBuffer[(left + right) / 2];
    const char* pivot = fileDataBuffer[pivotIndex].filename;

    int i = left;
    int j = right;

    while (i <= j) {
        while (strcmp(fileDataBuffer[fileIndexBuffer[i]].filename, pivot) < 0) i++;
        while (strcmp(fileDataBuffer[fileIndexBuffer[j]].filename, pivot) > 0) j--;

        if (i <= j) {
            uint16_t temp = fileIndexBuffer[i];
            fileIndexBuffer[i] = fileIndexBuffer[j];
            fileIndexBuffer[j] = temp;
            i++;
            j--;
        }
    }

    if (left < j) file_manager_quicksort(left, j);
    if (i < right) file_manager_quicksort(i, right);
}

void file_manager_clean_list(uint16_t* count)
{
    int i = 0;
    while (i < *count - 1)
    {
        int binIndex = fileIndexBuffer[i];
        const char* binName = fileDataBuffer[binIndex].filename;
        size_t len = strlen(binName);

        if (len >= 4 && strcmp(binName + len - 4, ".bin") == 0)
        {
            int cueIndex = fileIndexBuffer[i + 1];
            const char* cueName = fileDataBuffer[cueIndex].filename;
            if (strlen(cueName) == len &&
                strncmp(binName, cueName, len - 4) == 0 &&
                strcmp(cueName + len - 4, ".cue") == 0)
            {
                for (int j = i; j < *count - 1; ++j)
                {
                    fileIndexBuffer[j] = fileIndexBuffer[j + 1];
                }
                (*count)--;
                continue;
            }
        }
        i++;
    }
}

void file_manager_init()
{
	fileIndexBuffer = (uint16_t*)malloc(sizeof(uint16_t) * MAX_FILE_ITEMS);
	fileDataBuffer = (fileData*)malloc(sizeof(fileData) * MAX_FILE_ITEMS);
}

void file_manager_init_file_data(uint16_t index, uint8_t flag, char* filename, uint16_t filename_length)
{
	fileData* file = &fileDataBuffer[index];
	file->flag = flag;
	memcpy(file->filename, filename, filename_length);
	file->filename[filename_length] = 0;
	fileIndexBuffer[index] = index;
}

fileData* file_manager_get_file_data(uint16_t index)
{
	uint16_t fileIndex = fileIndexBuffer[index];
	return &fileDataBuffer[fileIndex];
}

uint16_t file_manager_get_file_index(uint16_t index)
{
	return fileIndexBuffer[index];
}

void file_manager_sort(uint16_t count)
{
	file_manager_quicksort(0, count - 1);
}
