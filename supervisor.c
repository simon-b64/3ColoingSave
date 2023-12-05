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
#include <sys/stat.h>
#include <fcntl.h> 

// TODO: Change this prefix
#define MAT_NUMMER_PREFIX "XXXX_"
#define SHM_NAME MAT_NUMMER_PREFIX "SHM"
#define R_SEM_NAME MAT_NUMMER_PREFIX "R_SEM"
#define W_SEM_NAME MAT_NUMMER_PREFIX "W_SEM"
#define W_SEM_SYNC_NAME MAT_NUMMER_PREFIX "W_SEM_SYNC"

typedef struct {
    long limit;
    long delay;
    bool p;
} program_parameters_t;

const char *PROGRAM_NAME;

static program_parameters_t programParameters = {
        -1,
        -1,
        false,
};

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
    printStderrAndExit("Usage: %s [-n limit] [-w delay] [-p]\n", PROGRAM_NAME);
}

// ---------------------------------------------------------------------------------------------------------------------
// Argument parsing

static void parseArguments(int argc, char **argv) {
    int option;
    while ((option = getopt(argc, argv, ":n:w:p ")) != -1) {
        switch (option) {
            case 'n':
                if (programParameters.limit != -1) {
                    fprintf(stderr, "[%s] ERROR: multiple limit parameters were passed!\n", PROGRAM_NAME);
                    printUsageAndExit();
                }
                // maybe move out of this scope to not have 1 & 2
                char *endptr1;
                programParameters.limit = strtol(optarg, &endptr1, 10);
                if(programParameters.limit == LONG_MIN || programParameters.limit == LONG_MAX) {
                    if(errno == ERANGE) {
                        printStderrAndExit("[%s] ERROR: Converting integer failed: %s\n", PROGRAM_NAME, strerror(errno));
                    }
                }
                if (endptr1 == optarg) {
                    printStderrAndExit("[%s] ERROR: No digits were found in the input string for limit!\n", PROGRAM_NAME);
                }
                if(programParameters.limit < 0) {
                    fprintf(stderr, "[%s] ERROR: Limit cannot be smaller than 0!\n", PROGRAM_NAME);
                    printUsageAndExit();
                }
                break;
            case 'w':
                if (programParameters.delay != -1) {
                    fprintf(stderr, "[%s] ERROR: multiple wait parameters were passed!\n", PROGRAM_NAME);
                    printUsageAndExit();
                }
                char *endptr2;
                programParameters.delay = strtol(optarg, &endptr2, 10);
                if(programParameters.delay == LONG_MIN || programParameters.delay == LONG_MAX) {
                    if(errno == ERANGE) {
                        printStderrAndExit("[%s] ERROR: Converting integer failed: %s\n", PROGRAM_NAME, strerror(errno));
                    }
                }
                if (endptr2 == optarg) {
                    printStderrAndExit("[%s] ERROR: No digits were found in the input string for wait!\n", PROGRAM_NAME);
                }
                if(programParameters.delay < 0) {
                    fprintf(stderr, "[%s] ERROR: Delay cannot be smaller than 0!\n", PROGRAM_NAME);
                    printUsageAndExit();
                }
                break;
            case 'p':
                if (programParameters.p) {
                    fprintf(stderr, "[%s] ERROR: multiple -p parameters were passed!\n", PROGRAM_NAME);
                    printUsageAndExit();
                }
                programParameters.p = true;
                break;
            case ':':
                fprintf(stderr, "[%s] ERROR: Option -%c requires a value!\n", PROGRAM_NAME, optopt);
                printUsageAndExit();
                break;
            case '?':
            default:
                fprintf(stderr, "[%s] ERROR: Unknown option: %c\n", PROGRAM_NAME, optopt);
                printUsageAndExit();
                break;
        }
    }

    if ((argc - optind) > 0) {
        fprintf(stderr, "[%s] ERROR: Too many arguments were passed!\n", PROGRAM_NAME);
        printUsageAndExit();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Shared memory and semaphores

static void openSHMandSEM() {
    
}

static void closeSHMandSEM() {

}

// ---------------------------------------------------------------------------------------------------------------------
// Main

int main(int argc, char **argv) {
    PROGRAM_NAME = argv[0];
    parseArguments(argc, argv);

    // Initialise shared memory, semaphores and cricular buffer

    if(programParameters.delay > 0) {
        sleep(programParameters.delay);
    }


    return EXIT_SUCCESS;
}
