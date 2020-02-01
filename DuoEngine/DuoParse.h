/*
Copyright (c) 2019 Mark Siner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef DUOPARSE_H
#define DUOPARSE_H

#include <winsock.h>

#include <stdio.h>
#include <stdbool.h>

#include "DuoEngine.h"


static int parseUintArg(char* arg, unsigned int* result, int base) {
    char* endPtr = NULL;
    errno = 0;
    unsigned int tmp = strtoul(arg, &endPtr, base);
    if (errno || endPtr == arg) {
        perror("failed to parse unsigned int");
        return 1;
    }
    *result = tmp;
    return 0;
}


static int parseIntArg(char* arg, int* result, int base) {
    char* endPtr = NULL;
    errno = 0;
    int tmp = strtol(arg, &endPtr, base);
    if (errno || endPtr == arg) {
        perror("failed to parse int");
        return 1;
    }
    *result = tmp;
    return 0;
}


static int parseFrequency(char* arg, float* result) {
    char* endPtr = NULL;
    float freq = 0;
    float freqMult = 1;
    size_t freqLen = strlen(arg);
    if (arg[freqLen - 1] == 'k' || arg[freqLen - 1] == 'K') {
        freqMult = 1000;
        arg[freqLen - 1] = 0;
    }
    else if (arg[freqLen - 1] == 'm' || arg[freqLen - 1] == 'M') {
        freqMult = 1000 * 1000;
        arg[freqLen - 1] = 0;
    }
    else if (arg[freqLen - 1] == 'g' || arg[freqLen - 1] == 'G') {
        freqMult = 1000 * 1000 * 1000;
        arg[freqLen - 1] = 0;
    }
    errno = 0;
    freq = strtof(arg, &endPtr);
    if (errno || endPtr == arg) {
        return 1;
    }
    *result = freq * freqMult;
    return 0;
}


static int parseSize(char* arg, size_t* result) {
    char* endPtr = NULL;
    unsigned int mult = 1;
    size_t argLen = strlen(arg);
    if (arg[argLen - 1] == 'k' || arg[argLen - 1] == 'K') {
        mult = 1024;
        arg[argLen - 1] = 0;
    }
    else if (arg[argLen - 1] == 'm' || arg[argLen - 1] == 'M') {
        mult = 1024 * 1024;
        arg[argLen - 1] = 0;
    }
    else if (arg[argLen - 1] == 'g' || arg[argLen - 1] == 'G') {
        mult = 1024 * 1024 * 1024;
        arg[argLen - 1] = 0;
    }
    errno = 0;
    unsigned long long tmp = strtoull(arg, &endPtr, 10);
    if (errno || endPtr == arg) {
        perror("failed to parse unsigned long long");
        return 1;
    }
    tmp *= mult;
    if (tmp > SIZE_MAX) {
        printf("specified size exceeds maximum of %zu", SIZE_MAX);
        return 1;
    }
    *result = (size_t)tmp;
    return 0;
}


static int parseAgcBandwidth(char* arg, unsigned int* result) {
    unsigned int tmp;
    if (parseUintArg(arg, &tmp, 10)) {
        printf("invalid AGC loop bandwidth, must be an unsigned int\n");
        return 1;
    }
    if (tmp != 0 && tmp != 5 && tmp != 50 && tmp != 100) {
        printf("invalid AGC loop bandwidth, must be 0, 5, 10, or 100\n");
        return 1;
    }
    *result = tmp;
    return 0;
}


static int parseAgcSetPoint(char* arg, int* result) {
    int tmp;
    if (parseIntArg(arg, &tmp, 10)) {
        printf("invalid AGC set point, must be an int\n");
        return 1;
    }
    if (tmp > 0 || tmp < -72) {
        printf("invalid AGC set point, must be in [-72-0] dBFS\n");
        return 1;
    }
    *result = tmp;
    return 0;
}


static int parseLnaState(char* arg, unsigned int* result) {
    unsigned int tmp;
    if (parseUintArg(arg, &tmp, 10)) {
        printf("invalid LNA state, must be an unsigned int\n");
        return 1;
    }
    if (tmp > 9) {
        printf("invalid LNA state, must be in [0-9]\n");
        return 1;
    }
    *result = tmp;
    return 0;
}


static int parseDecimFactor(char* arg, unsigned int* result) {
    unsigned int tmp;
    if (parseUintArg(arg, &tmp, 10)) {
        printf("invalid decimation factor, must be an unsigned int\n");
        return 1;
    }
    if (tmp != 1 && tmp != 2 && tmp != 4 && tmp != 8 && tmp != 16 && tmp != 32) {
        printf("invalid decimation factor, must be in [1,2,4,8,16,32]\n");
        return 1;
    }
    *result = tmp;
    return 0;
}


static int parseNotchFilter(char* arg, bool* mwfm, bool* dab) {
    if (strncmp(arg, "mwfm", 4) == 0) {
        *mwfm = true;
    }
    else if (strncmp(arg, "dab", 3) == 0) {
        *dab = true;
    }
    else {
        printf("invalid notch filter name [%s]\n", arg);
        return 1;
    }
    return 0;
}


#endif