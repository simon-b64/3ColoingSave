/**
 * @file commons.h
 * @author Simon Buchinger 12220026 <e12220026@student.tuwien.ac.at>
 * @date 27.11.2023
 * @program: 3coloring
 * 
 * @brief Headder file for shared defines and type definitions
 *
 **/

#ifndef COMMONS_H_FILE
#define COMMONS_H_FILE

#define MAT_NUMMER_PREFIX "12220026_"
#define SHM_NAME MAT_NUMMER_PREFIX "SHM"
#define R_SEM_NAME MAT_NUMMER_PREFIX "R_SEM"
#define W_SEM_NAME MAT_NUMMER_PREFIX "W_SEM"
#define W_SEM_SYNC_NAME MAT_NUMMER_PREFIX "W_SEM_SYNC"

#include <semaphore.h>

#define MAX_NUM_RESULT_SETS 10
#define MAX_NUM_EDGES_RESULT_SET 10

/**
 * @brief Structure to keep circular buffer data and stop generators signal
 * 
 */
typedef struct {
    long resultSets[MAX_NUM_RESULT_SETS][MAX_NUM_EDGES_RESULT_SET][2];
    int readPos;
    int writePos;
    bool stopGenerators;
} circular_buffer_data_t;

/**
 * @brief Structure to keep pointers to all open semaphores
 * 
 */
typedef struct {
    sem_t *rSem;
    sem_t *wSem;
    sem_t *wSyncSem;
} semaphore_colleciton_t;

#endif
