#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <getopt.h>
#include <sys/mman.h>
#include <fcntl.h> 

#include "commons.h"

const char *PROGRAM_NAME;

// ---------------------------------------------------------------------------------------------------------------------
// Logging

static void printStderrAndExit(const char *output, ...) {
    va_list args;
    va_start(args, output);
    vfprintf(stderr, output, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

// ---------------------------------------------------------------------------------------------------------------------
// Util

static void printUsageAndExit(void) {
    printStderrAndExit("Usage: %s EDGE1...\n", PROGRAM_NAME);
}

// ---------------------------------------------------------------------------------------------------------------------
// Main

int main(int argc, char **argv) {
    PROGRAM_NAME = argv[0];

    printUsageAndExit();
    return EXIT_SUCCESS;
}