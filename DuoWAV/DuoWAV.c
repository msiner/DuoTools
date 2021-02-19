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

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <conio.h>
#include "windows_getopt.h"
#else
#include <getopt.h>
#include "posix_conio.h"
#endif

#include <stdio.h>
#include <time.h>

#define DEFAULT_AGC_BANDWIDTH (5)

#include "DuoEngine.h"
#include "DuoParse.h"
#include "wav.h"


static const char* USAGE = "\
Usage: DuoWAV.exe [-h] [-m max] [-a agchz] [-t agcdb] [-l lna] [-d decim]\n\
                  [-n notch] [-w warmup] [-o] [-f] [-k] [-x] freq bytes [path]\n\
\n\
Options:\n\
  -h: print this help message\n\
  -m max: maximum transfer size in bytes (default=10240)\n\
  -a 0|5|50|100: AGC loop bandwidth in Hz (default=5)\n\
  -t [-72-0]: AGC set point in dBFS (default=-30)\n\
  -l 0-9: LNA state where 0 provides the least RF gain reduction.\n\
      Default value is 4 (20-37 dB reduction depending on frequency).\n\
  -d 1|2|4|8|16|32: Decimation factor (default=1)\n\
      For factors 4, 8, 16, and 32, the analog bandwidth will \n\
      be reduced to 600, 300, 200, and 200 kHz respectively unless \n\
      the -x option is also specified. In which case the analog \n\
      bandwidth remains 1.536 MHz.\n\
  -n mwfm|dab: Enable MW/FM or DAB notch filter\n\
      Both filters can be enabled by providing the -n option twice\n\
      (once for each filter). By default, both filters are disabled.\n\
  -w seconds: Run the radio for the specified number of seconds to\n\
      warm up and stabilize performance before capture (default=2).\n\
      During the warmup period, samples are discarded.\n\
  -f: Convert samples to floating point\n\
  -o: Omit the WAV header. Samples will start at beginning of file.\n\
  -k: Use USB bulk transfer mode instead of isochronous\n\
  -x: Use the maximum 8 MHz master sample rate.\n\
      This will deliver 12 bit ADC resolution, but with slightly \n\
      better anti-aliaising performance at the widest bandwidth.\n\
      This mode is only available at 1.536 MHz analog bandwidth.\n\
      The default mode is to use a 6 MHz master sample clock.\n\
      That mode delivers 14 bit ADC resolution, but with slightly \n\
      inferior anti-aliaising performance at the widest bandwidth.\n\
      The default mode is also compatible with analog bandwidths of \n\
      1.536 MHz, 600 kHz, 300 kHz, and 200 kHz. 6 MHz operation \n\
      should result in a slightly lower CPU load.\n\
\n\
Arguments:\n\
  freq: Tuner RF frequency in Hz is a mandatory argument.\n\
      Can be specified with k, K, m, M, g, or G suffix to indicate\n\
      the value is in kHz, MHz, or GHz respectively (e.g. 1.42G)\n\
  bytes: Maximum output file size in bytes.\n\
      Can be specified with k, K, m, M, g, or G suffix to indicate\n\
      the value is in KiB, MiB, or GiB respectively (e.g. 10M)\n\
      NOTE: WAV files cannot exceed 4 GiB.\n\
  [path]: The destination file path (default=duo.wav)\n\
\n";


struct Context {
    FILE* out;
    size_t maxBytes;
    size_t bytesWritten;
    time_t startTime;
    bool started;
    bool done;
};


static void transferCallback(struct DuoEngineTransfer* transfer, void* userContext) {
    int rcode = 0;
    struct Context* context = (struct Context*)userContext;
    size_t numFrames = transfer->numFrames;
    size_t bytesRemaining = context->maxBytes - context->bytesWritten;
    if (bytesRemaining < transfer->numBytes) {
        numFrames = bytesRemaining / transfer->frameSize;
    }

    if (context->started && !context->done) {
        if (numFrames > 0) {
            size_t result = fwrite(transfer->data, transfer->frameSize, numFrames, context->out);
            if (result != numFrames) {
                printf("unexpected result from write expected=%zu got=%zu\n", numFrames, result);
                context->done = true;
            }
            else {
                context->bytesWritten += numFrames * transfer->frameSize;
                if (context->bytesWritten >= context->maxBytes) {
                    context->done = true;
                }
            }
        }
        else {
            context->done = true;
        }
    }
    else if (!context->started) {
        if (time(NULL) >= context->startTime) {
            context->started = true;
        }
    }
}


static int controlCallback(struct DuoEngineControl* control, void* userContext) {
    struct Context* context = (struct Context*)userContext;
    if (_kbhit()) {
        char ctrl = _getch();
        if (ctrl == 'q') {
            context->done = true;
            return 1;
        }
    }
    if (context->done) {
        return 1;
    }
    return 0;
}


static void messageCallback(const char* msg, void* userContext) {
    printf("%s\n", msg);
}


static void usage(void) {
    printf(USAGE);
}


