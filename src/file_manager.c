#include "file_manager.h"
#include <stdlib.h>
#include <string.h>

/*
 * Fall back to statically allocated storage if the heap cannot satisfy the
 * allocations requested in file_manager_init().  This keeps the menu usable
 * even in low-memory scenarios instead of crashing when the global pointers
 * remain NULL.
 */
static uint16_t fileIndexStatic[MAX_FILE_ITEMS];
static fileData fileDataStatic[MAX_FILE_ITEMS];

uint16_t *fileIndexBuffer;
fileData *fileDataBuffer;

int file_manager_compare(uint16_t indexA, uint16_t indexB) 
{
    const fileData* a = &fileDataBuffer[indexA];
    const fileData* b = &fileDataBuffer[indexB];

    if ((a->flag == 1) && (b->flag == 0)) return -1; // Directories come first
    if ((a->flag == 0) && (b->flag == 1)) return 1;  // Files after directories

    // Both same type, compare names
    return strcmp(a->filename, b->filename);
}

static void file_manager_quicksort(int left, int right)
{
    if (left >= right)
    {
        return;
    }

    /*
     * Use signed indices internally so that calls such as sort(0) do not cause
     * the "right" parameter to wrap around and index past the end of the
     * buffers.  This also avoids repeated integer promotions in the hot loops.
     */
    const int pivotIndex = fileIndexBuffer[left + ((right - left) / 2)];

    int i = left;
    int j = right;

    while (i <= j)
    {
        while (file_manager_compare(fileIndexBuffer[i], pivotIndex) < 0)
        {
            i++;
        }

        while (file_manager_compare(fileIndexBuffer[j], pivotIndex) > 0)
        {
            j--;
        }

        if (i <= j)
        {
            const uint16_t temp = fileIndexBuffer[i];
            fileIndexBuffer[i] = fileIndexBuffer[j];
            fileIndexBuffer[j] = temp;
            i++;
            j--;
        }
    }

    if (left < j)
    {
        file_manager_quicksort(left, j);
    }

    if (i < right)
    {
        file_manager_quicksort(i, right);
    }
}

void file_manager_clean_list(uint16_t *count)
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

void file_manager_init(void)
{
    uint16_t *const indexAlloc = (uint16_t *)malloc(sizeof(uint16_t) * MAX_FILE_ITEMS);
    fileData *const dataAlloc = (fileData *)malloc(sizeof(fileData) * MAX_FILE_ITEMS);

    if (!indexAlloc || !dataAlloc)
    {
        /*
         * Low-memory situations are rare but catastrophic if we kept the NULL
         * pointers: guard against them by falling back to the static buffers.
         */
        free(indexAlloc);
        free(dataAlloc);
        fileIndexBuffer = fileIndexStatic;
        fileDataBuffer = fileDataStatic;
        return;
    }

    fileIndexBuffer = indexAlloc;
    fileDataBuffer = dataAlloc;
}

void file_manager_init_file_data(uint16_t index, uint8_t flag, const char *filename, uint16_t filename_length)
{
    fileData *file = &fileDataBuffer[index];

    /*
     * Clamp and copy the filename manually to make sure we never overrun the
     * destination buffer, even if the caller passes a bogus length.
     */
    file->flag = flag;

    uint16_t safeLength = 0;
    if (filename != NULL)
    {
        safeLength = filename_length;
        if (safeLength > MAX_FILE_LENGTH)
        {
            safeLength = MAX_FILE_LENGTH;
        }

        memcpy(file->filename, filename, safeLength);
    }

    file->filename[safeLength] = '\0';
    fileIndexBuffer[index] = index;
}

fileData *file_manager_get_file_data(uint16_t index)
{
    const uint16_t fileIndex = fileIndexBuffer[index];
    return &fileDataBuffer[fileIndex];
}

uint16_t file_manager_get_file_index(uint16_t index)
{
    return fileIndexBuffer[index];
}

void file_manager_sort(uint16_t count)
{
    if (count < 2)
    {
        /* Sorting zero or one element is a no-op and avoids signed wraparound. */
        return;
    }

    file_manager_quicksort(0, (int)count - 1);
}
