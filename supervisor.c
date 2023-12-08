/**
 * @file supervisor.c
 * @author Simon Buchinger 12220026 <e12220026@student.tuwien.ac.at>
 * @date 27.11.2023
 * @program: 3coloring
 * 
 * @brief Main-file of the supervisor program
 * @details The supervisor will open a circular buffer in shared memory and three semaphores,
 *          enabling the generators to write their solutions to the three coloring problem into the buffer.
 *          The supervisor will read these solutions until a perfect one is found or a quit condition is reached.
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
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "commons.h"

/**
 * Program parameters struct
 * @brief Stores all parameters passed to the program
 */
typedef struct {
    long limit;
    long delay;
    bool printGraph;
} program_parameters_t;

/**
 * Program name
 * @brief Pointer to the program name string
 */
static const char *PROGRAM_NAME;

/**
 * Quit signal recieved
 * @brief Boolean to store if a quit singnal was recieved. Has to be completely asynchronous save.
 */
volatile sig_atomic_t quitSignalRecieved = false;

// TODO:
// Write useful coments
// Doxygen documentation
// Check if you reinvent the wheel somewhere
// NAME OF CONSTANTS IN UPPERCASE
// Use meaningful variable and constant names

circular_buffer_data_t *circularBufferData = NULL;
semaphore_colleciton_t semaphoreCollection = {
    NULL,
    NULL,
    NULL,
};