int main(int argc, char** argv) {
    char opt = 0;
    char defaultPath[] = "duo.wav";
    char* outputPath = defaultPath;
    unsigned int warmup = 2;
    bool omitHeader = false;

    struct DuoEngine engine;
    duoEngineInit(&engine);

    struct Context context;
    context.out = NULL;
    context.maxBytes = 0;
    context.bytesWritten = 0;
    context.startTime = time(NULL);
    context.started = false;
    context.done = false;

    while ((opt = getopt(argc, argv, "ha:t:l:d:n:w:ofkx")) != -1) {
        switch (opt) {
        case 'm':
            if (parseUintArg(optarg, &engine.maxTransferSize, 10)) {
                printf("invalid max transfer size, must be an unsigned int\n");
                usage();
                return EXIT_FAILURE;
            }
            break;
        case 'a':
            if (parseAgcBandwidth(optarg, &engine.agcBandwidth)) {
                usage();
                return EXIT_FAILURE;
            }
            break;
        case 't':
            if (parseAgcSetPoint(optarg, &engine.agcSetPoint)) {
                usage();
                return EXIT_FAILURE;
            }
            break;
        case 'l':
            if (parseLnaState(optarg, &engine.lnaState)) {
                usage();
                return EXIT_FAILURE;
            }
            break;
        case 'd':
            if (parseDecimFactor(optarg, &engine.decimFactor)) {
                usage();
                return EXIT_FAILURE;
            }
            break;
        case 'n':
            if (parseNotchFilter(optarg, &engine.notchMwfm, &engine.notchDab)) {
                usage();
                return EXIT_FAILURE;
            }
            break;
        case 'w':
            if (parseUintArg(optarg, &warmup, 10)) {
                printf("invalid warmup time, must be an unsigned int\n");
                usage();
                return EXIT_FAILURE;
            }
            break; 
        case 'o':
            omitHeader = true;
            break;
        case 'f':
            engine.floatingPoint = true;
            break;
        case 'k':
            engine.usbBulkMode = true;
            break;
        case 'x':
            engine.maxSampleRate = true;
            break;
        case 'h':
            usage();
            return EXIT_SUCCESS;
        default:
            printf("unrecognized option\n");
            usage();
            return EXIT_FAILURE;
        }
    }

    // Handle remaining positional arguments
    if (optind == (argc - 2) || optind == (argc - 3)) {
        if (parseFrequency(argv[optind], &engine.tuneFreq)) {
            printf("invalid frequency argument\n");
            usage();
            return EXIT_FAILURE;
        }
        if (parseSize(argv[optind + 1], &context.maxBytes)) {
            printf("invalid size argument\n");
            usage();
            return EXIT_FAILURE;
        }
        if (context.maxBytes > UINT32_MAX) {
            printf("WAV file only supports file sizes <= 4 GiB\n");
            usage();
            return EXIT_FAILURE;
        }
        if (optind == (argc - 3)) {
            outputPath = argv[optind + 2];
        }
    }
    else {
        printf("invalid number of arguments\n");
        usage();
        return EXIT_FAILURE;
    }

    printf("Output file: %s\n", outputPath);
    printf("Maximum Bytes: %zu\n", context.maxBytes);
    printf("Omit WAV header: %s\n", omitHeader ? "true" : "false");
    if (!omitHeader) {
        printf("WAV header size: %zu bytes\n", sizeof(struct WavHeader));
    }
    printf("Warmup: %u seconds\n", warmup);
    printf("RF Tune Frequency: %f Hz\n", engine.tuneFreq);
    printf("AGC Loop Bandwidth: %u Hz\n", engine.agcBandwidth);
    if (engine.agcBandwidth > 0) {
        printf("AGC Set Point: %d dBFS\n", engine.agcSetPoint);
    }
    printf("LNA State: %u\n", engine.lnaState);
    printf("Decimation Factor: %u\n", engine.decimFactor);
    printf("Floating Point: %s\n", engine.floatingPoint ? "true" : "false");
    printf("USB Bulk Mode: %s\n", engine.usbBulkMode ? "true" : "false");
    printf("Max Fs Mode: %s\n", engine.maxSampleRate ? "true" : "false");

    // Prepare the WAV header metadata
    struct WavHeader wav;
    uint8_t bytesPerSample = sizeof(short);
    bool floatingPoint = false;
    if (engine.floatingPoint) {
        bytesPerSample = sizeof(float);
        floatingPoint = true;
    }
    wavHeaderInit(
        &wav,
        2000000 / engine.decimFactor, // sample rate
        4, // num channels, one for each scalar: Ia Qa Ib Qb
        bytesPerSample,
        floatingPoint);

    // Open output file
    // Need the "b" binary option to avoid translations
 #if defined(_WIN32) || defined(_WIN64)
    errno_t err = fopen_s(&context.out, outputPath, "wb");
    if (err != 0) {
        printf("failed to open file rcode=%d\n", err);
        return EXIT_FAILURE;
    }
 #else
    context.out = fopen(outputPath, "wb");
    if (context.out == NULL) {
        perror("failed to open file");
	return EXIT_FAILURE;
    }
 #endif

    if (!omitHeader) {
        // Write WAV header, file position will be at start of data portion
        size_t result = fwrite(&wav, sizeof(wav), 1, context.out);
        if (result != 1) {
            printf("failed to write wav header result=%zu\n", result);
            return EXIT_FAILURE;
        }

        // Max bytes is the entire file size,
        // need to allocate the size taken by the WAV header.
        context.maxBytes -= sizeof(wav);
    }

    // Configure callbacks
    engine.userContext = &context;
    engine.transferCallback = transferCallback;
    engine.controlCallback = controlCallback;
    engine.messageCallback = messageCallback;

    // Configure warmup start time if needed
    if (warmup == 0) {
        context.started = true;
    }
    else {
        context.started = false;
        context.startTime = time(NULL) + warmup;
    }

    printf("PRESS q to QUIT\n");
    int rcode = duoEngineRun(&engine);

    if (!omitHeader) {
        // Need to update the file and data size values in the header
        // and overwrite the old header
        wavHeaderUpdate(&wav, (uint32_t)context.bytesWritten);
        fseek(context.out, 0, SEEK_SET);
        size_t result = fwrite(&wav, sizeof(wav), 1, context.out);
        if (result != 1) {
            printf("failed to update wav header result=%zu\n", result);
            return EXIT_FAILURE;
        }
    }

    fclose(context.out);

    if (rcode != 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
