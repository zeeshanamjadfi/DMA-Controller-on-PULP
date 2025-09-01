/**
 * @file test.c
 * @brief PULP-SDK DMA Transfer Test
 * 
 * This program demonstrates DMA transfers between L2 (external) and L1 (cluster local) 
 * memory on PULP platforms. It performs chunked data transfers with simple processing
 * to validate DMA functionality and measure performance.
 * 
 * Memory Layout:
 * - L2 Memory: External memory accessible by fabric controller
 * - L1 Memory: Fast cluster-local memory (TCDM - Tightly Coupled Data Memory)
 * 
 * Test Flow:
 * 1. Initialize source buffer in L2 with pseudo-random data
 * 2. Transfer data from L2 to L1 in chunks using DMA
 * 3. Process data in L1 (multiply by 3)
 * 4. Transfer processed data back to L2 using DMA
 * 5. Verify correctness and measure performance
 */

#include "pmsis.h"
#include "pmsis/cluster/dma/cl_dma.h"
#include <stdio.h>

/*=============================================================================
 * CONFIGURATION PARAMETERS
 *============================================================================*/
#define BUFF_SIZE 2048       /**< Total buffer size in bytes - increased for parameter sweep */
#define NB_COPY   2          /**< Number of DMA chunks per iteration */
#define NB_ITER   4          /**< Number of iterations to process entire buffer */
#define COPY_SIZE (BUFF_SIZE/NB_ITER/NB_COPY)  /**< Size of each DMA chunk (128 bytes) */
#define ITER_SIZE (BUFF_SIZE/NB_ITER)          /**< Size processed per iteration (256 bytes) */

/*=============================================================================
 * GLOBAL MEMORY BUFFERS
 *============================================================================*/
static char ext_buff0[BUFF_SIZE];   /**< Source buffer in L2 external memory */
static char ext_buff1[BUFF_SIZE];   /**< Destination buffer in L2 external memory */
static char *loc_buff;              /**< Processing buffer in L1 cluster memory */

/*=============================================================================
 * PSEUDO-RANDOM NUMBER GENERATOR
 *============================================================================*/
static uint32_t lcg_seed = 1;       /**< Seed for Linear Congruential Generator */

/**
 * @brief Simple pseudo-random number generator using Linear Congruential Generator
 * @return 31-bit pseudo-random number
 * 
 * Uses the same parameters as glibc's rand() function for reproducible results.
 * Formula: next = (a * seed + c) mod m
 * Where: a = 1103515245, c = 12345, m = 2^31
 */
static inline uint32_t my_rand(void)
{
    lcg_seed = (1103515245 * lcg_seed + 12345) & 0x7fffffff;
    return lcg_seed;
}

/*=============================================================================
 * CLUSTER PROCESSING FUNCTION
 *============================================================================*/
/**
 * @brief Main cluster task performing DMA transfers and data processing
 * @param arg Unused parameter (required by cluster task interface)
 * 
 * This function runs on the cluster and performs:
 * 1. Chunked DMA transfers from L2 to L1 (EXT2LOC)
 * 2. Simple data processing in fast L1 memory
 * 3. Chunked DMA transfers from L1 back to L2 (LOC2EXT)
 * 
 * The chunked approach allows overlapping computation with memory transfers
 * and helps manage limited L1 memory resources.
 */
