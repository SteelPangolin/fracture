#ifndef FPIMAGE_H
#define FPIMAGE_H

#include <stdlib.h>

struct floatImageHeader
{
    char sig[4];
    size_t numChannels;
    size_t w;
    size_t h;
};

#endif
