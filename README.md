**DMA-Controller-on-PULP**
This repository contains the implementation, experiments, and results of my project on Direct Memory Access (DMA) controller performance analysis using the PULP (Parallel Ultra-Low Power) platform. 
The project focuses on evaluating DMA latency and throughput under varying memory transfer sizes, contributing to the optimization of low-power SoCs.

📌 **Project Overview**
The DMA controller is a critical hardware block in modern SoCs, allowing memory-to-memory and peripheral-to-memory data transfers without continuous CPU intervention. This improves system efficiency and reduces power consumption.

**This project investigates:**
Latency analysis of DMA transfers for different buffer sizes.
Throughput analysis (bytes per cycle / bytes per second) to evaluate DMA efficiency at various transfer sizes.
Impact of system parameters (buffer alignment, transfer size, overheads) on performance.
Comparison with baseline CPU-driven transfers (planned extension).

🛠 **Tools & Technologies**
PULP SDK (runtime environment for application development).
RISC-V GCC Toolchain for cross-compilation.
C Language for benchmarking DMA test programs.
GNU Make for building and running test code.