static void cluster_entry(void *arg)
{
    // Process buffer in multiple iterations
    for (int j = 0; j < NB_ITER; j++)
    {
        pi_cl_dma_cmd_t copy[NB_COPY]; // DMA command structures for this iteration
        
        /*---------------------------------------------------------------------
         * PHASE 1: Transfer data from L2 to L1 (EXT2LOC)
         *--------------------------------------------------------------------*/
        for (int i = 0; i < NB_COPY; i++)
        {
            // Calculate source and destination addresses for this chunk
            uint32_t src_addr = (uint32_t)ext_buff0 + COPY_SIZE * i + ITER_SIZE * j;
            uint32_t dst_addr = (uint32_t)loc_buff + COPY_SIZE * i + ITER_SIZE * j;
            
            // Issue DMA transfer command
            pi_cl_dma_cmd(
                src_addr,                    // Source address in L2
                dst_addr,                    // Destination address in L1  
                COPY_SIZE,                   // Transfer size
                PI_CL_DMA_DIR_EXT2LOC,      // Direction: External to Local
                &copy[i]                     // Command structure to track transfer
            );
        }
        
        // Wait for all EXT2LOC transfers to complete before processing
        for (int i = 0; i < NB_COPY; i++) {
            pi_cl_dma_cmd_wait(&copy[i]);
        }
        
        /*---------------------------------------------------------------------
         * PHASE 2: Process data in fast L1 memory
         *--------------------------------------------------------------------*/
        // Simple processing: multiply each byte by 3
        // This runs efficiently in L1 memory with low latency access
        for (int i = 0; i < BUFF_SIZE; i++) {
            loc_buff[i] = (char)(loc_buff[i] * 3);
        }
        
        /*---------------------------------------------------------------------
         * PHASE 3: Transfer processed data from L1 back to L2 (LOC2EXT)
         *--------------------------------------------------------------------*/
        for (int i = 0; i < NB_COPY; i++)
        {
            // Calculate source and destination addresses for this chunk
            uint32_t src_addr = (uint32_t)loc_buff + COPY_SIZE * i + ITER_SIZE * j;
            uint32_t dst_addr = (uint32_t)ext_buff1 + COPY_SIZE * i + ITER_SIZE * j;
            
            // Issue DMA transfer command  
            pi_cl_dma_cmd(
                dst_addr,                    // Destination address in L2
                src_addr,                    // Source address in L1
                COPY_SIZE,                   // Transfer size
                PI_CL_DMA_DIR_LOC2EXT,      // Direction: Local to External
                &copy[i]                     // Command structure to track transfer
            );
        }
        
        // Wait for all LOC2EXT transfers to complete before next iteration
        for (int i = 0; i < NB_COPY; i++) {
            pi_cl_dma_cmd_wait(&copy[i]);
        }
    }
}

/*=============================================================================
 * TEST EXECUTION AND VERIFICATION
 *============================================================================*/
/**
 * @brief Main test function that orchestrates the DMA test
 * @return 0 on success, -1 on failure
 * 
 * This function:
 * 1. Initializes and configures the cluster
 * 2. Allocates L1 memory buffer
 * 3. Initializes test data
 * 4. Executes the cluster task with performance monitoring
 * 5. Verifies results and reports performance
 */
