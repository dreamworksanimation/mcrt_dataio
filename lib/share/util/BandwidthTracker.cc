// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "BandwidthTracker.h"

#include <iostream>
#include <sstream>

namespace mcrt_dataio {
    
void
BandwidthTracker::set(size_t dataSize)
{
    mEventList.emplace_front(std::make_shared<BandwidthEvent>(dataSize));

    while (mEventList.size() > 10) {
        if (deltaSecWhole() > mKeepIntervalSec) {
            mEventList.pop_back();
        } else {
            break;
        }
    }
}

float
BandwidthTracker::getBps()
{
    if (mEventList.empty()) return 0.0f;

    float wholeSec = deltaSecWhole();
    if (wholeSec <= 0.0f) return 0.0f;

    float sum = 0.0f;
    for (auto itr = mEventList.begin(); itr != mEventList.end(); ++itr) {
        sum += static_cast<float>((*itr)->getDataSize());
    }
    return sum / wholeSec;
}

float
BandwidthTracker::deltaSecWhole()
{
    return deltaSec(mEventList.front()->getTimeStamp(), mEventList.back()->getTimeStamp());
}

// static function
float
BandwidthTracker::deltaSec(const uint64_t currTime, const uint64_t oldTime) // both microSec
{
    return (float)(currTime - oldTime) * 0.000001f; // return sec
}

std::string
BandwidthTracker::show() const
{
    std::ostringstream ostr;
    ostr << "BandwidthTracker {\n"
         << "  mKeepIntervalSec:" << mKeepIntervalSec << " sec\n"
         << "  mEventList (size:" << mEventList.size() << ") {\n";
    for (auto itr = mEventList.begin(); itr != mEventList.end(); ++itr) {
        ostr << "    mDataSize:" << (*itr)->getDataSize()
             << " mTimeStamp:" << MiscUtil::timeFromEpochStr((*itr)->getTimeStamp()) << '\n';
    }
    ostr << "  }\n"
         << "}";
    return ostr.str();
}

} // namespace mcrt_dataio

