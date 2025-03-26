
## Overview
### This is Matrix transposition Implementation in C++ that exploits cache associativity for performance.Observes various  performance characteristics based on exploits of the underlying hardware. Implementation benchmarks naive transposition and blocked transposition considering cache organization system.

***Cache associativity describes how a CPU cache organizes memory to map main memory addresses to cache locations. Implementing a naive matrix transposition without considering cache organization details slows down the process due to set eviction. A blocked matrix transposition, where the block size is chosen to fully fit within the cache, improves performance for both row-major and column-major iterations in the original and transposed matrices.***

*I am using a 13th-gen Intel Core i7-1355U processor, formally known as Raptor Lake-S. By referring to the processor [documentation]
(https://edc.intel.com/content/www/us/en/design/products/platforms/details/raptor-lake-s/13th-generation-core-processor-specification-update/),
i7-1355U processor has 10 cores each divided to a P-core and E-core where P-cores L1 cache has 48KB capacity for data which is 12-way assosiative(find in 13th Generation Intel® Core™ and Intel® Core™ 14th Generation Processors Datasheet, Volume 1 of 2 pg 46).
I am focusing on ensuring that blocks from matrix A (row-major) and the corresponding transposed blocks in matrix B (column-major) fit within the 48 KB, 12-way set-associative L1 data cache. This approach prevents them from overwriting each other while accounting for the cache’s associativity and access patterns.*



### Step 1: Understand the Cache and Requirements
- **L1 Data Cache (P-core)**:
  - **Size**: 48 KB.
  - **Associativity**: 12-way set-associative.
  - **Cache Line Size**: 64 bytes.
  - **Number of Sets**:
    - Total cache lines = 48 KB ÷ 64 bytes = 48,000 ÷ 64 = 750 cache lines.
    - Number of sets = 750 ÷ 12 = 62.5, rounded to 64 sets (2^6, for hardware simplicity).
  - **Set Mapping**:
    - Bits 0–5 (6 bits): Offset within a 64-byte cache line (2^6 = 64).
    - Bits 6–11 (6 bits): Set index (2^6 = 64 sets).
- **Matrix Transpose**:
  - Operation: `B[j][i] = A[i][j]`.
  - **A (Row-Major)**: Sequential access, good spatial locality.
  - **B (Column-Major)**: Non-sequential access, poor spatial locality (stride = N x 4 bytes).
- **Goals**:
  1. **Fit Both Blocks in L1 Cache**: The block from A and the transposed block in B should fit in the 48 KB L1 cache.
  2. **Minimize Overwriting**: Ensure A and B’s cache lines don’t evict each other due to excessive conflicts in the 12-way set-associative cache.
  3. **Account for 12-Way Associativity**: Keep the number of cache lines per set within the 12-way limit.
  4. **Consider Row-Major and Column-Major Access**: Balance the access patterns to reduce conflicts.


### Step 3: Align Block Size with 12-Way Associativity
To minimize conflicts and prevent overwriting, we need a block size B x B where the number of rows B in B’s column-major access results in a number of cache lines per set that fits within the 12-way associativity limit.


### Block Size Formula for Cache-Aware Matrix Transposition

This project provides a C++ function to calculate optimal block sizes (B_N, B_M) 
for transposing an N x M matrix, leveraging a 48MB, 12-way associative cache 
with 64-byte lines. The goal is to minimize cache evictions and maximize 
performance by fitting source and destination blocks into the cache.

// Cache Details
- Cache size: 48MB = 50,331,648 bytes
- Line size: 64 bytes → 786,432 lines
- Associativity: 12-way → 65,536 sets
- Each set holds 12 lines; total capacity = 786,432 lines

// Block Size Calculation
For an N x M matrix with element size s (bytes):
- Source block: B_N x B_M, size = B_N * B_M * s bytes
- Destination block: B_M x B_N, size = B_N * B_M * s bytes
- Total size: 2 * B_N * B_M * s ≤ 50,331,648 * cache_fraction
- Lines: 2 * B_N * B_M * s / 64
- Goal: Keep lines per set ≤ 12 to avoid evictions

// Formula
Max elements: B_N * B_M ≤ 25,165,824 / s
- Ideal: B_N = sqrt((25,165,824 / s) * (N / M))
        B_M = sqrt((25,165,824 / s) * (M / N))
- Adjust to fit cache and tile N, M cleanly

// Two Approaches
1. Casting to Integer:
   - B_N = int(sqrt(max_elements * N/M)), B_M = int(sqrt(max_elements * M/N))
   - Pros: Simple, near-max cache usage (e.g., 47.98MB for s=4)
   - Cons: Uneven tiling (e.g., 2048/1773 ≈ 1.155), requires remainder handling

2. Using Divisors:
   - Snap B_N, B_M to largest divisors of N, M within limits
   - Pros: Clean tiling (e.g., 2048/2048 = 1), better locality
   - Cons: Slightly more computation

// C++ Function (findBlockSize)
Input: N (rows), M (cols), s (element size), cache_fraction (0.0 to 1.0)
Output: std::pair<int, int> {B_N, B_M}
- Uses divisors to ensure whole blocks
- Example: N=2048, M=4096, s=4, full cache → B_N=2048, B_M=3072 (48MB)

// Key Insights
- 12-way Associativity: Matters if set conflicts exceed 12 lines/set, even if total fits
- Divisors vs. Casting: Divisors optimize tiling; casting maximizes cache but complicates loops
- Cache Fraction: Tune usage (e.g., 0.5 for 24MB)

// Example Results
- N=2048, M=4096, s=4:
  - Divisors: B_N=2048, B_M=3072 (48MB, 12 lines/set)
  - Casting: B_N=1773, B_M=3547 (~47.98MB, remainder)
- N=2048, M=4096, s=8:
  - Divisors: B_N=1024, B_M=3072 (48MB)


## Dependencies

- C++20 compiler (for std::format)
- Standard Template Library (STL)
- Custom headers:
  - [`kaizen.h`](https://github.com/heinsaar/kaizen) (timer, print, random_int utilities)
  - `Rec_MatMul.h` (matrix multiplication implementations)
  - `cache_size.h` (cache-related constants)

  
## Build Instructions

1. **Clone the repository**:
    ```bash
    git clone https://github.com/marybadalyan/Mat_Trans_with_Cache_Associativity
    ```

2. **Go into the repository**:
    ```bash
    cd Mat_Trans_with_Cache_Associativity
    ```

3. **Generate the build files**:
    ```bash
    cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
    ```

4. **Build the project**:
    ```bash
    cmake --build build --config Release
    ```

## Usage
Once compiled, run the program to start the memory stress test:

```bash
./Mat_Trans_with_Cache_Associativity --rows [num] --cols [num]  // num as in int 
```
### Sample output:
```
Multiplication Performance (1000x1000 * 1000x1000)
Pinned to P-core 0, Thread 0 (logical processor 0)
Running on CPU (thread) 0 (should be 0)
| Iterative Transposition        |  740500.00 |
| Naive Transposition            | 1113800.00 |
| Speedup Factor                 |       0.66 | // Iterative/Naive
```

This snippet from ```main()``` sets thread affinity, ensuring that the executing thread runs on a specific CPU core specific thread and subcore(P and E). 
The implementation is platform-dependent, handling both Windows and Linux.

### **Windows Implementation (`#ifdef _WIN32`)**
1. **Get the Current Thread Handle**  
   ```cpp
   HANDLE thread = GetCurrentThread();
   ```
   - Retrieves a pseudo-handle for the current thread.

2. **Set Thread Affinity to Logical Processor 0**  
   ```cpp
   DWORD_PTR affinityMask = 1ULL << 0; // Pin to logical processor 0
   DWORD_PTR oldMask = SetThreadAffinityMask(thread, affinityMask);
   ```
   - `affinityMask = 1ULL << 0` means the thread is pinned to CPU core 0 (bit 0 is set).
   - `SetThreadAffinityMask(thread, affinityMask)` forces the thread to run only on CPU 0.
   - Returns the old affinity mask (0 on failure).

3. **Check for Errors**  
   ```cpp
   if (oldMask == 0) {
       std::cerr << "Failed to set affinity: " << GetLastError() << std::endl;
       return 1;
   }
   ```
   - If `oldMask == 0`, affinity setting failed.

4. **Confirm Execution on Core 0**  
   ```cpp
   DWORD cpu = GetCurrentProcessorNumber();
   std::cout << "Running on CPU (thread) " << cpu << " (should be 0)" << std::endl;
   ```
   - Calls `GetCurrentProcessorNumber()` to verify that the thread is indeed running on CPU 0.

### **Linux Implementation (`#else`)**
1. **Initialize CPU Set**  
   ```cpp
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(0, &cpuset); // Pin to CPU 0
   ```
   - `CPU_ZERO(&cpuset)` initializes an empty CPU set.
   - `CPU_SET(0, &cpuset)` adds CPU 0 to the set.

2. **Set Thread Affinity**  
   ```cpp
   int result = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
   ```
   - `sched_setaffinity(0, sizeof(cpu_set_t), &cpuset)` sets the calling thread’s CPU affinity to CPU 0.

3. **Check for Errors**  
   ```cpp
   if (result != 0) {
       std::cerr << "Failed to set affinity: " << strerror(errno) << std::endl;
       return 1;
   }
   ```
   - If `result != 0`, affinity setting failed.

4. **Confirm Execution on Core 0**  
   ```cpp
   int cpu = sched_getcpu();
   std::cout << "Running on CPU " << cpu << " (should be 0)" << std::endl;
   ```
   - Calls `sched_getcpu()` to verify that the thread is actually running on CPU 0.
