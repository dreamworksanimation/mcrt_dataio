// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "MergeStats.h"

#include <iomanip>
#include <sstream>

namespace mcrt_dataio {

std::string
MergeStats::show(const float elapsedSecFromStart) const
{
    std::ostringstream ostr;

    ostr << "time:" << std::setw(5) << std::fixed << std::setprecision(2) << elapsedSecFromStart << "sec"
         << " fps:" << std::setw(5) << std::fixed << std::setprecision(2) << calcFps()
         << " msgSize:" << byteStr(calcAveSendMsgSize())
         << " (" << bpsStr(calcBps()) << ")";

    /* useful debug dump
    ostr << elapsedSecFromStart << ' '
         << calcAveLatency() << ' '
         << calcFps() << ' '
         << aveRecvMsgSizeByte / 1024.0 / 1024.0 << ' ' // MByte
         << aveRecvmsgSizeByte / 1024.0;                // KByte
    */
    
    return ostr.str();
}

std::string
MergeStats::byteStr(const uint64_t size) const
//
// Byte size to string
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
MergeStats::bpsStr(const float bps) const
//
// Byte per sec to string
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

