#include <iostream>
#include "Rec_MatMul.h"  // Assuming this is your custom header
#include "kaizen.h"     // Assuming this is your custom header
#include <format>
#include <cstring>      // Added for strerror on Linux
#include <numeric> //for gcd
using namespace MatMath;

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif
const int cacheSize = getL1CacheSize()*1024;

std::pair<int, int> calculateBlockSize(int M, int N) {
    const int lineSize = 64;
    const int associativity = 12;
    const int numSets = cacheSize / (lineSize * associativity); // 64

    // Source block stride (M columns)
    int linesPerRow = (M * 4 + lineSize - 1) / lineSize; // ceil(M / 16)
    int setIncrement = linesPerRow % numSets;
    int gcdValue = std::gcd(setIncrement, numSets);
    int repetitionPeriod = numSets / gcdValue;
    int maxS = associativity * repetitionPeriod;

    // Cap S by cache and matrix size
    int S = (std::min)(maxS, M); // Don’t exceed matrix width
    while (S > 1) {
        int maxLinesS = (S * S * 4 + lineSize - 1) / lineSize; // Source block lines
        if (maxLinesS / (std::min)(numSets, linesPerRow) <= associativity) break;
        S--;
    }

    // Cap R by cache and N, considering destination stride
    int R = (std::min)(N, cacheSize / (S * 4)); // Initial max based on cache
    while (R > 1) {
        int workingSetBytes = 2LL * R * S * 4;
        int maxLinesR = R * ((S * 4 + lineSize - 1) / lineSize); // Dest block lines
        int setsTouched = (std::min)(numSets, linesPerRow);
        if (workingSetBytes <= cacheSize && maxLinesR / setsTouched <= associativity) break;
        R--;
    }

    return {R, S};
}


//can use which does not consider cache organization sqrt((CacheDetector::getL1CacheSize()*1024)/12);

std::pair<int, int> process_args(int argc, char* argv[]) {
    zen::cmd_args args(argv, argc);
    auto row_options = args.get_options("--rows");
    auto col_options = args.get_options("--cols");

    if (row_options.empty() || col_options.empty()) {
        zen::log("Error: --cols and/or --rows arguments are absent, using default 3000!");
        return {3000, 3000};
    }

    try {
        int rows = std::stoi(row_options[0]);
        int cols = std::stoi(col_options[0]);
        return {rows, cols};
    } catch (const std::exception& e) {
        zen::log("Invalid input detected. Using default values (3000,3000) for both rows and columns!");
        return {3000, 3000};
    }
}

Mat naiveTP(Mat& m1) {
    Mat temp(m1.cols, m1.rows);
    for (int i = 0; i < m1.rows; ++i) {
        for (int j = 0; j < m1.cols; ++j) {
            temp.matrix[j * m1.rows + i] = m1.matrix[i * m1.cols + j];
        }
    }
    return temp;
}

int main(int argc, char* argv[]) {
    auto [cols, rows] = process_args(argc, argv);
    Mat m1(cols, rows);
    const std::pair<int,int> BLOCK_SIZE =  calculateBlockSize(cols,rows);
#ifdef _WIN32
    // Windows-specific thread affinity
    HANDLE thread = GetCurrentThread();
    DWORD_PTR affinityMask = 1ULL << 0; // Pin to logical processor 0
    DWORD_PTR oldMask = SetThreadAffinityMask(thread, affinityMask);

    if (oldMask == 0) {
        std::cerr << "Failed to set affinity: " << GetLastError() << std::endl;
        return 1;
    }
    std::cout << "Pinned to P-core 0, Thread 0 (logical processor 0)" << std::endl;
    DWORD cpu = GetCurrentProcessorNumber();
    std::cout << "Running on CPU (thread) " << cpu << " (should be 0)" << std::endl;
#else
    // Linux-specific thread affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset); // Pin to CPU 0

    int result = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "Failed to set affinity: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Pinned to CPU 0" << std::endl;
    int cpu = sched_getcpu();
    std::cout << "Running on CPU " << cpu << " (should be 0)" << std::endl;
#endif

    zen::timer timer;
    timer.start();
    Mat Tm = MatMath::BlockedTPIt(m1,BLOCK_SIZE);
    timer.stop();
    double Iterative = timer.duration<zen::timer::nsec>().count();

    timer.start();
    naiveTP(m1);
    timer.stop();
    double Naive = timer.duration<zen::timer::nsec>().count();

    zen::print(std::format("| {:<30} | {:>10.2f} |\n", "Iterative Transposition", Iterative));
    zen::print(std::format("| {:<30} | {:>10.2f} |\n", "Naive Transposition", Naive));
    zen::print(std::format("| {:<30} | {:>10.2f} |\n", "Speedup Factor", Iterative / Naive));

    return 0;
}