// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "SysUsage.h"

#include <scene_rdl2/render/util/StrUtil.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

#include <sys/sysinfo.h> // sysinfo

namespace mcrt_dataio {

// static function
int
SysUsage::getCpuTotal()
{
    return std::thread::hardware_concurrency(); // return HTcore total
}

bool
SysUsage::isCpuUsageReady() const
{
    // There is no particular meaning of this value. This should be pretty small and non zero.
    // The number of clock ticks per second can be obtained using : sysconf(_SC_CLK_TCK)
    // (= 100 for example).
    constexpr clock_t minInterval = 16; // clock ticks
    
    clock_t now = times(nullptr); // get current time    
    return (now - mAll.getPrevTime() > minInterval);
}

float
SysUsage::getCpuUsage()
// return all CPU usage fraction 0.0~1.0, negative value is error
{
    std::ifstream ifs("/proc/stat");
    if (!ifs) return -1.0f; // error

    clock_t now = times(nullptr); // get current time

    std::string line;
    while (1) {
        if (!getline(ifs, line)) return false;

        std::istringstream istr(line);
        std::string token;
        size_t usr, nice, sys;
        istr >> token >> usr >> nice >> sys;

        if (token.substr(0, 3) != "cpu") break;

        size_t currTick = usr + nice + sys;
        
        if (token.size() == 3) {
            // cpu
            mAll.set(now, currTick, 1.0f / mCpuTotal);
        } else {
            // cpu#
            int cpuId = std::stoi(token.substr(3));
            mCores[cpuId].set(now, currTick, 1.0f);
        }
    }

    return mAll.getFraction();
}

std::vector<float>
SysUsage::getCoreUsage() const
{
    std::vector<float> vec(mCores.size(), 0.0f);
    for (size_t i = 0; i < mCores.size(); ++i) {
        vec[i] = mCores[i].getFraction();
    }
    return vec; // returns copy data
}

// static function
size_t
SysUsage::getMemTotal()
{
    struct sysinfo info;
    sysinfo(&info);

    size_t totalRam = (info.totalram * info.mem_unit); // Byte
    return totalRam;
}

// static function
float
SysUsage::getMemUsage()
// return memory usage fraction 0.0~1.0
{
    struct sysinfo info;
    sysinfo(&info);

    size_t totalRam = (info.totalram * info.mem_unit) / 1024; // KByte
    size_t freeRam = (info.freeram * info.mem_unit) / 1024; // KByte

    return (float)(totalRam - freeRam) / (float)totalRam;
}

bool
SysUsage::updateNetIO()
{
    size_t recv, send;
    if (!getNetIO(recv, send)) return false; // early exit

    if (recv == 0 || send == 0) {
        return false;
    }
    size_t deltaRecv = recv - mPrevNetRecv;
    size_t deltaSend = send - mPrevNetSend;
    if (deltaRecv == 0 || deltaSend == 0) {
        return false;
    } else {
        float now = mNetIoRecTime.end();
        float deltaTime = now - mPrevNetTime;

        mNetRecvBps = (!mPrevNetRecv) ? 0.0f : static_cast<float>(deltaRecv) / deltaTime;
        mPrevNetRecv = recv;

        mNetSendBps = (!mPrevNetSend) ? 0.0f : static_cast<float>(deltaSend) / deltaTime;
        mPrevNetSend = send;

        mPrevNetTime = now;
    }
    return true;
}

std::string
SysUsage::show() const
{
    auto showPct = [](float fraction) {
        if (fraction < 0.0f) fraction = 0.0f;
        std::ostringstream ostr;
        ostr << std::setw(6) << std::fixed << std::setprecision(2) << fraction * 100.0f << '%';
        return ostr.str();
    };

    int w = scene_rdl2::str_util::getNumberOfDigits(mCores.size());

    std::ostringstream ostr;
    ostr << "CpuUsage {\n"
         << " all:" << showPct(mAll.getFraction()) << '\n'
         << " cpuTotal:" << mCpuTotal << " {\n";
    for (size_t i = 0; i < mCores.size(); ++i) {
        ostr << "    i:" << std::setw(w) << mCores[i].getCpuId() << ' ' << showPct(mCores[i].getFraction()) << '\n';
    }
    ostr << "  }\n"
         << "}";
    return ostr.str();
}

// static function
bool
SysUsage::getNetIO(size_t& recv, size_t& send)
{
    auto isDevName = [](const std::string& name) {
        if (name.size() <= 1) return false;
        return (name[name.size() - 1] == ':');
    };

    std::ifstream ifs("/proc/net/dev");
    if (!ifs) return false;

    size_t recvMax {0};
    size_t sendMax {0};

    //
    // We want to try to get value without knowing the device's name.
    // This logic does not smart enough if we have 2 (or more) NIC actively working conditions.
    // Otherwise, as far as using a single NIC config, return the expected result.
    //                                               
    std::string line;
    while (1) {
        if (!getline(ifs, line)) break;

        std::istringstream istr(line);
        std::string devName;
        istr >> devName;
        if (isDevName(devName)) {
            size_t v0, v1, v2, v3, v4, v5, v6, v7, v8;
            istr >> v0 >> v1 >> v2 >> v3 >> v4 >> v5 >> v6 >> v7 >> v8;
            if (v0 > recvMax) recvMax = v0;
            if (v8 > sendMax) sendMax = v8;
        }
    }

    recv = recvMax;
    send = sendMax;

    return true;
}

} // namespace mcrt_dataio
