// TODO: Change this prefix
#define MAT_NUMMER_PREFIX "XXXX_"
#define SHM_NAME MAT_NUMMER_PREFIX "SHM"
#define R_SEM_NAME MAT_NUMMER_PREFIX "R_SEM"
#define W_SEM_NAME MAT_NUMMER_PREFIX "W_SEM"
#define W_SEM_SYNC_NAME MAT_NUMMER_PREFIX "W_SEM_SYNC"

#define MAX_NUM_RESULT_SETS 10
#define MAX_NUM_EDGES_RESULT_SET 10

typedef struct {
    int resultSets[MAX_NUM_RESULT_SETS][MAX_NUM_EDGES_RESULT_SET][2];
    int readPos;
    int writePos;
} circular_buffer_data_t;
