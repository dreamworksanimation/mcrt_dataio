// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <cstddef>              // size_t
#include <sys/times.h>          // times() clock_t

namespace mcrt_dataio {

class SysUsage
//
// This class is designed in order to get system related usage and related information
// like CPU load and memory usage.
//
// How to get CPU usage
//   1) construct SysUsage object
//   2) If isCpuUsageReady() is false, wait some time and do 2) again
//   3) If isCpuUsageReady() is true, call cpu()
//   4) wait some time and goto 2) again if you want to get CPU usage multiple times
//
// How to get Memory usage
//   simply just call mem() anytime you want.
//
{
public:
    SysUsage() :
        mPrevTick(0),
        mPrevTime(0)
    {}

    static int cpuTotal(); // return HTcore total
    bool isCpuUsageReady() const;
    float cpu(); // return CPU usage fraction 0.0~1.0, negative value is error

    static size_t memTotal(); // return total RAM as byte
    static float mem(); // return memory usage fraction 0.0~1.0

private:
    size_t mPrevTick;           // previous cpu usage
    clock_t mPrevTime;          // previous time
};

} // namespace mcrt_dataio

