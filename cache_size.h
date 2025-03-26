#pragma once

#include <vector>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fstream>
    #include <string>
#endif


int get_L1_cache_associativity(int core_id) {
    int associativity = -1;

#ifdef _WIN32
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer[128];
    DWORD len = sizeof(buffer);

    if (!GetLogicalProcessorInformationEx(RelationCache, buffer, &len)) {
        std::cerr << "Failed to get cache info.\n";
        return -1;
    }

    for (BYTE *ptr = reinterpret_cast<BYTE *>(buffer); ptr < reinterpret_cast<BYTE *>(buffer) + len;) {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(ptr);
        if (info->Relationship == RelationCache && info->Cache.Level == 1) {
            return info->Cache.Associativity;
        }
        ptr += info->Size;
    }

    return -1;
#else
    std::string assoc_path = "/sys/devices/system/cpu/cpu" + std::to_string(core_id) + "/cache/index0/ways_of_associativity";
    std::ifstream assoc_file(assoc_path);

    if (assoc_file.is_open()) {
        assoc_file >> associativity;
        assoc_file.close();
    } else {
        std::cerr << "Failed to read cache associativity.\n";
    }

    return associativity;
#endif
}

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
