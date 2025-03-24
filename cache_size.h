#ifndef CACHE_INFO_H
#define CACHE_INFO_H

#include <cstdint>
#include <iostream>

#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace CacheDetector {

    // Cross-platform CPUID function
    inline void getCpuid(uint32_t func, uint32_t subfunc, uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx) {
#ifdef _WIN32
        int regs[4];
        __cpuidex(regs, func, subfunc);
        eax = regs[0];
        ebx = regs[1];
        ecx = regs[2];
        edx = regs[3];
#else
        eax = func;
        ecx = subfunc;
        __cpuid_count(func, subfunc, eax, ebx, ecx, edx);
#endif
    }

    // Get L1 cache size in KB
    inline uint32_t getL1CacheSize() {
        uint32_t size = 0;
        uint32_t eax, ebx, ecx, edx;

        // First check CPUID leaf 2 (older method)
        getCpuid(2, 0, eax, ebx, ecx, edx);

        uint8_t* descriptors = reinterpret_cast<uint8_t*>(&eax);
        for (int i = 0; i < 4; i++) {
            if (descriptors[i] == 0x06) {  // L1 Instruction cache: 8KB
                size = 8;
                break;
            }
            else if (descriptors[i] == 0x08) {  // L1 Instruction cache: 16KB
                size = 16;
                break;
            }
            else if (descriptors[i] == 0x0A) {  // L1 Data cache: 8KB
                size = 8;
                break;
            }
            else if (descriptors[i] == 0x0C) {  // L1 Data cache: 16KB
                size = 16;
                break;
            }
        }

        // Try newer method with leaf 4 if leaf 2 didn't work
        if (size == 0) {
            for (uint32_t i = 0; i < 4; i++) {
                getCpuid(0x4, i, eax, ebx, ecx, edx);  // Use sub-leaf index i

                uint32_t cacheType = (eax & 0x1F);         // Cache type (0 = null, 1 = data, 3 = unified)
                if (cacheType == 0) break;                 // No more caches to enumerate
                uint32_t cacheLevel = ((eax >> 5) & 0x7);  // Cache level

                if ((cacheType == 1 || cacheType == 3) && cacheLevel == 1) {  // L1 data or unified cache
                    uint32_t ways = ((ebx >> 22) & 0x3FF) + 1;  // Ways of associativity
                    uint32_t line = (ebx & 0xFFF) + 1;          // Cache line size in bytes
                    uint32_t sets = ecx + 1;                    // Number of sets
                    size = (ways * line * sets) / 1024;        // Total size in KB
                    break;
                }
            }
        }

        // Fallback if no size detected
        if (size == 0) {
            std::cerr << "Warning: Could not detect L1 cache size, defaulting to 32 KB\n";
            size = 32;  // Reasonable default for modern CPUs
        }
        return size;
    }

    // Static constant for L1 cache size in KB
    static const uint32_t CHUNK_SIZE = getL1CacheSize();

} // namespace CacheDetector

#endif // CACHE_INFO_H