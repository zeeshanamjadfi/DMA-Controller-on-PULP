# PULP DMA Transfer Test For Throughput Calcuation

A comprehensive test suite for validating DMA (Direct Memory Access) functionality on PULP (Parallel Ultra-Low Power) platforms. This test demonstrates efficient data movement between L2 external memory and L1 cluster memory with performance measurement and verification.

## Overview

This test performs chunked DMA transfers with simple data processing to validate:
- DMA engine functionality (EXT2LOC and LOC2EXT)
- Memory hierarchy performance (L2 ↔ L1 transfers)
- Data integrity through processing pipeline
- Performance measurement and optimization

## Test Architecture

```
┌─────────────┐    DMA     ┌─────────────┐    Process    ┌─────────────┐
│ L2 Memory   │ ---------> │ L1 Memory   │ ------------> │ L2 Memory   │
│ (External)  │ EXT2LOC    │ (Cluster)   │ (×3 multiply) │ (External)  │
│ ext_buff0   │            │ loc_buff    │               │ ext_buff1   │
└─────────────┘            └─────────────┘               └─────────────┘
```

## Features

- **Chunked Transfers**: Configurable chunk sizes for optimal performance
- **Performance Monitoring**: Cycle counting and throughput calculation
- **Data Verification**: Complete result validation
- **Error Handling**: Comprehensive error checking and reporting
- **Memory Management**: Automatic L1 allocation and cleanup
- **Configurable Parameters**: Easy tuning of buffer sizes and chunk counts

## Configuration

Key parameters can be modified in `test.c`:

```c
#define BUFF_SIZE 2048       // Total buffer size in bytes
#define NB_COPY   2          // Number of DMA chunks per iteration  
#define NB_ITER   4          // Number of iterations
```

## Build and Run

### Prerequisites
- PULP SDK installed and configured https://github.com/pulp-platform/pulp-sdk.git
- GCC RISC-V toolchain https://github.com/pulp-platform/pulp-riscv-gnu-toolchain.git

### Basic Execution
```bash
# Clean and build
make clean all

# Run test
make run
```

### With Detailed GTK waveform
```bash
# Run with VCD
make run runner_args="--vcd"
```

## Performance Results

### Latest Test Results (2048 bytes)
- **Execution Time**: 11,201 cycles (224.02 μs @ 50MHz)
- **Throughput**: 18.29 MB/s (@ 50MHz)
- **Efficiency**: 2.73 cycles/byte
- **Status**: All tests passed

### Performance Metrics
| Configuration | Buffer Size | Cycles | Throughput |
|---------------|-------------|--------|------------|
| Current       | 2048 bytes  | 11,201 | 18.29 MB/s |

## Code Structure

```
test.c
├── Configuration Parameters     // Buffer sizes, chunk configuration
├── Memory Buffers              // L1/L2 buffer declarations
├── Random Number Generator     // Test data generation
├── Cluster Processing          // Main DMA and processing logic
├── Test Execution             // Test orchestration and verification
└── Performance Measurement    // Cycle counting and metrics
```

## Memory Layout

- **L2 Memory**: External memory accessible by fabric controller
- **L1 Memory**: Fast cluster-local TCDM (Tightly Coupled Data Memory)
- **Buffer Size**: Configurable (default: 2048 bytes)
- **Chunk Size**: Auto-calculated based on iterations and copies

## Optimization Notes

### Current Implementation
- Sequential DMA transfers with synchronization
- Simple processing (multiply by 3) for verification
- Chunked approach to manage L1 memory efficiently

### Optimization Opportunities
- **Pipeline Overlapping**: Overlap DMA with computation
- **Multi-Core**: Utilize multiple cluster cores
- **Double Buffering**: Implement ping-pong buffers
- **Burst Optimization**: Tune chunk sizes for platform

## Test Validation

The test validates:
1. **DMA Functionality**: Successful L2 ↔ L1 transfers
2. **Data Integrity**: Byte-by-byte result verification
3. **Performance**: Cycle counting and throughput measurement
4. **Memory Safety**: Proper allocation and error handling

## Contributing

When modifying the test:
1. Maintain backward compatibility with existing configurations
2. Add appropriate documentation for new features
3. Ensure all test cases pass
4. Update performance baselines in documentation


## Support

For issues related to:
- **PULP Platform**: Check PULP project documentation https://github.com/pulp-platform
- **Test Modifications**: See inline code comments

