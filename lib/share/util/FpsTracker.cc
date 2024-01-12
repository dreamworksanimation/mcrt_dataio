// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "FpsTracker.h"
#include "MiscUtil.h"

#include <scene_rdl2/render/util/StrUtil.h>

namespace mcrt_dataio {

void
FpsTracker::set()
{
    mEventQueue.push(MiscUtil::getCurrentMicroSec());

    while (mEventQueue.size() > 10) {
        if (getDeltaSecWhole() > static_cast<double>(mKeepIntervalSec)) {
            mEventQueue.pop();
        } else {
            break;
        }
    }
}

float
FpsTracker::getFps() const
{
    if (mEventQueue.empty()) return 0.0f;

    double wholeSec = getDeltaSecWhole();
    if (wholeSec <= 0.0) return 0.0f;
    return static_cast<float>(static_cast<double>(mEventQueue.size()) / wholeSec);
}

std::string    
FpsTracker::show() const
{
    auto showEventQueue = [&]() {
        std::queue<uint64_t> currQueue = mEventQueue;
        std::ostringstream ostr;
        ostr << "mEventQueue (size:" << currQueue.size() << ") {\n";
        unsigned i = 0;
        int w = scene_rdl2::str_util::getNumberOfDigits(currQueue.size() - 1);
        while (!currQueue.empty()) {
            ostr << "  i:" << std::setw(w) << i
                 << ' ' << MiscUtil::timeFromEpochStr(currQueue.front()) << '\n';
            currQueue.pop();
            i++;
        }
        ostr
        << "}"
        << " getDeltaSecWhole():" << getDeltaSecWhole() << " sec";
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "FpsTracker {\n"
         << "  mKeepIntervalSec:" << mKeepIntervalSec << " sec\n";
    if (mEventQueue.empty()) {
        ostr << "  mEventQueue is empty\n";
    } else {
        ostr << scene_rdl2::str_util::addIndent(showEventQueue()) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

} // namespace mcrt_dataio

