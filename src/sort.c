#include "sort.h"
#include "file_manager.h"

#include <string.h>
#include <stdlib.h>

int compareEntriesByIndex(char* buffer, uint16_t i, uint16_t j) {
    char* a = buffer + (i << 8);
    char* b = buffer + (j << 8);
    return strcmp(a, b);
}

void quicksortIndices(char* buffer, uint16_t* indices, int left, int right) {
    if (left >= right) return;

    uint16_t pivotIndex = indices[(left + right) / 2];
    int i = left, j = right;

    while (i <= j) 
    {
        while (compareEntriesByIndex(buffer, indices[i], pivotIndex) < 0) ++i;
        while (compareEntriesByIndex(buffer, indices[j], pivotIndex) > 0) --j;

        if (i <= j) {
            uint16_t temp = indices[i];
            indices[i] = indices[j];
            indices[j] = temp;
            ++i;
            --j;
        }
    }

    quicksortIndices(buffer, indices, left, j);
    quicksortIndices(buffer, indices, i, right);
}

void sortFileBlocksByIndex(uint16_t* fileIndexBuffer, fileData* fileDataBuffer, uint16_t count)
{
    // uint16_t* indices = (uint16_t*)malloc(count * 2);
    // for (uint16_t i = 0; i < count; ++i) 
    // {
    //     indices[i] = i;
    // }

    // quicksortIndices(filenameBuffer, fileLookupBuffer, 0, (int)count - 1);

    // uint8_t* moved = (uint8_t*)malloc(count);
    // memset(moved, 0, count);

    // char temp[256];

    // for (size_t i = 0; i < count; ++i) {
    //     if (moved[i] || indices[i] == i)
    //         continue;

    //     size_t j = i;
    //     memcpy(temp, buffer + (j << 8), 256);

    //     while (!moved[j]) {
    //         moved[j] = 1;
    //         size_t k = indices[j];

    //         if (k == i) {
    //             memcpy(buffer + j * 256, temp, 256);
    //             break;
    //         }

    //         memcpy(buffer + (j << 8), buffer + (k << 8), 256);
    //         j = k;
    //     }
    // }

    // free(indices);
    // free(moved);
}