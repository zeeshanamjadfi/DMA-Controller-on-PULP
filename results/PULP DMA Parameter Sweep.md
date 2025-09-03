# PULP DMA Parameter Sweep Test Results

## Overview

This repository src folder contains a comprehensive DMA (Direct Memory Access) parameter sweep test for PULP (Parallel Ultra Low Power) architecture. The test evaluates different DMA configurations by varying chunking parameters and measures their impact on performance and data transfer efficiency.

## Test Description

### Purpose
The test analyzes how different DMA transfer strategies affect performance when moving data between L2 external memory and L1 cluster memory on PULP processors. This is crucial for optimizing data-intensive applications on ultra-low-power embedded systems.

### Memory Architecture
- **L2 External Memory**: Slower but larger capacity storage
- **L1 Cluster Memory (TCDM)**: Fast, low-latency memory with limited capacity
- **Data Flow**: L2(source) → L1(processing) → L2(destination)

### Test Parameters
- **Buffer Size**: Fixed at 2048 bytes for consistent comparison
- **NB_COPY**: Number of DMA transfers per iteration {1, 2, 4, 8}
- **NB_ITER**: Number of iterations to complete the buffer {1, 2, 4, 8}
- **Total Configurations**: 16 (4 × 4 parameter combinations)

## Test Results Analysis

### Raw Performance Data

| NB_COPY | NB_ITER | Buffer Size | Cycles | Result | Transfer Size per DMA |
|---------|---------|-------------|--------|--------|--------------------|
| 1 | 1 | 2048 | 4345 | SUCCESS | 2048 bytes |
| 1 | 2 | 2048 | 5570 | SUCCESS | 1024 bytes |
| 1 | 4 | 2048 | 9311 | SUCCESS | 512 bytes |
| 1 | 8 | 2048 | 16819 | SUCCESS | 256 bytes |
| 2 | 1 | 2048 | 3782 | SUCCESS | 1024 bytes |
| 2 | 2 | 2048 | 5618 | SUCCESS | 512 bytes |
| 2 | 4 | 2048 | 9359 | SUCCESS | 256 bytes |
| 2 | 8 | 2048 | 16974 | SUCCESS | 128 bytes |
| 4 | 1 | 2048 | 3816 | SUCCESS | 512 bytes |
| 4 | 2 | 2048 | 5702 | SUCCESS | 256 bytes |
| 4 | 4 | 2048 | 9797 | SUCCESS | 128 bytes |
| 4 | 8 | 2048 | 18235 | SUCCESS | 64 bytes |
| 8 | 1 | 2048 | 3919 | SUCCESS | 256 bytes |
| 8 | 2 | 2048 | 6176 | SUCCESS | 128 bytes |
| 8 | 4 | 2048 | 10911 | SUCCESS | 64 bytes |
| 8 | 8 | 2048 | 20771 | SUCCESS | 32 bytes |

### Key Performance Insights

#### 1. Optimal Configuration
- **Best Performance**: NB_COPY=2, NB_ITER=1 (3782 cycles)
- **Transfer Strategy**: 2 DMA transfers of 1024 bytes each
- **Performance Gain**: ~13% better than single large transfer

#### 2. Performance Patterns

**By NB_ITER (Impact of Iteration Count):**
- NB_ITER=1: 3782-4345 cycles (best range)
- NB_ITER=2: 5570-6176 cycles
- NB_ITER=4: 9311-10911 cycles  
- NB_ITER=8: 16819-20771 cycles (worst range)

**Key Finding**: Performance degrades significantly with increased iterations due to loop overhead and repeated synchronization.

**By NB_COPY (Impact of Transfer Chunking):**
- Sweet spot at NB_COPY=2-4 for single iterations
- Diminishing returns beyond NB_COPY=4
- NB_COPY=8 shows performance degradation due to excessive fragmentation

#### 3. Transfer Size Impact
- **Large Transfers (1024+ bytes)**: Most efficient
- **Medium Transfers (256-512 bytes)**: Good performance
- **Small Transfers (<128 bytes)**: Significant overhead penalty

### Performance Trends

1. **Iteration Overhead**: Each additional iteration adds ~3000-4000 cycles overhead
2. **DMA Setup Cost**: Multiple small DMA commands have higher setup-to-transfer ratio
3. **Memory Bandwidth**: Larger contiguous transfers better utilize available bandwidth
4. **Synchronization Cost**: Frequent wait operations impact performance

## Technical Implementation

### Code Structure
```
dma_parameter_sweep.c
├── Configuration Parameters (BUFF_SIZE)
├── Memory Buffers (ext_buff0, ext_buff1, loc_buff)
├── Pseudo-Random Generator (reproducible test data)
├── Cluster Processing Function (parameterized DMA transfers)
├── Individual Test Execution (performance measurement)
└── Main Test Function (parameter sweep)
```

### Data Processing
- Simple multiplication by 3 for verification
- 8-bit arithmetic with wraparound
- Correctness verification on all 16 configurations

### Memory Management
- Dynamic L1 buffer allocation
- Proper cleanup and error handling
- Resource management for cluster operations

## Recommendations

### For Application Developers

1. **Optimal Strategy**: Use NB_COPY=2, NB_ITER=1 for best performance
2. **Avoid Over-Fragmentation**: Keep DMA transfer sizes above 256 bytes when possible
3. **Minimize Iterations**: Design algorithms to process data in fewer iterations
4. **Balance Memory Usage**: Consider L1 memory constraints vs. performance gains


## Usage

### Compilation
# Compile for PULP target
make clean all

### Execution
# Run the parameter sweep test
make run runner_args="--vcd" 

### Expected Output
The test will output performance results for all 16 configurations, showing cycles consumed and success/failure status for each combination.

## Technical Notes

- **Hardware**: PULP cluster architecture with L1 TCDM and L2 external memory
- **Performance Counter**: Hardware cycle counter for precise measurement
- **Data Verification**: Byte-by-byte comparison ensures transfer integrity
- **Memory Safety**: Proper allocation/deallocation prevents memory leaks

## Future Work

1. **Extended Parameter Range**: Test with different buffer sizes and transfer patterns
2. **Energy Measurement**: Add power consumption analysis
3. **Concurrent Access**: Evaluate multi-core DMA usage patterns
4. **Real-World Workloads**: Apply findings to actual signal processing applications

## Author

**Zeeshan Amjad** 
