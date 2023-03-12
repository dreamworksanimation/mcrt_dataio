// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "SysUsage.h"

#include <fstream>
#include <sstream>
#include <thread>

#include <sys/sysinfo.h> // sysinfo

namespace mcrt_dataio {

// static function
int
SysUsage::cpuTotal()
{
    return std::thread::hardware_concurrency(); // return HTcore total
}

bool
SysUsage::isCpuUsageReady() const
{
    // There is no particular meaning of 256 values. This should be pretty small and non zero.
    // The number of clock ticks per second can be obtained using : sysconf(_SC_CLK_TCK)
    constexpr clock_t minInterval = 256; // clock ticks
    
    clock_t now = times(nullptr); // get current time    
    return (now - mPrevTime > minInterval);
}

float
SysUsage::cpu()
// return CPU usage fraction 0.0~1.0, negative value is error
{
    size_t currTick;
    {
        std::ifstream ifs("/proc/stat");
        if (!ifs) return -1.0f; // error
        std::string line;
        if (!getline(ifs, line)) return -1.0f; // error

        std::istringstream istr(line);
        std::string token;
        size_t usr, nice, sys;
        istr >> token >> usr >> nice >> sys;
        currTick = usr + nice + sys;
    }

    clock_t now = times(nullptr); // get current time

    float fraction = 0.0f;
    if (mPrevTick) {
        float deltaTime = (float)(now - mPrevTime);
        float deltaTick = (float)(currTick - mPrevTick);
        fraction = (deltaTick / deltaTime) / (float)cpuTotal();
        /* useful debug message
        std::cerr << ">> SysUsage.cc"
                  << " fraction:" << fraction
                  << " deltaTick:" << deltaTick
                  << " deltaTime:" << deltaTime
                  << " nCpu:" << cpuTotal()
                  << std::endl;
        */
    }
    mPrevTick = currTick;
    mPrevTime = now;
        
    return fraction;
}

// static function
size_t
SysUsage::memTotal()
{
    struct sysinfo info;
    sysinfo(&info);

    size_t totalRam = (info.totalram * info.mem_unit); // Byte
    return totalRam;
}

// static function
float
SysUsage::mem()
// return memory usage fraction 0.0~1.0
{
    struct sysinfo info;
    sysinfo(&info);

    size_t totalRam = (info.totalram * info.mem_unit) / 1024; // KByte
    size_t freeRam = (info.freeram * info.mem_unit) / 1024; // KByte

    return (float)(totalRam - freeRam) / (float)totalRam;
}

} // namespace mcrt_dataio

