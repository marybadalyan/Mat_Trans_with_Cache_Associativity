
## Overview

***Cache associativity describes how a CPU cache organizes memory to map main memory addresses to cache locations. Implementing a naïve matrix transposition without considering cache organization details slows down the process due to set eviction. A blocked matrix transposition, where the block size is chosen to fully fit within the cache, improves performance for both row-major and column-major iterations in the original and transposed matrices.  

I am using a 13th-gen Intel Core i7-1355U processor, formally known as Raptor Lake-S. By referring to the processor documentation, I am focusing on ensuring that blocks from matrix A (row-major) and the corresponding transposed blocks in matrix B (column-major) fit within the 48 KB, 12-way set-associative L1 data cache. This approach prevents them from overwriting each other while accounting for the cache’s associativity and access patterns.***

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
  - **B (Column-Major)**: Non-sequential access, poor spatial locality (stride = \( N \times 4 \) bytes).
- **Goals**:
  1. **Fit Both Blocks in L1 Cache**: The block from A and the transposed block in B should fit in the 48 KB L1 cache.
  2. **Minimize Overwriting**: Ensure A and B’s cache lines don’t evict each other due to excessive conflicts in the 12-way set-associative cache.
  3. **Account for 12-Way Associativity**: Keep the number of cache lines per set within the 12-way limit.
  4. **Consider Row-Major and Column-Major Access**: Balance the access patterns to reduce conflicts.

### Step 2: Define the Block Size and Working Set
- **Block Size**:
  - Use a square block of size \( B \times B \).
  - Each element is an `int` (4 bytes).
  - Size of a \( B \times B \) block = \( B \times B \times 4 \) bytes.
  - Working set = block from A + block from B = \( 2 \times B \times B \times 4 \) bytes.
- **Fit in L1 Cache**:
  - Aim to use a significant portion of the 48 KB L1 cache, but leave room for other data (e.g., stack variables).
  - Let’s target a working set of ~40 KB to start, then adjust based on associativity:
    - \( 2 \times B \times B \times 4 \leq 40,000 \) bytes.
    - \( B \times B \times 8 \leq 40,000 \).
    - \( B \times B \leq 5,000 \).
    - \( B \leq \sqrt{5,000} \approx 70.7 \).
  - So, \( B = 70 \):
    - \( 70 \times 70 = 4,900 \) elements.
    - \( 4,900 \times 4 = 19,600 \) bytes per block.
    - Working set = \( 2 \times 19,600 = 39,200 \) bytes (39.2 KB), 81.7% of 48 KB.
- **Cache Lines**:
  - 19,600 bytes ÷ 64 = 306.25 ≈ 307 cache lines per block.
  - Total cache lines = 307 (A) + 307 (B) = 614 cache lines.
  - L1 cache has 768 cache lines, so 614 cache lines fit (79.9% of the cache’s lines).

#### Issue with 70×70: Conflicts
- **A (Row-Major)**:
  - 307 cache lines across 64 sets: 307 mod 64 = 51 sets, ~4.8 cache lines per set, within the 12-way limit.
- **B (Column-Major)**:
  - For \( N = 512 \):
    - Stride = \( 512 \times 4 = 2,048 \) bytes = 32 cache lines.
    - Set increment = 32 mod 64 = 32.
    - Sets repeat every 64 ÷ gcd(32, 64) = 64 ÷ 32 = 2 rows.
    - 70 rows: Even rows (0, 2, ..., 68) = 35 rows → set 0; odd rows (1, 3, ..., 69) = 35 rows → set 32.
    - 35 cache lines per set, exceeding the 12-way limit, causing 35 - 12 = 23 evictions per set.
  - Total evictions = 23 × 2 sets = 46 evictions per block.
- **Problem**: The 46 evictions per block mean A and B’s cache lines are evicting each other, leading to thrashing and overwriting, which doesn’t meet your goal.

---

### Step 3: Align Block Size with 12-Way Associativity
To minimize conflicts and prevent overwriting, we need a block size \( B \times B \) where the number of rows \( B \) in B’s column-major access results in a number of cache lines per set that fits within the 12-way associativity limit.

#### B’s Column-Major Access Pattern
- **Stride**:
  - Stride between consecutive writes to B = \( N \times 4 \) bytes.
  - For \( N = 512 \):
    - Stride = \( 512 \times 4 = 2,048 \) bytes.
    - Cache lines = \( 2,048 \div 64 = 32 \).
    - Set increment = 32 mod 64 = 32.
- **Set Repetition**:
  - Sets repeat every 64 ÷ gcd(32, 64) = 64 ÷ 32 = 2 rows.
  - So, rows 0, 2, 4, ... map to one set (e.g., set 0), and rows 1, 3, 5, ... map to another set (e.g., set 32).
- **Rows per Set**:
  - If we access \( R \) rows in B:
    - Number of rows per set = \( R \div 2 \).
    - Number of cache lines per set = \( R \div 2 \) (since each row is a new cache line due to the large stride).
  - We want the number of cache lines per set to be ≤ 12 to avoid evictions:
    - \( R \div 2 \leq 12 \).
    - \( R \leq 24 \).
- **Choose \( R = 24 \)**:
  - 24 rows → \( 24 \div 2 = 12 \) cache lines per set, exactly matching the 12-way associativity, causing 0 evictions within B’s access pattern.

#### Block Size: 24×24
- **Working Set**:
  - 24×24 block = \( 24 \times 24 = 576 \) `int` elements.
  - \( 576 \times 4 = 2,304 \) bytes per block.
  - Total working set = \( 2,304 + 2,304 = 4,608 \) bytes (4.608 KB), 9.6% of 48 KB.
- **Cache Lines**:
  - 2,304 bytes ÷ 64 = 36 cache lines per block.
  - Total cache lines = 36 (A) + 36 (B) = 72 cache lines.
- **A (Row-Major)**:
  - 24 rows, each row = 24 `int` = \( 24 \times 4 = 96 \) bytes = 1.5 cache lines ≈ 2 cache lines.
  - Total = 36 cache lines (since elements share cache lines).
  - Set mapping: Cache lines 0 to 35 → sets 0 to 35, ~1 cache line per set.
- **B (Column-Major)**:
  - 24 rows → 12 cache lines per set (as calculated), using 2 sets (e.g., sets 0 and 32).
- **Combined Conflicts**:
  - Set 0:
    - A: 1 cache line.
    - B: 12 cache lines.
    - Total = 1 + 12 = 13 cache lines, causing 13 - 12 = 1 eviction.
  - Set 32:
    - A: 1 cache line.
    - B: 12 cache lines.
    - Total = 13 cache lines, causing 1 eviction.
  - Total evictions = 1 × 2 sets = 2 evictions per block, minimal.


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
