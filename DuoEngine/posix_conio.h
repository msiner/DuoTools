/**
 Linux (POSIX) implementation of _kbhit().
 Morgan McGuire, morgan@cs.brown.edu
 */

#ifndef POSIX_CONIO_H
#define POSIX_CONIO_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/select.h>
#include <asm-generic/ioctls.h>
#include <termios.h>
#include <stropts.h>

int _kbhit() {
    static const int STDIN = 0;
    static bool initialized = false;

    if (! initialized) {
        // Use termios to turn off line buffering
        struct termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

char _getch() {
    return (char)getchar();
}

#endif
