// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ClientReceiverStats.h"

#include <scene_rdl2/common/grid_util/LatencyLog.h>

#include <iomanip>
#include <sstream>

namespace mcrt_dataio {

void
ClientReceiverStats::updateMsgInterval()
{
    if (mRecvMsgIntervalTime.isInit()) {
        mRecvMsgIntervalAll = 0.0f;
        mRecvMsgIntervalTotal = 0;
    } else {
        mRecvMsgIntervalAll += mRecvMsgIntervalTime.end();
        mRecvMsgIntervalTotal++;
    }
    mRecvMsgIntervalTime.start();
}

void
ClientReceiverStats::updateLatency(const float latencySec)
{
    mLatencyAll += latencySec;
    mLatencyTotal++;

    /* useful debug message
    std::cerr << ">> ClientReceiverStats.cc"
              << " latencySec:" << latencySec
              << " mLatencyAll:" << mLatencyAll
              << " mLatencyTotal:" << mLatencyTotal << std::endl;
    */
}

void
ClientReceiverStats::updateRecvMsgSize(const uint64_t byte)
{
    mRecvMsgSizeAll += byte;
    mRecvMsgSizeTotal++;
}

std::string
ClientReceiverStats::show(const float elapsedSecFromStart) const
{
    std::ostringstream ostr;

    uint64_t aveRecvMsgSizeByte = calcAveRecvMsgSize();
    ostr << "time:" << std::setw(5) << std::fixed << std::setprecision(2) << elapsedSecFromStart << "sec"
         << " latency:" << std::setw(6) << std::fixed << std::setprecision(2) << calcAveLatency() << "ms"
         << " fps:" << std::setw(5) << std::fixed << std::setprecision(2) << calcFps()
         << " msgSize:" << byteStr(aveRecvMsgSizeByte)
         << " (" << bpsStr(calcBps()) << ")";

    /* useful debug message
    ostr << elapsedSecFromStart << ' '
         << calcAveLatency() << ' '
         << calcFps() << ' '
         << aveRecvMsgSizeByte / 1024.0 / 1024.0 << ' ' // MByte
         << aveRecvmsgSizeByte / 1024.0;                // KByte
    */
    
    return ostr.str();
}

std::string
ClientReceiverStats::byteStr(const uint64_t size) const
//
// Convert byte size info to string
//
{
    std::ostringstream ostr;
    if (size < (uint64_t)1024) {
        ostr << size << " Bytes";
    } else if (size < (uint64_t)1024 * (uint64_t)1024) {
        double f = (double)size / 1024.0;
        ostr << std::setw(3) << std::fixed << std::setprecision(2) << f << " KBytes";
    } else if (size < (uint64_t)1024 * (uint64_t)1024 * (uint64_t)1024) {
        double f = (double)size / 1024.0 / 1024.0;
        ostr << std::setw(3) << std::fixed << std::setprecision(2) << f << " MBytes";
    } else {
        double f = (double)size / 1024.0 / 1024.0 / 1024.0;
        ostr << std::setw(3) << std::fixed << std::setprecision(2) << f << " GBytes";
    }
    return ostr.str();
}

std::string
ClientReceiverStats::bpsStr(const float bps) const
//
// Convert byte/sec info to string
//
{
    std::ostringstream ostr;
    if (bps < 1024.0) {
        ostr << bps << " Byte/sec";
    } else if (bps < 1024.0f * 1024.0f) {
        double f = bps / 1024.0;
        ostr << std::setw(3) << std::fixed << std::setprecision(2) << f << " KBytes/sec";
    } else if (bps < 1024.0f * 1024.0f * 1024.0f) {
        double f = bps / 1024.0 / 1024.0;
        ostr << std::setw(3) << std::fixed << std::setprecision(2) << f << " MBytes/sec";
    } else {
        double f = bps / 1024.0 / 1024.0 / 1024.0;
        ostr << std::setw(3) << std::fixed << std::setprecision(2) << f << " GBytes/sec";
    }
    return ostr.str();
}

} // namespace mcrt_dataio
