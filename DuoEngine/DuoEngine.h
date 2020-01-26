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

#ifndef DUOENGINE_H
#define DUOENGINE_H

#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
* Representation of a transfer of data from engine to user.
* Includes redundant metadata to make it easy for users to interpret
* the data in multiple ways (e.g. scalar, sample, or frame).
* In context of DuoEngine, the following definitions apply:
*     scalar: single value I or Q
*     sample: complex sample from a single source (I, Q)
*     frame: pair of samples from two sources (Ia, Qa, Ib, Qb)
*/
struct DuoEngineTransfer {
    bool floatingPoint;
    unsigned int scalarSize;
    unsigned int sampleSize;
    unsigned int frameSize;
    unsigned int numBytes;
    unsigned int numScalars;
    unsigned int numSamples;
    unsigned int numFrames;
    void* data;
};


/**
* Function type to implement for user to receive data from DuoEngine
*
* @param transfer pointer to transfer object
*                 The user is not responsible for freeing any transfer
*                 memory and is also not guaranteed that the memory
*                 will be valid after returning from the callback
* @param userContext pointer to context memory specified in the
*                    userContext DuoEngine field
*/
typedef void (*DuoEngineTransferCallback)(struct DuoEngineTransfer* transfer, void *userContext);


/**
* Function type to implement for user to get periodic callbacks
* to allow user control in the main thread.
*
* @param userContext pointer to context memory specified in the
*                    userContext DuoEngine field
*
* @return zero to continue, non-zero to signal DuoEngine to stop
*/
typedef int (*DuoEngineControlCallback)(void *userContext);


/**
* Main configuration for DuoEngine
* User first calls duoEngineInit() to initialize with
* default values and then modifies specific settings.
*/
struct DuoEngine {
    // tuning frequency in Hz
    float tuneFreq;
    // AGC frequency in Hz, valid values are 0, 5, 50, 100
    unsigned int agcFreq;
    // AGC set point in dBFS
    int agcSetPoint;
    // LNA state in [0-9] where 0 is maximum gain
    unsigned int lnaState;
    // decimation factor in [1,2,4,8,16,32] where 1 is no decimation
    unsigned int decimFactor;
    // true to enable the frontend MW/FM notch filters
    bool notchMwfm;
    // true to enable the frontend DAB notch filter
    bool notchDab;
    /**
    * true to use the maximum 8 MHz ADC sample rate.
    * This will deliver 12 bit ADC resolution, but with slightly
    * better anti-aliaising performance at the widest bandwidth.
    * This mode is only available at 1.536 MHz analog bandwidth.
    * The default mode is to use a 6 MHz master sample clock.
    * That mode delivers 14 bit ADC resolution, but with slightly
    * inferior anti-aliaising performance at the widest bandwidth.
    * The default mode is also compatible with analog bandwidths of
    * 1.536 MHz, 600 kHz, 300 kHz, and 200 kHz. 6 MHz operation
    * should result in a slightly lower CPU load.
    */
    bool maxSampleRate;
    // true to use USB bulk transfer mode instead of isochronous
    bool usbBulkMode;
    // true to enable sdrplay_api debugging output
    bool apiDebug;
    // true to convert all sample scalars from short to float
    bool floatingPoint;
    /** 
    * maximum number of bytes DuoEngine can transfer to user
    * via transferCallback
    * NOTE: the actual transfer size may be less as it must be a
    * multiple of the frame size
    */
    unsigned int maxTransferSize;
    /**
    * pointer to user context struct that is passed back to user
    * as a parameter in each callback
    * NOTE: NULL is allowed
    */
    void* userContext;
    /**
    * pointer to user transfer callback function
    * NOTE: NULL is NOT allowed
    */
    DuoEngineTransferCallback transferCallback;
    /**
    * pointer to user control function
    * NOTE: NULL is allowed
    */
    DuoEngineControlCallback controlCallback;
};


#ifndef DEFAULT_AGC_FREQ
#define DEFAULT_AGC_FREQ (0)
#endif

#ifndef DEFAULT_AGC_SET_POINT
#define DEFAULT_AGC_SET_POINT (-30)
#endif

#ifndef DEFAULT_LNA_STATE
#define DEFAULT_LNA_STATE (4)
#endif

#ifndef DEFAULT_DECIM_FACTOR
#define DEFAULT_DECIM_FACTOR (1)
#endif

#ifndef DEFAULT_MAX_TRANSFER_SIZE
#define DEFAULT_MAX_TRANSFER_SIZE (10 * 1024)
#endif


/**
* Initialize engine struct with default values
*
* @param engine pointer to struct to initialize
*/
static void duoEngineInit(struct DuoEngine* engine) {
    memset(engine, 0, sizeof(struct DuoEngine));
    engine->agcFreq = DEFAULT_AGC_FREQ;
    engine->agcSetPoint = DEFAULT_AGC_SET_POINT;
    engine->lnaState = DEFAULT_LNA_STATE;
    engine->decimFactor = DEFAULT_DECIM_FACTOR;
    engine->floatingPoint = false;
    engine->maxTransferSize = DEFAULT_MAX_TRANSFER_SIZE;
}


/**
* Blocking function to start and run the engine.
*
* @param engine configuration
*
* @return zero on clean exit, non-zero otherwise
*/
int duoEngineRun(struct DuoEngine* engine);


#ifdef __cplusplus
}
#endif

#endif