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
#include <signal.h>

#include "commons.h"

const char *PROGRAM_NAME;
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
    printStderrAndExit("Usage: %s EDGE1...\nEdges: {node1}-{node2}", PROGRAM_NAME);
}

// ---------------------------------------------------------------------------------------------------------------------
// Argument parsing

static long** parseArguments(int argc, char **argv) {
    if(argc <= 1) {
        printUsageAndExit();
    }

    long **buffer;
    if((buffer = malloc(sizeof(long) * (argc - 1))) == NULL) {
        printStderrAndExit("[%s] ERROR: Failed to allocate buffer: %s\n", PROGRAM_NAME, strerror(errno));
    }
    for(int i = 0; i < (argc - 1); ++i) {
        if((buffer[i] = malloc(sizeof(long) * 2)) == NULL) {
            for(int x = 0; x < (argc - 1); ++x) {
                free(buffer[x]);
            } 
            free(buffer);
            printStderrAndExit("[%s] ERROR: Failed to allocate buffer: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }

    for(int i = 1; i < argc; ++i) {
        char *numb = strtok(argv[i], "-");
        if(numb == NULL) {
            for(int x = 0; x < (argc - 1); ++x) {
                free(buffer[x]);
            } 
            free(buffer);
            printStderrAndExit("[%s] ERROR: Could not parse edge %d: %s\n", PROGRAM_NAME, i, argv[i]);
        }
        char *endptr = NULL;
        buffer[i - 1][0] = strtol(numb, &endptr, 10);
        if(buffer[i - 1][0] == LONG_MIN || buffer[i - 1][0] == LONG_MAX) {
            if(errno == ERANGE) {
                for(int x = 0; x < (argc - 1); ++x) {
                    free(buffer[x]);
                } 
                free(buffer);
                printStderrAndExit("[%s] ERROR: Converting long failed: %s\n", PROGRAM_NAME, strerror(errno));
            }
        }
        if (endptr == optarg) {
            for(int x = 0; x < (argc - 1); ++x) {
                free(buffer[x]);
            } 
            free(buffer);
            printStderrAndExit("[%s] ERROR: No digits were found in the first node: %s\n", PROGRAM_NAME, errno);
        }

        numb = strtok(NULL, "-");
        if(numb == NULL) {
            for(int x = 0; x < (argc - 1); ++x) {
                free(buffer[x]);
            } 
            free(buffer);
            printStderrAndExit("[%s] ERROR: Could not parse edge %d: %s\n", PROGRAM_NAME, i, argv[i]);
        }
        endptr = NULL;
        buffer[i - 1][1] = strtol(numb, &endptr, 10);
        if(buffer[i - 1][1] == LONG_MIN || buffer[i - 1][1] == LONG_MAX) {
            if(errno == ERANGE) {
                for(int x = 0; x < (argc - 1); ++x) {
                    free(buffer[x]);
                } 
                free(buffer);
                printStderrAndExit("[%s] ERROR: Converting long failed: %s\n", PROGRAM_NAME, strerror(errno));
            }
        }
        if (endptr == optarg) {
            for(int x = 0; x < (argc - 1); ++x) {
                free(buffer[x]);
            } 
            free(buffer);
            printStderrAndExit("[%s] ERROR: No digits were found in the first node: %s\n", PROGRAM_NAME, errno);
        }
    }
    
    return buffer;
}

// ---------------------------------------------------------------------------------------------------------------------
// Shared memory

static void closeSHM(circular_buffer_data_t* circularBufferData) {
    if(circularBufferData != NULL) {
        if(munmap(circularBufferData, sizeof(circular_buffer_data_t)) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to unmap shared memory: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }
}

static circular_buffer_data_t* openSHM() {
    int sharedMemoryFd;
    if((sharedMemoryFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600)) == -1) {
        printStderrAndExit("[%s] ERROR: Failed to open shared memory: %s\n", PROGRAM_NAME, strerror(errno));
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
    }

    if(semaphoreCollection -> wSem != NULL) {
        if(sem_close(semaphoreCollection -> wSem) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }

    if(semaphoreCollection -> wSyncSem != NULL) {
        if(sem_close(semaphoreCollection -> wSyncSem) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }
}

static semaphore_colleciton_t openSEM() {
    semaphore_colleciton_t semaphoreCollection = {
        NULL,
        NULL,
        NULL,
    };

    if((semaphoreCollection.rSem = sem_open(R_SEM_NAME, 0)) == SEM_FAILED) {
        printStderrAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if((semaphoreCollection.wSem = sem_open(W_SEM_NAME, 0)) == SEM_FAILED) {
        closeSEM(&semaphoreCollection);
        printStderrAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if((semaphoreCollection.wSyncSem = sem_open(W_SEM_SYNC_NAME, 0)) == SEM_FAILED) {
        closeSEM(&semaphoreCollection);
        printStderrAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }

    return semaphoreCollection;
}

// ---------------------------------------------------------------------------------------------------------------------
// Singnal handler

static void handleSignal(int signal) {
    quitSignalRecieved = true;
}

static void registerSignalHandler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handleSignal;
    // TODO: Check if you should realy handle both
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

// ---------------------------------------------------------------------------------------------------------------------
// Main

int main(int argc, char **argv) {
    registerSignalHandler();
    PROGRAM_NAME = argv[0];

    long **edges = parseArguments(argc, argv);

    circular_buffer_data_t *circularBufferData = openSHM();
    semaphore_colleciton_t semaphoreCollection = openSEM();

    for(int i = 0; i < argc - 1; i++) {
        printf("[%ld %ld]\n", edges[i][0], edges[i][1]);
    }
    // Insert Program logic

    while(!quitSignalRecieved) {}

    closeSHM(circularBufferData);
    closeSEM(&semaphoreCollection);

    for(int x = 0; x < (argc - 1); ++x) {
        free(edges[x]);
    } 
    free(edges);

    return EXIT_SUCCESS;
}