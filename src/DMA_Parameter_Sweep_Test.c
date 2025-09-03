/**
 * @file dma_parameter_sweep.c
 * @brief PULP DMA Parameter Sweep Test
 * @author Zeeshan Amjad
 * 
 * This program tests different DMA configurations by varying the number of
 * DMA copies per iteration (NB_COPY) and the number of iterations (NB_ITER).
 * It measures performance in cycles and verifies data correctness.
 * 
 * Test Matrix:
 * - NB_COPY: {1, 2, 4, 8} - chunks per iteration
 * - NB_ITER: {1, 2, 4, 8} - iterations to complete buffer
 * - Total: 16 different configurations tested
 * 
 * Memory Flow: L2(ext_buff0) → L1(loc_buff) → process → L1(loc_buff) → L2(ext_buff1)
 */

//Vary DMA Parameter Code
#include "pmsis.h"
#include "pmsis/cluster/dma/cl_dma.h"
#include <stdio.h>

/*=============================================================================
 * CONFIGURATION PARAMETERS
 *============================================================================*/
#define BUFF_SIZE 2048  // Fixed buffer size for consistent parameter comparison

/*=============================================================================
 * GLOBAL MEMORY BUFFERS
 *============================================================================*/
static char ext_buff0[BUFF_SIZE];   // Source buffer in L2 external memory
static char ext_buff1[BUFF_SIZE];   // Destination buffer in L2 external memory  
static char *loc_buff;              // Processing buffer in L1 cluster memory (allocated dynamically)

/*=============================================================================
 * PSEUDO-RANDOM NUMBER GENERATOR
 *============================================================================*/
// Simple LCG for pseudo-random numbers
static uint32_t lcg_seed = 1;      // Seed for Linear Congruential Generator

/**
 * @brief Generate pseudo-random number using LCG algorithm
 * @return 31-bit pseudo-random number
 * 
 * Uses same parameters as glibc rand() for reproducible test data
 */
static inline uint32_t my_rand()
{
    lcg_seed = (1103515245 * lcg_seed + 12345) & 0x7fffffff;
    return lcg_seed;
}

/*=============================================================================
 * CLUSTER PROCESSING FUNCTION
 *============================================================================*/
/**
 * @brief Main cluster task that performs parameterized DMA transfers
 * @param arg Pointer to array containing [NB_COPY, NB_ITER] parameters
 * 
 * This function runs on the cluster and performs:
 * 1. Chunked DMA transfers from L2 to L1 (EXT2LOC)
 * 2. Simple data processing (multiply by 3) 
 * 3. Chunked DMA transfers from L1 back to L2 (LOC2EXT)
 * 
 * The chunking parameters control DMA granularity:
 * - NB_COPY: Number of DMA commands issued per iteration
 * - NB_ITER: Number of iterations to process entire buffer
 * - COPY_SIZE: Bytes transferred per DMA command
 */
static void cluster_entry(void *arg)
{
    // Extract DMA parameters from argument
    int NB_COPY  = ((int*)arg)[0];   // Number of DMA copies per iteration
    int NB_ITER  = ((int*)arg)[1];   // Number of iterations to complete buffer
    
    // Calculate chunk sizes based on parameters
    int COPY_SIZE = BUFF_SIZE / NB_ITER / NB_COPY;  // Bytes per individual DMA transfer
    int ITER_SIZE = BUFF_SIZE / NB_ITER;            // Bytes processed per iteration

    // Process buffer across multiple iterations
    for (int j = 0; j < NB_ITER; j++)
    {
        pi_cl_dma_cmd_t copy[NB_COPY];  // DMA command structures for this iteration

        /*---------------------------------------------------------------------
         * PHASE 1: Transfer data from L2 to L1 (EXT2LOC)
         *--------------------------------------------------------------------*/
        // Issue all DMA read commands for this iteration
        for (int i = 0; i < NB_COPY; i++)
            pi_cl_dma_cmd((int)ext_buff0 + COPY_SIZE*i + ITER_SIZE*j,  // L2 source address
                          (int)loc_buff + COPY_SIZE*i + ITER_SIZE*j,   // L1 destination address
                          COPY_SIZE, PI_CL_DMA_DIR_EXT2LOC, &copy[i]);

        // Wait for all EXT2LOC transfers to complete before processing
        for (int i = 0; i < NB_COPY; i++)
            pi_cl_dma_cmd_wait(&copy[i]);

        /*---------------------------------------------------------------------
         * PHASE 2: Process data in fast L1 memory
         *--------------------------------------------------------------------*/
        // Optional processing: multiply each byte by 3 for verification
        // This runs efficiently in L1 memory with low access latency
        for (int i = 0; i < BUFF_SIZE; i++)
            loc_buff[i] = loc_buff[i] * 3;

        /*---------------------------------------------------------------------
         * PHASE 3: Transfer processed data from L1 back to L2 (LOC2EXT)
         *--------------------------------------------------------------------*/
        // Write back: Issue all DMA write commands for this iteration
        for (int i = 0; i < NB_COPY; i++)
            pi_cl_dma_cmd((int)ext_buff1 + COPY_SIZE*i + ITER_SIZE*j,  // L2 destination address
                          (int)loc_buff + COPY_SIZE*i + ITER_SIZE*j,   // L1 source address
                          COPY_SIZE, PI_CL_DMA_DIR_LOC2EXT, &copy[i]);

        // Wait for all LOC2EXT transfers to complete before next iteration
        for (int i = 0; i < NB_COPY; i++)
            pi_cl_dma_cmd_wait(&copy[i]);
    }
}