static int test_entry(void)
{
    printf("=== PULP DMA Transfer Test ===\n");
    printf("Buffer size: %d bytes\n", BUFF_SIZE);
    printf("Chunks per iteration: %d\n", NB_COPY);
    printf("Number of iterations: %d\n", NB_ITER);
    printf("Chunk size: %d bytes\n", COPY_SIZE);
    
    /*-------------------------------------------------------------------------
     * CLUSTER INITIALIZATION
     *------------------------------------------------------------------------*/
    struct pi_device cluster_dev;
    struct pi_cluster_conf conf;
    struct pi_cluster_task cluster_task;
    
    // Initialize cluster configuration with default settings
    pi_cluster_conf_init(&conf);
    
    // Open cluster device
    pi_open_from_conf(&cluster_dev, &conf);
    if (pi_cluster_open(&cluster_dev)) {
        printf("ERROR: Failed to open cluster device!\n");
        return -1;
    }
    
    /*-------------------------------------------------------------------------
     * MEMORY ALLOCATION
     *------------------------------------------------------------------------*/
    // Allocate buffer in L1 cluster memory (fast TCDM)
    loc_buff = pmsis_l1_malloc(BUFF_SIZE);
    if (!loc_buff) {
        printf("ERROR: Failed to allocate %d bytes in L1 memory!\n", BUFF_SIZE);
        pi_cluster_close(&cluster_dev);
        return -1;
    }
    printf("L1 buffer allocated at address: 0x%08x\n", (uint32_t)loc_buff);
    
    /*-------------------------------------------------------------------------
     * TEST DATA INITIALIZATION
     *------------------------------------------------------------------------*/
    printf("Initializing source buffer with pseudo-random data...\n");
    for (int i = 0; i < BUFF_SIZE; i++) {
        ext_buff0[i] = (char)(my_rand() & 0xFF);
    }
    
    // Clear destination buffer to ensure clean test
    for (int i = 0; i < BUFF_SIZE; i++) {
        ext_buff1[i] = 0;
    }
    
    /*-------------------------------------------------------------------------
     * PERFORMANCE MEASUREMENT SETUP
     *------------------------------------------------------------------------*/
    // Configure performance counter to measure cycles
    pi_perf_conf(1 << PI_PERF_CYCLES);
    pi_perf_reset();
    pi_perf_start();
    
    /*-------------------------------------------------------------------------
     * CLUSTER TASK EXECUTION
     *------------------------------------------------------------------------*/
    printf("Executing DMA transfers and processing on cluster...\n");
    
    // Configure cluster task
    pi_cluster_task(&cluster_task, cluster_entry, NULL);
    
    // Send task to cluster and wait for completion
    pi_cluster_send_task_to_cl(&cluster_dev, &cluster_task);
    
    /*-------------------------------------------------------------------------
     * PERFORMANCE MEASUREMENT
     *------------------------------------------------------------------------*/
    pi_perf_stop();
    uint32_t cycles = pi_perf_read(PI_PERF_CYCLES);
    
    printf("DMA test completed in %u cycles\n", cycles);
    
    // Calculate throughput metrics
    uint32_t total_transfers = BUFF_SIZE * 2; // Read + Write
    float cycles_per_byte = (float)cycles / total_transfers;
    printf("Performance: %.2f cycles per byte transferred\n", cycles_per_byte);
    
    /*-------------------------------------------------------------------------
     * RESULT VERIFICATION
     *------------------------------------------------------------------------*/
    printf("Verifying results...\n");
    int error_count = 0;
    int max_errors = 10; // Limit error reporting
    
    for (int i = 0; i < BUFF_SIZE; i++) {
        char expected = (char)((ext_buff0[i] * 3) & 0xFF);
        if (ext_buff1[i] != expected) {
            if (error_count < max_errors) {
                printf("ERROR at index %d: expected 0x%02x, got 0x%02x (source: 0x%02x)\n",
                       i, expected & 0xFF, ext_buff1[i] & 0xFF, ext_buff0[i] & 0xFF);
            }
            error_count++;
        }
    }
    
    // Print test results
    if (error_count == 0) {
        printf("✓ TEST PASSED: All %d bytes processed correctly\n", BUFF_SIZE);
    } else {
        printf("✗ TEST FAILED: %d errors found", error_count);
        if (error_count > max_errors) {
            printf(" (%d errors shown)", max_errors);
        }
        printf("\n");
    }
    
    /*-------------------------------------------------------------------------
     * CLEANUP
     *------------------------------------------------------------------------*/
    pi_cluster_close(&cluster_dev);
    return (error_count == 0) ? 0 : -1;
}

/*=============================================================================
 * APPLICATION ENTRY POINTS
 *============================================================================*/
/**
 * @brief Test kickoff function called by PMSIS runtime
 * @param arg Unused parameter
 * 
 * This function is called by the PMSIS runtime system and handles
 * test execution and system exit.
 */
static void test_kickoff(void *arg)
{
    int ret = test_entry();
    printf("=== Test %s ===\n", (ret == 0) ? "COMPLETED SUCCESSFULLY" : "FAILED");
    pmsis_exit(ret);
}

/**
 * @brief Main entry point
 * @return Exit code (not directly used in PMSIS applications)
 * 
 * Starts the PMSIS runtime system with our test kickoff function.
 */
int main(void)
{
    printf("Starting PULP DMA Test Application\n");
    return pmsis_kickoff((void *)test_kickoff);
}
