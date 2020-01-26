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

#include <Windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <conio.h>

#include "sdrplay_api.h"

#include "duoengine.h"


#define MAX_DEVS (6)
static const float SAMPLE_FREQ_DEFAULT = 6000000.0;
static const float SAMPLE_FREQ_MAXFS = 8000000.0;


// Context passed by DuoEngine to sdrplay_api
struct Context {
    // Device state
    sdrplay_api_DeviceT device;
    sdrplay_api_DeviceParamsT* params;
    // Buffer parameters
    void* buffer;
    unsigned int bufferSize;
    unsigned int bufferLen;
    // Buffer state
    unsigned int numSamplesA;
    unsigned int numSamplesB;
    unsigned int rxIdx;
    unsigned int txIdx;
    // User parameters
    DuoEngineTransferCallback transferCallback;
    DuoEngineControlCallback controlCallback;
    void* userContext;
    struct DuoEngineTransfer transfer;
};


/**
* Transfers data to DuoEngine user by call transferCallback()
*
* @params context DuoEngine context
*/
static void doTransfer(struct Context* context) {
    unsigned int offset = context->txIdx * context->transfer.scalarSize;
    context->transfer.data = (char*)context->buffer + offset;
    context->txIdx = (context->txIdx + context->transfer.numScalars) % context->bufferLen;
    context->transferCallback(&context->transfer, context->userContext);
}


/**
* sdrplay_api callback for tuner 1
* 
* @param xi real data buffer
* @param xq imaginary data buffer
* @param params stream parameters
* @param numSamples number of samples available from xi and xq
* @param reset true if a reset has occurred and buffer should be cleared
* @param cbContext pointer to DuoEngine Context
*/
static void callbackStreamA(
        short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
        unsigned int numSamples, unsigned int reset, void *cbContext) {
    struct Context* context = (struct Context*)cbContext;
    if (reset) {
        printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
        context->numSamplesA = 0;
        context->numSamplesB = 0;
        context->rxIdx = 0;
        context->txIdx = 0;
    }

    if (!reset && (context->numSamplesA || context->numSamplesB == 0)) {
        printf("buffer overflow: stream B has not been handled\n");
    }
    else if (!reset && context->numSamplesB != numSamples) {
        printf("buffer out of sync: numSamplesA=%u numSamplesB=%u\n", numSamples, context->numSamplesB);
    }
    else {
        context->numSamplesA = numSamples;

        unsigned int inIdx = 0;
        unsigned int bufIdx = context->rxIdx;

        if (context->transfer.floatingPoint) {
            float* floatBuffer = (float*)context->buffer;
            for (inIdx = 0; inIdx < numSamples; inIdx++) {
                floatBuffer[bufIdx++] = xi[inIdx] / (float)32767.0;
                floatBuffer[bufIdx++] = xq[inIdx] / (float)32767.0;
                // skip stream B samples
                bufIdx = (bufIdx + 2) % context->bufferLen;
                // Only need to take modulo at end of frame
            }
        }
        else {
            short* shortBuffer = (short*)context->buffer;
            for (inIdx = 0; inIdx < numSamples; inIdx++) {
                shortBuffer[bufIdx++] = xi[inIdx];
                shortBuffer[bufIdx++] = xq[inIdx];
                // skip stream B samples
                bufIdx = (bufIdx + 2) % context->bufferLen;
                // Only need to take modulo at end of frame
            }
        }
    }
}


