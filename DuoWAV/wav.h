/*
Copyright (c) 2020 Mark Siner

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

#ifndef WAV_H
#define WAV_H

#include <stdint.h>
#include <stdbool.h>


// WAV "RIFF" chunk
struct WavRiffChunk {
    int8_t chunkId[4];
    uint32_t chunkSize;
    int8_t format[4];
};


// WAV "fmt" chunk
struct WavFmtChunk {
    int8_t chunkId[4];
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    uint16_t extSize; // required to support floating-point
};


// WAV "fact" chunk
struct WavFactChunk {
    int8_t chunkId[4];
    uint32_t chunkSize;
    uint32_t sampleLength;
};


// WAV "data" chunk
struct WavDataChunk {
    int8_t chunkId[4];
    uint32_t chunkSize;
    // samples follow this chunk but it is not represented
    // here to allow for easy overwriting of just the file header
    // using these chunk structs
};


/**
* Aggregation of chunk structs into a single WAV header struct.
* There are multiple valid header configurations for WAV files.
* The format used here is the minimum necessary to support a
* floating-point sample format, but is still valid for LPCM.
*/
struct WavHeader {
    struct WavRiffChunk riff;
    struct WavFmtChunk fmt;
    struct WavFactChunk fact; // required for floating-point
    struct WavDataChunk data;
    // samples follow the header data chunk
};


/**
* Simple function to copy a four character label to a header field.
* Specifically, this is used for copying string constants to chunkId fields.
*
* @param dest pointer to memory big enough for four bytes
* @param label pointer to string with at least four characters
*/
static void wavLabelCopy(int8_t* dest, const char* label) {
    dest[0] = label[0];
    dest[1] = label[1];
    dest[2] = label[2];
    dest[3] = label[3];
}


/**
* Determine endianess
*
* @return true if architecture is big-endian
*/
static bool wavIsBigEndian(void) {
    union {
        uint32_t intBuf;
        uint8_t byteBuf[4];
    } endianCheck;
    endianCheck.intBuf = 1;
    return endianCheck.byteBuf[3] == 1;
}


/**
* Initialize a WAV header struct for the specified configuration
*
* @param head pointer to header struct to initialize
* @param sampleRate sample rate in samples per second
* @param numChannels number of channels (e.g. stereo is 2 channels)
* @param bytesPerSample size of each sample in bytes
* @param floatingPoint true if samples will be IEEE floating point
*                      false if samples will be LPCM
*/
static void wavHeaderInit(
        struct WavHeader* head, uint32_t sampleRate, uint16_t numChannels,
        uint8_t bytesPerSample, bool floatingPoint) {
    // RIFF header
    if (wavIsBigEndian()) {
        wavLabelCopy(head->riff.chunkId, "RIFX");
    }
    else {
        wavLabelCopy(head->riff.chunkId, "RIFF");
    }
    head->riff.chunkSize = sizeof(struct WavHeader) - 8;
    wavLabelCopy(head->riff.format, "WAVE");

    // fmt header
    wavLabelCopy(head->fmt.chunkId, "fmt ");
    head->fmt.chunkSize = sizeof(struct WavFmtChunk) - 8;
    if (floatingPoint) {
        head->fmt.audioFormat = 3; // IEEE floating point
    }
    else {
        head->fmt.audioFormat = 1; // LPCM
    }
    head->fmt.numChannels = numChannels;
    head->fmt.sampleRate = sampleRate;
    head->fmt.byteRate = sampleRate * numChannels * bytesPerSample;
    head->fmt.blockAlign = numChannels * bytesPerSample;
    head->fmt.bitsPerSample = bytesPerSample * 8;
    head->fmt.extSize = 0; // required for floating-point

    // fact header
    wavLabelCopy(head->fact.chunkId, "fact");
    head->fact.chunkSize = sizeof(struct WavFactChunk) - 8;
    head->fact.sampleLength = 0;

    // data header
    wavLabelCopy(head->data.chunkId, "data");
    head->data.chunkSize = 0;
}


/**
* Update the necessary header fields with the actual size of data.
* This should be used after all data has been written and then
* the header block of the output file should be overwritten.
* NOTE: This is not a progressive update, the number of bytes specified
* is always assumed to be the total bytes for the file.
*
* @param head pointer to header struct to update
* @param dataBytesWritten total number of bytes written to the data portion
*/
static void wavHeaderUpdate(struct WavHeader* head, uint32_t dataBytesWritten) {
    head->riff.chunkSize = sizeof(struct WavHeader) - 8 + dataBytesWritten;
    uint32_t bytesPerFrame = head->fmt.bitsPerSample * 8 * head->fmt.numChannels;
    head->fact.sampleLength = dataBytesWritten / bytesPerFrame;
    head->data.chunkSize = dataBytesWritten;
}


#endif