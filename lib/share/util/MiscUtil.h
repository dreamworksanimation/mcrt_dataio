// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <cstdint>              // uint64_t
#include <string>
#include <sys/time.h>           // struct timeval

namespace mcrt_dataio {

class MiscUtil
{
public:
    static uint64_t getCurrentMicroSec(); // return microsec from Epoch
    static float us2s(const uint64_t microsec); // microsec to sec
    static std::string timeFromEpochStr(const uint64_t microsecFromEpoch);
    static std::string timeFromEpochStr(const struct timeval &tv);
    static std::string currentTimeStr();
    static std::string secStr(const float sec);

    static std::string getHostName();     // return current hostName
};

} // namespace mcrt_dataio