/**
* sdrplay_api callback for tuner 2
*
* @param xi real data buffer
* @param xq imaginary data buffer
* @param params stream parameters
* @param numSamples number of samples available from xi and xq
* @param reset true if a reset has occurred and buffer should be cleared
* @param cbContext pointer to DuoEngine Context
*/
static void callbackStreamB(
        short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
        unsigned int numSamples, unsigned int reset, void *cbContext) {
    struct Context* context = (struct Context*)cbContext;
    if (reset) {
        printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
    }

    if (context->numSamplesA == 0) {
        printf("buffer out of sync: stream A has not been handled\n");
    }
    else if (context->numSamplesA != numSamples) {
        printf("buffer out of sync: numSamplesA=%u numSamplesB=%u\n", context->numSamplesA, numSamples);
    }
    else {
        context->numSamplesB = numSamples;

        unsigned int inIdx = 0;
        unsigned int bufIdx = context->rxIdx;

        if (context->transfer.floatingPoint) {
            float* floatBuffer = (float*)context->buffer;
            for (inIdx = 0; inIdx < numSamples; inIdx++) {
                // skip stream A samples
                bufIdx += 2;
                floatBuffer[bufIdx++] = xi[inIdx] / (float)32767.0;
                floatBuffer[bufIdx++] = xq[inIdx] / (float)32767.0;
                // Only need to take modulo at end of frame
                bufIdx = bufIdx % context->bufferLen;
                if ((bufIdx % context->transfer.numScalars) == 0) {
                    // we have a full packet ready to go
                    doTransfer(context);
                }
            }
        }
        else {
            short* shortBuffer = (short*)context->buffer;
            for (inIdx = 0; inIdx < numSamples; inIdx++) {
                // skip stream A samples
                bufIdx += 2;
                shortBuffer[bufIdx++] = xi[inIdx];
                shortBuffer[bufIdx++] = xq[inIdx];
                // Only need to take modulo at end of frame
                bufIdx = bufIdx % context->bufferLen;
                if ((bufIdx % context->transfer.numScalars) == 0) {
                    // we have a full transfer ready to go
                    doTransfer(context);
                }
            }
        }

        context->rxIdx = bufIdx;

        // clear to indicate to A that B has been handled
        context->numSamplesA = 0;
    }
}


