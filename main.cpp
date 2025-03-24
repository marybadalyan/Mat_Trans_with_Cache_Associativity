#include <iostream>
#include "Rec_MatMul.h"  // Assuming this is your custom header
#include "kaizen.h"     // Assuming this is your custom header
#include <format>

using namespace MatMath;

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

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
    Mat Tm = MatMath::BlockedTPIt(m1);
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