// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/common/rec_time/RecTime.h>

#include <cstddef>              // size_t
#include <sys/times.h>          // times() clock_t
#include <thread>
#include <vector>

namespace mcrt_dataio {

class CpuPerf
{
public:
    CpuPerf() = default;

    void setCpuId(int id) { mCpuId = id; }

    void set(const clock_t now, size_t currTick, float fractionScale) {
        if (!mPrevTick) {
            mFraction = 0.0f;
        } else {
            float deltaTime = static_cast<float>(now - mPrevTime);
            float deltaTick = static_cast<float>(currTick - mPrevTick);
            mFraction = (deltaTick / deltaTime) * fractionScale;
        }

        mPrevTick = currTick;
        mPrevTime = now;
    }

    int getCpuId() const { return mCpuId; }
    clock_t getPrevTime() const { return mPrevTime; }
    float getFraction() const { return mFraction; }

private:

    int mCpuId {-1};

    size_t mPrevTick {0};           // previous cpu usage
    clock_t mPrevTime {0};          // previous time

    float mFraction {0.0f};         // usage (fraction)
};

class SysUsage
//
// This class is designed in order to get system related usage and related information
// like CPU load and memory usage.
//
// How to get CPU usage
//   1) construct SysUsage object
//   2) If isCpuUsageReady() is false, wait some time and do 2) again
//   3) If isCpuUsageReady() is true, call getCpuUsage(), and/or getCoreUsage()
//   4) wait some time and goto 2) again if you want to get CPU usage multiple times
//
// How to get Memory usage
//   simply just call getMemUsage() anytime you want.
//
// How to get NetIO info
//   1) construct SysUsage object
//   2) call updateNetIO()
//   3) if updateNetIO() returns false, wait some time and do 2) again
//   4) if updateNetIO() returns true, call getNetRecv(), getNetSend()
//   5) wait some time and goto 2) again if you want to get NetIO info multiple times
//
{
public:
    SysUsage()
    {
        mCpuTotal = std::thread::hardware_concurrency();
        mCores.resize(mCpuTotal);
        for (int i = 0; i < mCores.size(); ++i) {
            mCores[i].setCpuId(i);
        }

        mNetIoRecTime.start();
    }

    static int getCpuTotal(); // return HTcore total
    bool isCpuUsageReady() const;
    float getCpuUsage(); // return CPU usage fraction 0.0~1.0, negative value is error
    std::vector<float> getCoreUsage() const;

    static size_t getMemTotal(); // return total RAM as byte
    static float getMemUsage(); // return memory usage fraction 0.0~1.0

    bool updateNetIO(); // update netIO information
    float getNetRecv() const { return mNetRecvBps; } // Byte/Sec
    float getNetSend() const { return mNetSendBps; } // Byte/Sec

    std::string show() const;

private:

    static bool getNetIO(size_t& recv, size_t& send);

    //------------------------------

    int mCpuTotal {1};

    CpuPerf mAll;
    std::vector<CpuPerf> mCores;

    scene_rdl2::rec_time::RecTime mNetIoRecTime;
    size_t mPrevNetRecv {0};
    size_t mPrevNetSend {0};
    float mNetRecvBps {0.0f};
    float mNetSendBps {0.0f};
    float mPrevNetTime {0.0f};
};

} // namespace mcrt_dataio
