#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>

#include "errors.h"
#include "fpimage.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        ERR("missing args", "");
    }
    
    char* filePath = argv[1];
    int fd;
    struct stat sb;
    void* imgBase;
    size_t len;
    
    fd = open(filePath, O_RDONLY);
    CHK_SYSCALL(fd, "open() failed", filePath);
    CHK_SYSCALL(fstat(fd, &sb), "fstat() failed", filePath);
    if (!S_ISREG(sb.st_mode)) ERR("not a regular file", filePath);
    len = sb.st_size;
    if (len == 0) ERR("file is empty", filePath);
    imgBase = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    CHK_NULL(imgBase, "mmap() failed", filePath);
    
    struct floatImageHeader* imgHeaderBase = (struct floatImageHeader*)imgBase;
    if (   imgHeaderBase->sig[0] != '2'
        || imgHeaderBase->sig[1] != '3'
        || imgHeaderBase->sig[2] != 'l'
        || imgHeaderBase->sig[3] != 'f')
    {
        ERR("bad signature", "");
    }
    size_t numChannels = imgHeaderBase->numChannels;
    size_t w = imgHeaderBase->w;
    size_t h = imgHeaderBase->h;
    if (w == 0 || h == 0)
    {
        ERR("bad dimensions", "");
    }
    
    float* imgDataBase = imgBase + sizeof(struct floatImageHeader);
    float* imgDataEnd = (float*)(imgBase + len);
    float* minVals = malloc(numChannels * sizeof(float));
    float* maxVals = malloc(numChannels * sizeof(float));
    int i;
    for (i = 0; i < numChannels; i++)
    {
        minVals[i] = imgDataBase[i];
        maxVals[i] = imgDataBase[i];
    }
    float* pxlPtr;
    for (pxlPtr = imgDataBase + numChannels; pxlPtr != imgDataEnd; pxlPtr += numChannels)
    {
        for (i = 0; i < numChannels; i++)
        {
            minVals[i] = minVals[i] < pxlPtr[i] ? minVals[i] : pxlPtr[i];
            maxVals[i] = maxVals[i] > pxlPtr[i] ? maxVals[i] : pxlPtr[i];
        }
    }
    
    CHK_SYSCALL(munmap(imgBase, len), "munmap() failed", filePath);
    CHK_SYSCALL(close(fd), "close() failed", filePath);
    
    for (i = 0; i < numChannels; i++)
    {
        printf("channel %d: min = %0.2f, max = %0.2f\n", i, minVals[i], maxVals[i]);
    }
    free(minVals);
    free(maxVals);
    
    return EXIT_SUCCESS;
}