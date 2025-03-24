#include <iostream>
#include "Rec_MatMul.h"
#include "kaizen.h"
#include <format>
#include <windows.h>

using namespace MatMath;



std::pair<int, int> process_args(int argc, char* argv[]) {
    zen::cmd_args args(argv, argc);
    auto row_options = args.get_options("--rows");
    auto col_options = args.get_options("--cols");

    if (row_options.empty() || col_options.empty()) {
        zen::log("Error: --cols and/or --rows arguments are absent, using default 3000!");
        return {3000,3000};
    }

    try {
        int rows = std::stoi(row_options[0]);
        int cols  = std::stoi(col_options[0]);

        return {rows, cols};
    } catch (const std::exception& e) {
        zen::log("Invalid input detected. Using default values (150,150) for both rows and columns!");
        return {3000,3000};
    }
}

Mat naiveTP(Mat& m1){
    Mat temp(m1.cols, m1.rows);
    for(int i = 0; i < m1.rows; ++i){
        for(int j = 0; j < m1.cols; ++j){
            temp.matrix[j * m1.rows + i] = m1.matrix[i * m1.cols + j];
        }
    }
    return temp;
}


int main(int argc,char* argv[]) {
    auto [cols,rows] = process_args(argc,argv);

    Mat m1(cols,rows);
    // Get the current thread
    HANDLE thread = GetCurrentThread();

    // Set affinity to P-core 0, Thread 0 (logical processor 0)
    DWORD_PTR affinityMask = 1ULL << 0; // Bit 0 = Thread 0 (P-core 0, first thread)
    DWORD_PTR oldMask = SetThreadAffinityMask(thread, affinityMask);

    if (oldMask == 0) {
        std::cerr << "Failed to set affinity: " << GetLastError() << std::endl;
        return 1;   
    }

    std::cout << "Pinned to P-core 0, Thread 0 (logical processor 0)" << std::endl;

    // Verify the current thread
    DWORD cpu = GetCurrentProcessorNumber();
    std::cout << "Running on CPU (thread) " << cpu << " (should be 0)" << std::endl;


    zen::timer timer;
    timer.start();
    Mat Tm = MatMath::BlockedTPIt(m1);
    timer.stop();
    double Iterative = timer.duration<zen::timer::nsec>().count(); //using the double later 

    timer.start();
    naiveTP(m1);
    timer.stop();
    double Naive = timer.duration<zen::timer::nsec>().count(); //using the double later 

    zen::print(std::format("| {:<30} | {:>10.2f} |\n", "Iterative Transposition", Iterative));
    zen::print(std::format("| {:<30} | {:>10.2f} |\n", "Naive Transposition", Naive));
    zen::print(std::format("| {:<30} | {:>10.2f} |\n", "Speedup Factor", Iterative/Naive));


    return 0;
}