/*=============================================================================
 * INDIVIDUAL TEST EXECUTION
 *============================================================================*/
/**
 * @brief Execute DMA test for a specific parameter combination
 * @param nb_copy Number of DMA transfers per iteration
 * @param nb_iter Number of iterations to complete the buffer
 * @return 0 on success, -1 on failure
 * 
 * This function:
 * 1. Allocates L1 memory and initializes test data
 * 2. Configures and runs the cluster task with specified parameters
 * 3. Measures execution time in cycles
 * 4. Verifies data correctness
 * 5. Reports results and cleans up resources
 */
static int run_dma_test(int nb_copy, int nb_iter)
{
    /*-------------------------------------------------------------------------
     * MEMORY ALLOCATION
     *------------------------------------------------------------------------*/
    // Allocate buffer in L1 cluster memory (fast TCDM)
    loc_buff = pmsis_l1_malloc(BUFF_SIZE);
    if (!loc_buff)
    {
        printf("Failed to allocate L1 buffer!\n");
        return -1;
    }

    /*-------------------------------------------------------------------------
     * TEST DATA INITIALIZATION  
     *------------------------------------------------------------------------*/
    // Fill external buffer with pseudo-random data for testing
    for (int i = 0; i < BUFF_SIZE; i++)
        ext_buff0[i] = my_rand() & 0xFF;

    /*-------------------------------------------------------------------------
     * CLUSTER SETUP AND CONFIGURATION
     *------------------------------------------------------------------------*/
    struct pi_device cluster_dev;
    struct pi_cluster_conf conf;
    struct pi_cluster_task cluster_task;

    // Initialize cluster with default configuration
    pi_cluster_conf_init(&conf);
    pi_open_from_conf(&cluster_dev, &conf);

    // Open cluster device
    if (pi_cluster_open(&cluster_dev))
    {
        printf("Cluster open failed!\n");
        return -1;
    }

    /*-------------------------------------------------------------------------
     * CLUSTER TASK SETUP
     *------------------------------------------------------------------------*/
    // Pass DMA parameters to cluster task
    int args[2] = {nb_copy, nb_iter};
    pi_cluster_task(&cluster_task, cluster_entry, args);

    /*-------------------------------------------------------------------------
     * PERFORMANCE MEASUREMENT
     *------------------------------------------------------------------------*/
    // Performance measurement: configure cycle counter
    pi_perf_conf(1 << PI_PERF_CYCLES);
    pi_perf_reset();
    pi_perf_start();

    // Execute the DMA test on cluster
    pi_cluster_send_task_to_cl(&cluster_dev, &cluster_task);

    // Stop performance measurement and read results
    pi_perf_stop();
    uint32_t cycles = pi_perf_read(PI_PERF_CYCLES);

    /*-------------------------------------------------------------------------
     * RESULT VERIFICATION
     *------------------------------------------------------------------------*/
    // Verify correctness: check if processing was applied correctly
    int error = 0;
    for (int i = 0; i < BUFF_SIZE; i++)
    {
        // Expected result: original value * 3 (with 8-bit wraparound)
        if (ext_buff1[i] != (char)((ext_buff0[i] * 3) & 0xFF))
        {
            error = 1;
            break;  // Stop on first error for efficiency
        }
    }

    /*-------------------------------------------------------------------------
     * RESULTS REPORTING
     *------------------------------------------------------------------------*/
    // Print test results in consistent format for analysis
    printf("NB_COPY=%d NB_ITER=%d Buffer=%d Cycles=%u Result=%s\n",
           nb_copy, nb_iter, BUFF_SIZE, cycles, error ? "FAIL" : "SUCCESS");

    /*-------------------------------------------------------------------------
     * CLEANUP
     *------------------------------------------------------------------------*/
    // Close cluster device and free allocated memory
    pi_cluster_close(&cluster_dev);
    pmsis_l1_malloc_free(loc_buff, BUFF_SIZE);

    return error ? -1 : 0;
}

//=============================================================================
// Main Test Function
//=============================================================================
// Execute parameter sweep across all combinations
static int test_entry()
{
    int nb_copy_values[] = {1, 2, 4, 8};  // DMA chunks per iteration
    int nb_iter_values[] = {1, 2, 4, 8};  // Iterations to complete buffer

    printf("Starting DMA parameter sweep tests...\n");

    // Test all combinations (4 × 4 = 16 configurations)
    for (int i = 0; i < sizeof(nb_copy_values)/sizeof(int); i++)
    {
        for (int j = 0; j < sizeof(nb_iter_values)/sizeof(int); j++)
        {
            run_dma_test(nb_copy_values[i], nb_iter_values[j]);
        }
    }
    return 0;
}

//=============================================================================
// Application Entry Points
//=============================================================================
static void test_kickoff(void *arg)
{
    int ret = test_entry();
    pmsis_exit(ret);
}

int main()
{
    return pmsis_kickoff((void *)test_kickoff);
}
