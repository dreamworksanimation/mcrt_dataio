// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "FpsTracker.h"
#include "MiscUtil.h"

#include <sstream>

namespace mcrt_dataio {

void
FpsTracker::set()
{
    mEventQueue.push(MiscUtil::getCurrentMicroSec());

    while (mEventQueue.size() > 10) {
        if (deltaSecWhole() > mKeepIntervalSec) {
            mEventQueue.pop();
        } else {
            break;
        }
    }
}

float
FpsTracker::getFps()
{
    if (mEventQueue.empty()) return 0.0f;

    float wholeSec = deltaSecWhole();
    if (wholeSec <= 0.0f) return 0.0f;
    return (float)mEventQueue.size() / wholeSec;
}

std::string    
FpsTracker::show() const
{
    std::ostringstream ostr;
    ostr << "FpsTracker {\n"
         << "  mKeepIntervalSec:" << mKeepIntervalSec << " sec\n"
         << "  mEventQueue.size():" << mEventQueue.size() << '\n'
         << "}";
    return ostr.str();
}

} // namespace mcrt_dataio

