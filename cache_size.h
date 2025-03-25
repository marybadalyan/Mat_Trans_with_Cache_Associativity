#pragma once

#include <vector>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fstream>
    #include <string>
#endif

size_t getL1CacheSize() {
#ifdef _WIN32
    DWORD bufferSize = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &bufferSize);

    if (bufferSize == 0) return 0;  // No cache info available

    std::vector<uint8_t> buffer(bufferSize);
    auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());

    if (!GetLogicalProcessorInformationEx(RelationCache, info, &bufferSize)) {
        return 0;
    }

    size_t l1CacheSize = 0;
    for (DWORD offset = 0; offset < bufferSize; offset += info->Size) {
        if (info->Relationship == RelationCache && info->Cache.Level == 1) {
            l1CacheSize = info->Cache.CacheSize;
            break;
        }
        info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            reinterpret_cast<uint8_t*>(info) + info->Size);
    }
    return l1CacheSize;

#else
    std::ifstream file("/sys/devices/system/cpu/cpu0/cache/index0/size");
    if (!file.is_open()) return 0;

    std::string sizeStr;
    file >> sizeStr;
    file.close();

    // Convert "32K" or "256K" to bytes
    size_t l1CacheSize = 0;
    if (sizeStr.back() == 'K' || sizeStr.back() == 'k') {
        l1CacheSize = std::stoul(sizeStr) * 1024;
    } else {
        l1CacheSize = std::stoul(sizeStr);
    }

    return l1CacheSize;
#endif
}