/**
* sdrplay_api callback for non-data events
*
* @param eventId type of event that has occurred
* @param tuner tuner event relates to
* @param params pointer to the event callback union
* @param cbContext pointer to DuoEngine Context
*/
static void callbackEvent(
        sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
        sdrplay_api_EventParamsT *params, void *cbContext) {
    struct Context* context = (struct Context*)cbContext;
    switch (eventId) {
    case sdrplay_api_GainChange:
        printf(
            "sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n",
            "sdrplay_api_GainChange", (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A" :
            "sdrplay_api_Tuner_B", params->gainParams.gRdB, params->gainParams.lnaGRdB,
             params->gainParams.currGain);
        break;
    case sdrplay_api_PowerOverloadChange:
        printf(
            "sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n",
            (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A" : "sdrplay_api_Tuner_B",
            (params->powerOverloadParams.powerOverloadChangeType ==
             sdrplay_api_Overload_Detected) ? "sdrplay_api_Overload_Detected" :
               "sdrplay_api_Overload_Corrected");
        // Send update message to acknowledge power overload message received
        sdrplay_api_Update(
            context->device.dev, tuner,
            sdrplay_api_Update_Ctrl_OverloadMsgAck,
            sdrplay_api_Update_Ext1_None);
        break;
    case sdrplay_api_DeviceRemoved:
        printf("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
        break;
    default:
        printf("sdrplay_api_EventCb: %d, unhandled event\n", eventId);
        break;
    }
}


/**
* Call necessary functions to open sdrplay_api session.
* If openApi() succeeds, sdrplay_api_Close() should be called
* when API access is no longer needed.
*
* @param debugEnabled true to enabled API debugging
*
* @return zero on success, non-zero otherwise
*/
static int openApi(bool debugEnabled) {
    sdrplay_api_ErrT err;
    float ver = 0.0;

    if ((err = sdrplay_api_Open()) != sdrplay_api_Success) {
        printf("sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
        return 1;
    }

    // Enable debug logging output
    if ((err = sdrplay_api_DebugEnable(NULL, debugEnabled)) != sdrplay_api_Success) {
        printf("sdrplay_api_DebugEnable failed %s\n", sdrplay_api_GetErrorString(err));
        sdrplay_api_Close();
        return 1;
    }

    // Check API runtime version matches buildtime version
    if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success) {
        printf("sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
        sdrplay_api_Close();
        return 1;
    }
    if (ver != SDRPLAY_API_VERSION) {
        printf("API version don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, ver);
        sdrplay_api_Close();
        return 1;
    }
    return 0;
}


/**
* Call necessary functions to find and reserve an RSPDuo
* The openApi() function should be called before this function.
* If getDevice() succeeds, sdrplay_api_ReleaseDevice() should be
* called when device access is no longer needed.
*
* @param device pointer to sdrplay_api_DeviceT to fill in with
*               reserved device fields
* @param maxFs true to use the maximum 8 MHz ADC master sample rate
*
* @return zero on success, non-zero otherwise
*/
static int getDevice(sdrplay_api_DeviceT* device, bool maxFs) {
    sdrplay_api_ErrT err;
    sdrplay_api_DeviceT devs[MAX_DEVS];
    unsigned int numDevs = 0;
    unsigned int chosenIdx = 0;
    bool found = false;

    // Fetch list of available devices
    if ((err = sdrplay_api_GetDevices(devs, &numDevs, MAX_DEVS)) != sdrplay_api_Success) {
        printf("sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
        return 0;
    }
    printf("MaxDevs=%d NumDevs=%d\n", MAX_DEVS, numDevs);
    if (numDevs > 0) {
        for (unsigned int devIdx = 0; devIdx < numDevs; devIdx++) {
            sdrplay_api_DeviceT* curr = &devs[devIdx];
            if (curr->hwVer == SDRPLAY_RSPduo_ID) {
                printf("Dev[%u]: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", devIdx,
                       curr->SerNo, curr->hwVer, curr->tuner, curr->rspDuoMode);
                if (!found && !(curr->rspDuoMode & sdrplay_api_RspDuoMode_Dual_Tuner)) {
                    printf("Dual tuner mode unavailable\n");
                }
                else if (!found) {
                    *device = *curr;
                    chosenIdx = devIdx;
                    found = true;
                }
            }
            else {
                printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", devIdx,
                       curr->SerNo, curr->hwVer, curr->tuner);
            }
        }
        if (!found) {
            printf("No suitable RSPDuo devices available\n");
            return 1;
        }
    }
    else {
        return 1;
    }

    // Select tuner based on user input (or default to TunerA)
    device->tuner = sdrplay_api_Tuner_Both;

    // Set operating mode
    device->rspDuoMode = sdrplay_api_RspDuoMode_Dual_Tuner;
    if (maxFs) {
        device->rspDuoSampleFreq = SAMPLE_FREQ_MAXFS;
    }
    else {
        device->rspDuoSampleFreq = SAMPLE_FREQ_DEFAULT;
    }
    printf("Selected index=%u SerNo=%s\n", chosenIdx, device->SerNo);

    // Select chosen device
    if ((err = sdrplay_api_SelectDevice(device)) != sdrplay_api_Success) {
        printf("sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
        return 1;
    }
    return 0;
}


/**
* Configure one channel/tuner.
* This has been wrapped into a single function because DuoEngine
* configures the tuner identically.
*
* @param chanParams channel/tuner to configure
* @param engine pointer to DuoEngine configuration
*/
static void configureChannel(
        sdrplay_api_RxChannelParamsT *chanParams, struct DuoEngine* engine) {
    // Set center frequency
    chanParams->tunerParams.rfFreq.rfHz = engine->tuneFreq;

    // Set low IF frequency and analog bandwidth
    chanParams->tunerParams.bwType = sdrplay_api_BW_1_536;
    if (engine->maxSampleRate) {
        chanParams->tunerParams.ifType = sdrplay_api_IF_2_048;
        // This mode is only compatible with 1.536 MHz analog bandwidth
    }
    else {
        chanParams->tunerParams.ifType = sdrplay_api_IF_1_620;
        // At high decimation factors, narrow the analog passband
        if (engine->decimFactor == 4) {
            // 2 MS/s / 4 = 500 kHz
            chanParams->tunerParams.bwType = sdrplay_api_BW_0_600;
        }
        else if (engine->decimFactor == 8) {
            // 2 MS/s / 8 = 250 kHz
            chanParams->tunerParams.bwType = sdrplay_api_BW_0_300;
        }
        else if (engine->decimFactor == 16 || engine->decimFactor == 32) {
            // 2 MS/s / 16 = 125 kHz
            // 2 MS/s / 32 = 62.5 kHz
            chanParams->tunerParams.bwType = sdrplay_api_BW_0_200;
        }
    }

    // Configure notch filters
    chanParams->rspDuoTunerParams.rfNotchEnable = 0;
    chanParams->rspDuoTunerParams.rfDabNotchEnable = 0;
    if (engine->notchMwfm) {
        chanParams->rspDuoTunerParams.rfNotchEnable = 1;
    }
    if (engine->notchDab) {
        chanParams->rspDuoTunerParams.rfDabNotchEnable = 1;
    }

    // Set gain
    chanParams->tunerParams.gain.gRdB = 40;
    chanParams->tunerParams.gain.LNAstate = min(engine->lnaState, 9);

    // Set AGC
    chanParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
    if (engine->agcFreq == 5) {
        chanParams->ctrlParams.agc.enable = sdrplay_api_AGC_5HZ;
    }
    else if (engine->agcFreq == 50) {
        chanParams->ctrlParams.agc.enable = sdrplay_api_AGC_50HZ;
    }
    else if (engine->agcFreq == 100) {
        chanParams->ctrlParams.agc.enable = sdrplay_api_AGC_100HZ;
    }
    else if (engine->agcFreq != 0) {
        printf("invalid AGC frequency [%u], AGC disabled\n", engine->agcFreq);
    }

    if (chanParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE) {
        chanParams->ctrlParams.agc.setPoint_dBfs = min(engine->agcSetPoint, 0);
    }

    // Set decimation
    chanParams->ctrlParams.decimation.enable = 0;
    chanParams->ctrlParams.decimation.decimationFactor = 1;
    if (engine->decimFactor == 2 || engine->decimFactor == 4 ||
        engine->decimFactor == 8 || engine->decimFactor == 16 ||
        engine->decimFactor == 32) {
        chanParams->ctrlParams.decimation.enable = 1;
        chanParams->ctrlParams.decimation.decimationFactor = engine->decimFactor;
    }
    else if (engine->decimFactor != 1) {
        printf(
            "invalid decimation factor got=%u, decimation disabled\n",
            engine->decimFactor);
    }
}


/**
* Call necessary functions to configure an RSPDuo
* The getDevice() function should be called before this function.
*
* @param device pointer to sdrplay_api_DeviceT to configure
* @param engine pointer to DuoEngine configuration
*
* @return pointer to configured device parameters
*/
static sdrplay_api_DeviceParamsT* configureDevice(
        sdrplay_api_DeviceT* device, struct DuoEngine* engine) {
    sdrplay_api_ErrT err;
    sdrplay_api_DeviceParamsT* params = NULL;

    // Retrieve device parameters so they can be changed if wanted
    if ((err = sdrplay_api_GetDeviceParams(device->dev, &params)) != sdrplay_api_Success) {
        printf("sdrplay_api_GetDeviceParams failed %s\n",
               sdrplay_api_GetErrorString(err));
        return NULL;
    }

    // Check for NULL pointers before changing settings
    if (params == NULL) {
        printf("sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
        return NULL;
    }

    // Set ADC master sample rate
    if (engine->maxSampleRate) {
        params->devParams->fsFreq.fsHz = SAMPLE_FREQ_MAXFS;
    }
    else {
        params->devParams->fsFreq.fsHz = SAMPLE_FREQ_DEFAULT;
    }

    // Set USB mode
    if (engine->usbBulkMode) {
        params->devParams->mode = sdrplay_api_BULK;
    }
    else {
        params->devParams->mode = sdrplay_api_ISOCH;
    }

    // Configure both channels identically
    configureChannel(params->rxChannelA, engine);
    configureChannel(params->rxChannelB, engine);

    return params;
}


/**
* Blocking function that loops for as long as DuoEngine is running.
* Calls user-specified controlCallback() on a regular interval.
*
* @param context pointer to DuoEngine Context passed to sdrplay_api
*
* @return zero on a clean exit, non-zero otherwise
*/
static int controlLoop(struct Context* context) {
    sdrplay_api_ErrT err;
    sdrplay_api_CallbackFnsT callbacks;
    int rcode = 0;

    // Assign callback functions to be passed to sdrplay_api_Init()
    callbacks.StreamACbFn = callbackStreamA;
    callbacks.StreamBCbFn = callbackStreamB;
    callbacks.EventCbFn = callbackEvent;

    // Now we're ready to start by calling the initialisation function
    // This will configure the device and start streaming
    if ((err = sdrplay_api_Init(context->device.dev, &callbacks, context)) != sdrplay_api_Success) {
        printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
        return 1;
    }

    // Loop allowing user control in main thread
    while (true) {
        if (context->controlCallback != NULL) {
            rcode = context->controlCallback(context->userContext);
            if (rcode != 0) {
                break;
            }
        }
        Sleep(100);
    }

    // Finished with device so uninitialise it
    if ((err = sdrplay_api_Uninit(context->device.dev)) != sdrplay_api_Success) {
        printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
        return 1;
    }

    Sleep(1000);

    return 0;
}


/**
* Main function for user to call to pass control to DuoEngine.
* This is a blocking function and will run until either:
*   1. An error is encountered
*   2. The user requests an exit via controlCallback()
*
* @param engine DuoEngine configuration passed by user
*
* @return zero on clean exit, non-zero otherwise
*/
int duoEngineRun(struct DuoEngine* engine) {
    struct Context context;
    int rcode = 0;
    
    context.transfer.floatingPoint = engine->floatingPoint;
    if (engine->floatingPoint) {
        context.transfer.scalarSize = sizeof(float);
    }
    else {
        context.transfer.scalarSize = sizeof(short);
    }
    context.transfer.sampleSize = context.transfer.scalarSize * 2;
    context.transfer.frameSize = context.transfer.sampleSize * 2;

    context.transfer.numFrames = engine->maxTransferSize / context.transfer.frameSize;
    context.transfer.numSamples = context.transfer.numFrames * 2;
    context.transfer.numScalars = context.transfer.numSamples * 2;
    context.transfer.numBytes = context.transfer.numScalars * context.transfer.scalarSize;

    // Make sure the buffer size is a multiple of the transfer size
    context.bufferSize = 100 * context.transfer.numBytes;
    context.bufferLen = context.bufferSize / context.transfer.scalarSize;
    context.buffer = malloc(context.bufferSize);
 
    if (context.buffer == NULL) {
        perror("malloc failed");
        return 1;
    }

    context.numSamplesA = 0;
    context.numSamplesB = 0;
    context.rxIdx = 0;
    context.txIdx = 0;
    context.transferCallback = engine->transferCallback;
    context.controlCallback = engine->controlCallback;
    context.userContext = engine->userContext;

    rcode = openApi(engine->apiDebug);
    if (rcode == 0) {
        // Lock API while device selection is performed
        sdrplay_api_LockDeviceApi();

        rcode = getDevice(&context.device, engine->maxSampleRate);

        // Unlock API now that device is selected
        sdrplay_api_UnlockDeviceApi();

        if (rcode == 0) {
            context.params = configureDevice(&context.device, engine);
            if (context.params != NULL) {
                rcode = controlLoop(&context);
            }
            // Release device (make it available to other applications)
            sdrplay_api_ReleaseDevice(&context.device);
        }

        sdrplay_api_Close();
    }

    if (context.buffer) {
        free(context.buffer);
        context.buffer = NULL;
    }

    return rcode;
}