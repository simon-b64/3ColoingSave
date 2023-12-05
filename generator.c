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
#include <time.h>

#include "commons.h"

const char *PROGRAM_NAME;
volatile sig_atomic_t quitSignalRecieved = false;

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
    if((buffer = malloc(sizeof(long*) * (argc - 1))) == NULL) {
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
// Cleanup

static void cleanup(int argc, long **edges, size_t nodesSize, long **nodes, circular_buffer_data_t *circularBufferData, semaphore_colleciton_t *semaphoreCollection) {
    if(edges != NULL) {
        for(int x = 0; x < (argc - 1); ++x) {
            free(edges[x]);
            edges[x] = NULL;
        } 
        free(edges);
        edges = NULL;
    }

    if(nodes != NULL) {
        for(int x = 0; x < nodesSize; ++x) {
            free(nodes[x]);
            nodes[x] = NULL;
        } 
        free(nodes);
        nodes = NULL;
    }
    
    closeSHM(circularBufferData);
    closeSEM(semaphoreCollection);
}

// ---------------------------------------------------------------------------------------------------------------------
// Main

int main(int argc, char **argv) {
    registerSignalHandler();
    PROGRAM_NAME = argv[0];
    srand(time(NULL));

    long **edges = parseArguments(argc, argv);

    circular_buffer_data_t *circularBufferData = openSHM();
    semaphore_colleciton_t semaphoreCollection = openSEM();

    // TODO: Debug
    for(int i = 0; i < argc - 1; i++) {
        printf("[%ld %ld]\n", edges[i][0], edges[i][1]);
    }

    long** nodes;
    size_t nodesSize = 16;
    if((nodes = malloc(sizeof(long*) * nodesSize)) == NULL) {
        cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
        printStderrAndExit("[%s] ERROR: Failed to allocate buffer: %s\n", PROGRAM_NAME, strerror(errno));
    }
    for(int i = 0; i < nodesSize; ++i) {
        nodes[i] = NULL;
    }

    for(int i = 0; i < argc - 1; ++i) {
        for(int a = 0; a < 2; ++a) {
            int x;
            for(x = 0; x < nodesSize; ++x) {
                if(nodes[x] == NULL) {
                    if((nodes[x] = malloc(sizeof(long) * 2)) == NULL) {
                        cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
                        printStderrAndExit("[%s] ERROR: Failed to allocate buffer: %s\n", PROGRAM_NAME, strerror(errno));
                    }
                    nodes[x][0] = edges[i][a];
                    nodes[x][1] = 0;
                    break;
                } else if(nodes[x][0] == edges[x][a]) {
                    break;
                }
            }
            if(x == nodesSize) {
                nodesSize *= 2;
                if((nodes = realloc(nodes, sizeof(long*) * nodesSize)) == NULL) {
                    cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
                    printStderrAndExit("[%s] ERROR: Failed to reallocate buffer: %s\n", PROGRAM_NAME, strerror(errno));
                }
                for(int y = nodesSize / 2; y < nodesSize; ++y) {
                    nodes[y] = NULL;
                }
                if((nodes[nodesSize / 2] = malloc(sizeof(long) * 2)) == NULL) {
                    cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
                    printStderrAndExit("[%s] ERROR: Failed to allocate buffer: %s\n", PROGRAM_NAME, strerror(errno));
                }
                nodes[nodesSize / 2][0] = edges[i][a];
                nodes[nodesSize / 2][1] = 0;
            }
        }
    }

    while(!quitSignalRecieved) {
        for(int i = 0; i < nodesSize; ++i) {
            if(nodes[i] != NULL) {
                nodes[i][1] = (rand() % 3) + 1;
            }
        }

        // TODO: Debug
        for(int i = 0; i < nodesSize; i++) {
            if(nodes[i] != NULL) {
                printf("Node [%ld %ld]\n", nodes[i][0], nodes[i][1]);
            }
        }

        for(int i = 0; i < (argc - 1); ++i) {
            int index1 = -1;
            int index2 = -1;
            for(int x = 0; x < nodesSize; ++x) {
                if(nodes[x] == NULL) {
                    break;
                }
                if(nodes[x][0] == edges[i][0]) {
                    index1 = x;
                }
                if(nodes[x][0] == edges[i][1]) {
                    index2 = x;
                }
                if(index1 != -1 && index2 != -1) {
                    break;
                }
            }

            if(index1 == -1 || index2 == -1) {
                cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
                printStderrAndExit("[%s] ERROR: There is an unmapped node!\n", PROGRAM_NAME);
            }

            if(nodes[index1][1] == nodes[index2][1]){
                // Write these edges to an result array
                printf("TBR: [%ld %ld]\n", edges[i][0], edges[i][1]);
            }
        }

        // Write the result array to the buffer
        
        // Continue until supervisor says to stop
        break;

    }

    cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);

    return EXIT_SUCCESS;
}