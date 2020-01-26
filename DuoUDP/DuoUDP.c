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
#include <winsock.h>
#include <wsipv6ok.h>
#include <conio.h>

#include <stdio.h>


#include "getopt.h"
#include "DuoEngine.h"
#include "DuoParse.h"


static const char* USAGE = "\
Usage: DuoUDP.exe [-h] [-m mtu] [-a agchz] [-t agcdb] [-l lna] [-d decim]\n\
                  [-n notch] [-k] [-x] freq [[ipaddr][:port]]\n\
\n\
Options:\n\
  -h: print this help message\n\
  -m mtu: packet MTU (default=1500)\n\
  -a 0|5|50|100: AGC frequency in Hz (default=0)\n\
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
  -f: Convert samples to floating-point\n\
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
  [ipaddr][:port]: The destination IPv4 address and UDP port can optionally\n\
      be specified (default=127.0.0.1:1234). One or both can be specified and\n\
      the default of the unspecified value will be used.\n\
\n";


struct Context {
    SOCKET sock;
    struct sockaddr_in dest;
};


static void transferCallback(struct DuoEngineTransfer* transfer, void* userContext) {
    int rcode = 0;
    struct Context* context = (struct Context*)userContext;
    rcode = sendto(
        context->sock, (char*)transfer->data, transfer->numBytes, 0,
        (struct sockaddr*)&context->dest, sizeof(context->dest));
    if (rcode == SOCKET_ERROR) {
        printf("sendto failed with error=%d\n", WSAGetLastError());
    }
}


static int controlCallback(void* userContext) {
    if (_kbhit()) {
        char ctrl = _getch();
        if (ctrl == 'q') {
            return 1;
        }
    }
    return 0;
}



/**
* Parsing function for [addr][:port] arguments.
* Can accept
*   1. IP address only with no semicolon (e.g. "192.168.1.1")
*   2. IP address and port separated by semicolon (e.g. "192.168.1.1:8080")
*   3. Port only with leading semicolon (e.g. ":8080")
*
* @param arg pointer to null-terminated string
* @param ipStr pointer location to store pointer to IP address string
*/
static int parseAddrPort(
        char* arg, char** ipStr, unsigned long* ipAddr, unsigned int* port) {
    int sepIdx = -1;
    int rcode = 1;
    unsigned long tmpAddr;
    unsigned int tmpPort;
    size_t argLen = strlen(arg);
    for (unsigned int charIdx = 0; charIdx < argLen; charIdx++) {
        if (arg[charIdx] == ':') {
            sepIdx = charIdx;
            break;
        }
    }
    if (sepIdx == 0 && argLen > 1) {
        // Starts with separator, must be just a port
        if (parseUintArg(&arg[1], &tmpPort, 10)) {
            return 1;
        }
        if (tmpPort > 65535) {
            printf("invalid UDP port [%u], must be in [0-65535]\n", tmpPort);
            return 1;
        }
        *port = tmpPort;
        return 0;
    }
    else if (sepIdx == -1) {
        // No separator, must be just an address
        tmpAddr = inet_addr(arg);
        if (tmpAddr == INADDR_NONE) {
            printf("invalid IPv4 address value [%s]\n", arg);
            return 1;
        }
        *ipAddr = tmpAddr;
        *ipStr = arg;
        return 0;
    }
    else if (argLen > 3 && sepIdx != (argLen - 1)) {
        // We found the separator somewhere in the middle
        // Insert a null-termination where the ':' used to be
        // This effectively separates arg into two C strings
        arg[sepIdx] = 0;
        tmpAddr = inet_addr(arg);
        if (tmpAddr == INADDR_NONE) {
            printf("invalid IPv4 address value [%s]\n", arg);
            return 1;
        }
        if (parseUintArg(&arg[sepIdx + 1], &tmpPort, 10)) {
            return 1;
        }
        if (tmpPort > 65535) {
            printf("invalid UDP port [%u], must be in [0-65535]\n", tmpPort);
            return 1;
        }
        *port = tmpPort;
        *ipAddr = tmpAddr;
        *ipStr = arg;
        return 0;
    }
    printf("invalid address and port specification [%s] "
           "(expect [addr][:port])\n", arg);
    return 1;
}


static void usage(void) {
    printf(USAGE);
}


int main(int argc, char** argv) {
    char opt = 0;
    unsigned int port = 1234;
    char defaultAddr[] = "127.0.0.1";
    char* ipStr = defaultAddr;
    unsigned long ipAddr = inet_addr(defaultAddr);
    unsigned int mtu = 1500;

    struct DuoEngine engine;
    duoEngineInit(&engine);

    WSADATA wsaData;
    struct Context context;
    int rcode = 0;

    while ((opt = getopt(argc, argv, "hm:a:t:l:d:n:fkx")) != -1) {
        switch (opt) {
        case 'm':
            if (parseUintArg(optarg, &mtu, 10)) {
                printf("invalid MTU, must be an unsigned int\n");
                usage();
                return EXIT_FAILURE;
            }
            break;
        case 'a':
            if (parseAgcFreq(optarg, &engine.agcFreq)) {
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
    if (optind == (argc - 1) || optind == (argc - 2)) {
        if (parseFrequency(argv[optind], &engine.tuneFreq)) {
            printf("invalid frequency argument\n");
            usage();
            return EXIT_FAILURE;
        }
        if (optind == (argc - 2)) {
            if (parseAddrPort(argv[optind + 1], &ipStr, &ipAddr, &port)) {
                usage();
                return EXIT_FAILURE;
            }
        }
    }
    else {
        printf("invalid number of arguments\n");
        usage();
        return EXIT_FAILURE;
    }

    printf("Destination IP Address: %s\n", ipStr);
    printf("Destination UDP Port: %u\n", port);
    printf("RF Tune Frequency: %f Hz\n", engine.tuneFreq);
    printf("Packet MTU: %u bytes\n", mtu);
    printf("AGC Frequency: %u Hz\n", engine.agcFreq);
    if (engine.agcFreq > 0) {
        printf("AGC Set Point: %d dBFS\n", engine.agcSetPoint);
    }
    printf("LNA State: %u\n", engine.lnaState);
    printf("Decimation Factor: %u\n", engine.decimFactor);
    printf("Floating Point: %s\n", engine.floatingPoint ? "true" : "false");
    printf("USB Bulk Mode: %s\n", engine.usbBulkMode ? "true" : "false");
    printf("Max Fs Mode: %s\n", engine.maxSampleRate ? "true" : "false");

    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        printf("WSAStartup() failed");
        return EXIT_FAILURE;
    }

    if ((context.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("socket creation failed error=%u", WSAGetLastError());
        return EXIT_FAILURE;
    }

    memset(&context.dest, 0, sizeof(context.dest));

    context.dest.sin_family = AF_INET;
    context.dest.sin_addr.s_addr = ipAddr;
    context.dest.sin_port = htons((unsigned short)port);

    // subtract IP and UDP headers
    engine.maxTransferSize = mtu - 20 - 8;

    // Configure callbacks
    engine.userContext = &context;
    engine.transferCallback = transferCallback;
    engine.controlCallback = controlCallback;

    printf("PRESS q to QUIT\n");
    rcode = duoEngineRun(&engine);

    closesocket(context.sock);
    WSACleanup();

    if (rcode != 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}