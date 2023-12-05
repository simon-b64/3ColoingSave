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
#include <signal.h>

#include "commons.h"

typedef struct {
    long limit;
    long delay;
    bool printGraph;
} program_parameters_t;

static const char *PROGRAM_NAME;
static bool quitSignalRecieved = false;

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

static program_parameters_t parseArguments(int argc, char **argv) {
    program_parameters_t programParameters = {
        -1,
        -1,
        false,
    };
    int option;
    while ((option = getopt(argc, argv, ":n:w:p")) != -1) {
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
                if (programParameters.printGraph) {
                    fprintf(stderr, "[%s] ERROR: Multiple -p parameters were passed!\n", PROGRAM_NAME);
                    printUsageAndExit();
                }
                programParameters.printGraph = true;
                break;
            case ':':
                fprintf(stderr, "[%s] ERROR: Option -%c requires a value!\n", PROGRAM_NAME, optopt);
                printUsageAndExit();
                break;
            case '?':
            default:
                fprintf(stderr, "[%s] ERROR: Unkn#include <semaphore.h>own option: -%c\n", PROGRAM_NAME, optopt);
                printUsageAndExit();
                break;
        }
    }

    if ((argc - optind) > 0) {
        fprintf(stderr, "[%s] ERROR: Too many arguments were passed!\n", PROGRAM_NAME);
        printUsageAndExit();
    }

    return programParameters;
}

// ---------------------------------------------------------------------------------------------------------------------
// Shared memory

static void closeSHM(circular_buffer_data_t* circularBufferData) {
    if(circularBufferData != NULL) {
        if(munmap(circularBufferData, sizeof(circular_buffer_data_t)) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to unmap shared memory: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }
    
    if(shm_unlink(SHM_NAME) == -1) {
        if(errno != ENOENT) {
            printStderrAndExit("[%s] ERROR: Failed to unlink shared memory: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }
}

static circular_buffer_data_t* openSHM() {
    int sharedMemoryFd;
    if((sharedMemoryFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600)) == -1) {
        printStderrAndExit("[%s] ERROR: Failed to open shared memory: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if (ftruncate(sharedMemoryFd, sizeof(circular_buffer_data_t)) < 0) {
        fprintf(stderr, "[%s] ERROR: Failed to truncate shared memory: %s\n", PROGRAM_NAME, strerror(errno));
        closeSHM(NULL);
        exit(EXIT_FAILURE);
    }
    circular_buffer_data_t *circularBufferData;
    circularBufferData = mmap(NULL, sizeof(circular_buffer_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);

    if(circularBufferData == MAP_FAILED) {
        fprintf(stderr, "[%s] ERROR: Failed to map shared memory: %s\n", PROGRAM_NAME, strerror(errno));
        closeSHM(NULL);
        exit(EXIT_FAILURE);
    }

    if(close(sharedMemoryFd) == -1) {
        fprintf(stderr, "[%s] ERROR: Failed to close shared memory file descriptor: %s\n", PROGRAM_NAME, strerror(errno));
        closeSHM(circularBufferData);
        exit(EXIT_FAILURE);
    }

    return circularBufferData;
}

// ---------------------------------------------------------------------------------------------------------------------
// Semaphores

static void closeSEM(semaphore_colleciton_t* semaphoreCollection) {
    // TODO: This has a problem: Whe one close fails it doesnt close the other ones!!!

    if(semaphoreCollection -> rSem != NULL) {
        if(sem_close(semaphoreCollection -> rSem) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }

        if(sem_unlink(R_SEM_NAME) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to unlink semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }

    if(semaphoreCollection -> wSem != NULL) {
        if(sem_close(semaphoreCollection -> wSem) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }

        if(sem_unlink(W_SEM_NAME) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to unlink semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }

    if(semaphoreCollection -> wSyncSem != NULL) {
        if(sem_close(semaphoreCollection -> wSyncSem) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }

        if(sem_unlink(W_SEM_SYNC_NAME) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to unlink semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }
}

static semaphore_colleciton_t openSEM() {
    semaphore_colleciton_t semaphoreCollection = {
        NULL,
        NULL,
        NULL,
    };

    if((semaphoreCollection.rSem = sem_open(R_SEM_NAME, O_CREAT | O_EXCL, 0600, 0)) == SEM_FAILED) {
        printStderrAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if((semaphoreCollection.wSem = sem_open(W_SEM_NAME, O_CREAT | O_EXCL, 0600, MAX_NUM_RESULT_SETS)) == SEM_FAILED) {
        closeSEM(&semaphoreCollection);
        printStderrAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if((semaphoreCollection.wSyncSem = sem_open(W_SEM_SYNC_NAME, O_CREAT | O_EXCL, 0600, 0)) == SEM_FAILED) {
        closeSEM(&semaphoreCollection);
        printStderrAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }

    return semaphoreCollection;
}

// ---------------------------------------------------------------------------------------------------------------------
// Singnal handler

static void handleSignal(int signal) {
    if(signal == SIGINT || signal == SIGTERM) {
        quitSignalRecieved = true;
    }
}

static void registerSignalHandler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handleSignal;
    sigaction(SIGINT, &sa, NULL);
}

// ---------------------------------------------------------------------------------------------------------------------
// Main

int main(int argc, char **argv) {
    registerSignalHandler();
    PROGRAM_NAME = argv[0];

    program_parameters_t programParameters = parseArguments(argc, argv);

    circular_buffer_data_t *circularBufferData = openSHM();
    semaphore_colleciton_t semaphoreCollection = openSEM();

    if(programParameters.delay > 0) {
        sleep(programParameters.delay);
    }
    
    while(!quitSignalRecieved) {}

    closeSHM(circularBufferData);
    closeSEM(&semaphoreCollection);

    return EXIT_SUCCESS;
}