static void cleanup(void);

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
static void printStderrCleaupAndExit(const char *output, ...) {
    va_list args;
    va_start(args, output);
    vfprintf(stderr, output, args);
    va_end(args);
    cleanup();
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
    printStderrCleaupAndExit("Usage: %s [-n limit] [-w delay] [-p]\n", PROGRAM_NAME);
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
 * @return An initialised program_parametes_t struct with the parsed argument values
 */
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
                        printStderrCleaupAndExit("[%s] ERROR: Converting integer failed: %s\n", PROGRAM_NAME, strerror(errno));
                    }
                }
                if (endptr1 == optarg) {
                    fprintf(stderr, "[%s] ERROR: No digits were found in the input string for limit!\n", PROGRAM_NAME);
                    printUsageAndExit();
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
                        printStderrCleaupAndExit("[%s] ERROR: Converting integer failed: %s\n", PROGRAM_NAME, strerror(errno));
                    }
                }
                if (endptr2 == optarg) {
                    fprintf(stderr, "[%s] ERROR: No digits were found in the input string for wait!\n", PROGRAM_NAME);
                    printUsageAndExit();
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
                fprintf(stderr, "[%s] ERROR: Unknown option: -%c\n", PROGRAM_NAME, optopt);
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

/**
 * @brief Umaps the shared memory circular buffer and unlinks the shared memory
 * @details global variables: PROGRAM_NAME
 * 
 * @param circularBufferData pointer to the mapped shared memory circular buffer
 */
static int closeSHM(void) {
    int returnValue = 0;

    if(circularBufferData != NULL) {
        if(munmap(circularBufferData, sizeof(circular_buffer_data_t)) == -1) {
            fprintf(stderr, "[%s] ERROR: Failed to unmap shared memory: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
        circularBufferData = NULL;
    }
    
    if(shm_unlink(SHM_NAME) == -1) {
        if(errno != ENOENT) {
            fprintf(stderr, "[%s] ERROR: Failed to unlink shared memory: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
    }

    return returnValue;
}

/**
 * @brief Opens a shared memory space, trucates it to the size of the circular_buffer_data_t object, 
 *        maps it to a variable, closes the fileDescriptor, intialises the circular buffer object and returns a pointer to that object.
 *        If something fails it closes already open resources and outputs an error.
 * @details global variables: PROGRAM_NAME
 * 
 * @return A pointer to the mapped shared memory circular buffer
 */
static void openSHM(void) {
    if(circularBufferData != NULL) {
        return;
    }

    int sharedMemoryFd;
    if((sharedMemoryFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600)) == -1) {
        printStderrCleaupAndExit("[%s] ERROR: Failed to open shared memory: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if (ftruncate(sharedMemoryFd, sizeof(circular_buffer_data_t)) < 0) {
        printStderrCleaupAndExit("[%s] ERROR: Failed to truncate shared memory: %s\n", PROGRAM_NAME, strerror(errno));
    }

    circularBufferData = mmap(NULL, sizeof(circular_buffer_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryFd, 0);

    if(circularBufferData == MAP_FAILED) {
        printStderrCleaupAndExit("[%s] ERROR: Failed to map shared memory: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if(close(sharedMemoryFd) == -1) {
        printStderrCleaupAndExit("[%s] ERROR: Failed to close shared memory file descriptor: %s\n", PROGRAM_NAME, strerror(errno));
    }

    circularBufferData -> readPos = 0;
    circularBufferData -> writePos = 0;
    circularBufferData -> stopGenerators = false;
    for(int i = 0; i < MAX_NUM_RESULT_SETS; ++i) {
        for(int x = 0; x < MAX_NUM_EDGES_RESULT_SET; ++x) {
            circularBufferData -> resultSets[i][x][0] = -1;
            circularBufferData -> resultSets[i][x][1] = -1;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Semaphores

/**
 * @brief Closes and unlinks the open semaphores given with semaphoreCollection
 * @details global variables: PROGRAM_NAME
 * 
 * @param semaphoreCollection pointer to a semaphore_collection_t containing the semaphores to close
 */
static int closeSEM(void) {
    int returnValue = 0;

    if(semaphoreCollection.rSem != NULL) {
        if(sem_close(semaphoreCollection.rSem) == -1) {
            fprintf(stderr, "[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
        semaphoreCollection.rSem = NULL;

        if(sem_unlink(R_SEM_NAME) == -1) {
            fprintf(stderr, "[%s] ERROR: Failed to unlink semaphores: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
    }
    semaphoreCollection.rSem = NULL;

    if(semaphoreCollection.wSem != NULL) {
        if(sem_close(semaphoreCollection.wSem) == -1) {
            fprintf(stderr, "[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
        semaphoreCollection.wSem = NULL;

        if(sem_unlink(W_SEM_NAME) == -1) {
            fprintf(stderr, "[%s] ERROR: Failed to unlink semaphores: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
    }
    semaphoreCollection.wSem = NULL;

    if(semaphoreCollection.wSyncSem != NULL) {
        if(sem_close(semaphoreCollection.wSyncSem) == -1) {
            fprintf(stderr, "[%s] ERROR: Failed to close semaphores: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
        semaphoreCollection.wSyncSem = NULL;

        if(sem_unlink(W_SEM_SYNC_NAME) == -1) {
            fprintf(stderr, "[%s] ERROR: Failed to unlink semaphores: %s\n", PROGRAM_NAME, strerror(errno));
            returnValue = -1;
        }
    }
    semaphoreCollection.wSyncSem = NULL;

    return returnValue;
}

/**
 * @brief Creates and opens all neccessary semaphores and returns a collection of these semaphores as a semphore_collection_t object.
 *        If something fails it automatically closes the semaphores and prints an error.
 * 
 * @return A collection of the opened semaphores as a semphore_collection_t object.
 */
static void openSEM(void) {
    if((semaphoreCollection.rSem = sem_open(R_SEM_NAME, O_CREAT | O_EXCL, 0600, 0)) == SEM_FAILED) {
        printStderrCleaupAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }
    
    if((semaphoreCollection.wSem = sem_open(W_SEM_NAME, O_CREAT | O_EXCL, 0600, MAX_NUM_RESULT_SETS)) == SEM_FAILED) {
        printStderrCleaupAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }

    if((semaphoreCollection.wSyncSem = sem_open(W_SEM_SYNC_NAME, O_CREAT | O_EXCL, 0600, 1)) == SEM_FAILED) {
        printStderrCleaupAndExit("[%s] ERROR: Failed to open semaphores: %s\n", PROGRAM_NAME, strerror(errno));
    }
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
 *        It stops the generators and closes the shared memory as well as the semaphores
 * 
 * @param circularBufferData pointer to the mapped shared memory circular buffer
 * @param semaphoreCollection pointer to a semaphore_collection_t containing the semaphores to close
 */
static void cleanup(void) {
    bool error = false;
    if(circularBufferData != NULL && semaphoreCollection.wSem != NULL) {
        circularBufferData -> stopGenerators = true;
        int semValue = 0;
        while(semValue < MAX_NUM_RESULT_SETS) {
            if(sem_getvalue(semaphoreCollection.wSem, &semValue) == -1) {
                fprintf(stderr, "[%s] ERROR: There was an error reading the value of a semaphore: %s\n", PROGRAM_NAME, strerror(errno));
                error = true;
                break;
            }
            
            if(sem_post(semaphoreCollection.wSem) == -1) {
                fprintf(stderr, "[%s] ERROR: There was an error pushing the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
                error = true;
                break;
            }
        }
    }

    if(closeSHM() == -1) {
        error = true;
    }
    if(closeSEM() == -1) {
        error = true;
    }

    if(error) {
        exit(EXIT_FAILURE);
    }
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

    program_parameters_t programParameters = parseArguments(argc, argv);

    openSHM();
    openSEM();

    // Wait if the delay is set
    if(programParameters.delay > 0) {
        sleep(programParameters.delay);
    }
    
    long readCounter = 0;
    long bestResultSet[MAX_NUM_EDGES_RESULT_SET][2];
    int numberOfEdgesInBestResult = MAX_NUM_EDGES_RESULT_SET + 1;
    while(!quitSignalRecieved && (programParameters.limit < 1 || readCounter < programParameters.limit)) {
        if(sem_wait(semaphoreCollection.rSem) == -1) {
            if(errno == EINTR) {
                continue;
            }
            cleanup();
            printStderrCleaupAndExit("[%s] ERROR: There was an error waiting for the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
        }

        int numberOfEdgesInResult = 0;
        for(int i = 0; i < MAX_NUM_EDGES_RESULT_SET && circularBufferData -> resultSets[circularBufferData -> readPos][i][0] != -1; i++) {
            numberOfEdgesInResult++;
        }

        // Break the loop if the result set is empty so the graph is three colorable
        if(numberOfEdgesInResult == 0) {
            numberOfEdgesInBestResult = 0;
            break;
        }

        // Save the new better result if it is better
        if(numberOfEdgesInResult < numberOfEdgesInBestResult) {
            for(int i = 0; i < MAX_NUM_EDGES_RESULT_SET; ++i) {
                bestResultSet[i][0] = circularBufferData -> resultSets[circularBufferData -> readPos][i][0];
                bestResultSet[i][1] = circularBufferData -> resultSets[circularBufferData -> readPos][i][1];
            }
            numberOfEdgesInBestResult = numberOfEdgesInResult;

            fprintf(stderr, "New best result found:\n");
            for(int i = 0; i < MAX_NUM_EDGES_RESULT_SET && bestResultSet[i][0] != -1; ++i) {
                fprintf(stderr, "[%ld, %ld]\n", bestResultSet[i][0], bestResultSet[i][1]);
            }
        }

        circularBufferData -> readPos = circularBufferData -> readPos + 1;
        circularBufferData -> readPos = circularBufferData -> readPos % MAX_NUM_RESULT_SETS;

        if(sem_post(semaphoreCollection.wSem) == -1) {
            cleanup();
            printStderrCleaupAndExit("[%s] ERROR: There was an error pushing the semaphore: %s\n", PROGRAM_NAME, strerror(errno));
        }

        ++readCounter;
    }

    if(numberOfEdgesInBestResult == 0) {
        printf("The graph is 3-colorable!\n");
    } else {
        printf("The graph might not be 3-colorable, best solution removes %d edges.\n", numberOfEdgesInBestResult);
    }

    cleanup();
    return EXIT_SUCCESS;
}
