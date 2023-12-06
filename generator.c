/**
 * @file generator.c
 * @author Simon Buchinger 12220026 <e12220026@student.tuwien.ac.at>
 * @date 27.11.2023
 * @program: 3coloring
 * 
 * @brief Main-file of the generator program
 * @details The generator program will try to open the already initialised shared memory and semaphores
 *          and then tries to find a 3coloring solution to the giphen graph. If it finds a solution it writes
 *          it to the circular buffer inside of the shared memory space.
 *
 **/

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

/**
 * Program name
 * @brief Pointer to the program name string
 */
const char *PROGRAM_NAME;

/**
 * Quit signal recieved
 * @brief Boolean to store if a quit singnal was recieved. Has to be completely asynchronous save.
 */
volatile sig_atomic_t quitSignalRecieved = false;

// ---------------------------------------------------------------------------------------------------------------------
// Logging

/**
 * Loggin function for errors
 * 
 * @brief This function writes a given formatted message to stderr and exits with EXIT_FAILURE
 * 
 * @param output Formatted output string
 * @param ... Fomat elements
 */
static void printStderrAndExit(const char *output, ...) {
    va_list args;
    va_start(args, output);
    vfprintf(stderr, output, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

// ---------------------------------------------------------------------------------------------------------------------
// Util

/**
 * Mandatory usage function
 * 
 * @brief This function writes helpful usage information about the program to stderr and exits with EXIT_FAILURE
 * @details global variables: PROGRAM_NAME 
 */
static void printUsageAndExit(void) {
    printStderrAndExit("Usage: %s EDGE1...\nEdges: {node1}-{node2}", PROGRAM_NAME);
}

// ---------------------------------------------------------------------------------------------------------------------
// Argument parsing

/**
 * Parse arguments function
 * 
 * @brief This function parses the arguments given to the program via argc and argv. If something is not right it prints an eror message and exits with EXIT_FAILURE
 * @details global variables: PROGRAM_NAME
 * 
 * @param argc The argument counter
 * @param argv The argument vector
 * @return A pointer to an allocated two dimensional array of the edges in the graph
 */
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

/**
 * @brief Umaps the shared memory circular buffer
 * @details global variables: PROGRAM_NAME
 * 
 * @param circularBufferData pointer to the mapped shared memory circular buffer
 */
static void closeSHM(circular_buffer_data_t* circularBufferData) {
    if(circularBufferData != NULL) {
        if(munmap(circularBufferData, sizeof(circular_buffer_data_t)) == -1) {
            printStderrAndExit("[%s] ERROR: Failed to unmap shared memory: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }
}

/**
 * @brief Opens a shared memory space, maps it to a variable, closes the fileDescriptor and returns a pointer to that object.
 *        If something fails it closes already open resources and outputs an error.
 * @details global variables: PROGRAM_NAME
 * 
 * @return A pointer to the mapped shared memory circular buffer
 */
static circular_buffer_data_t* openSHM(void) {
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

/**
 * @brief Closes the open semaphores given with semaphoreCollection
 * @details global variables: PROGRAM_NAME
 * 
 * @param semaphoreCollection pointer to a semaphore_collection_t containing the semaphores to close
 */
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

/**
 * @brief Opens all neccessary semaphores and returns a collection of these semaphores as a semphore_collection_t object.
 *        If something fails it automatically closes the semaphores and prints an error.
 * 
 * @return A collection of the opened semaphores as a semphore_collection_t object.
 */
static semaphore_colleciton_t openSEM(void) {
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

/**
 * @brief Function to handle singals
 * 
 * @param signal Signal that is being handles
 */
static void handleSignal(int signal) {
    quitSignalRecieved = true;
}

/**
 * @brief Registers the signal handler for SIGINT and SIGTERM
 * 
 */
static void registerSignalHandler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handleSignal;
    // TODO: Check if you should realy handle both
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

// ---------------------------------------------------------------------------------------------------------------------
// Cleanup

/**
 * @brief Cleans up everything there is to clean up.
 *        It frees allocated memory and closes the shared memory as well as the semaphores
 * 
 * @param argc The argument count
 * @param edges A pointer to an allocated two dimensional array of the edges in the graph
 * @param nodesSize The size of the nodes array
 * @param nodes A pointer to an allocated two dimensional array of the nodes together with their color
 * @param circularBufferData pointer to the mapped shared memory circular buffer
 * @param semaphoreCollection pointer to a semaphore_collection_t containing the semaphores to close
 */
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

/**
 * @brief Program entry point
 * 
 * @param argc The argument counter
 * @param argv The argument vector
 * @return Returns EXIT_SUCCESS on program success
 */
int main(int argc, char **argv) {
    registerSignalHandler();
    PROGRAM_NAME = argv[0];
    srand(time(NULL));

    long **edges = parseArguments(argc, argv);

    circular_buffer_data_t *circularBufferData = openSHM();
    semaphore_colleciton_t semaphoreCollection = openSEM();

    // Initialise the nodes array to contain all nodes and colors
    long** nodes;
    size_t nodesSize = 16;
    if((nodes = malloc(sizeof(long*) * nodesSize)) == NULL) {
        cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
        printStderrAndExit("[%s] ERROR: Failed to allocate buffer: %s\n", PROGRAM_NAME, strerror(errno));
    }
    for(int i = 0; i < nodesSize; ++i) {
        nodes[i] = NULL;
    }

    // Extract all node into the nodes array
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
                } else if(nodes[x][0] == edges[i][a]) {
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

    while(!quitSignalRecieved && !circularBufferData -> stopGenerators) {
        // Generate a random coloring
        for(int i = 0; i < nodesSize; ++i) {
            if(nodes[i] != NULL) {
                nodes[i][1] = (rand() % 3) + 1;
            }
        }

        // Generate a buffer in which to write the edges to remove
        long edgesToRemove[MAX_NUM_EDGES_RESULT_SET][2];
        for(int i = 0; i < MAX_NUM_EDGES_RESULT_SET; ++i) {
            edgesToRemove[i][0] = -1;
            edgesToRemove[i][1] = -1;
        }

        // Find out which edges to remove and add them to buffer
        int edgeIndex = 0;
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
                // ERROR out since this should never happen
                cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
                printStderrAndExit("[%s] ERROR: There is an unmapped node!\n", PROGRAM_NAME);
            }

            if(nodes[index1][1] == nodes[index2][1]){
                if(edgeIndex < MAX_NUM_EDGES_RESULT_SET) {
                    edgesToRemove[edgeIndex][0] = edges[i][0];
                    edgesToRemove[edgeIndex][1] = edges[i][1];
                    ++edgeIndex;
                }
            }
        }

        // Continue searching since the result is too large
        if(edgeIndex >= MAX_NUM_EDGES_RESULT_SET) {
            continue;
        }
        
        if(sem_wait(semaphoreCollection.wSyncSem) == -1) {
            if(errno == EINTR) {
                continue;
            }
            cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
            printStderrAndExit("[%s] ERROR: There was an error waiting the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
        }

        if(sem_wait(semaphoreCollection.wSem) == -1) {
            if(sem_post(semaphoreCollection.wSyncSem) == -1) {
                cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
                printStderrAndExit("[%s] ERROR: There was an error pushing the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
            }
            if(errno == EINTR) {   
                continue;
            }
            cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
            printStderrAndExit("[%s] ERROR: There was an error waiting the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
        }

        for(int i = 0; i < MAX_NUM_EDGES_RESULT_SET; ++i) {
            circularBufferData -> resultSets[circularBufferData -> writePos][i][0] = edgesToRemove[i][0];
            circularBufferData -> resultSets[circularBufferData -> writePos][i][1] = edgesToRemove[i][1];
        }

        circularBufferData -> writePos = circularBufferData -> writePos + 1;
        circularBufferData -> writePos = circularBufferData -> writePos % MAX_NUM_RESULT_SETS;

        if(sem_post(semaphoreCollection.rSem) == -1) {
            cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
            printStderrAndExit("[%s] ERROR: There was an error pushing the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
        }

        if(sem_post(semaphoreCollection.wSyncSem) == -1) {
            cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);
            printStderrAndExit("[%s] ERROR: There was an error pushing the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
        }
    }

    cleanup(argc, edges, nodesSize, nodes, circularBufferData, &semaphoreCollection);

    return EXIT_SUCCESS;
}