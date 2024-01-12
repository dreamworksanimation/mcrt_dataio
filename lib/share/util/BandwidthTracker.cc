// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "BandwidthTracker.h"

#include <scene_rdl2/render/util/StrUtil.h>

namespace mcrt_dataio {
    
void
BandwidthTracker::set(size_t dataSize)
{
    mEventList.emplace_front(std::make_shared<BandwidthEvent>(dataSize));

    while (mEventList.size() > 10) {
        if (getDeltaSecWhole() > static_cast<double>(mKeepIntervalSec)) {
            mEventList.pop_back();
        } else {
            break;
        }
    }
}

float
BandwidthTracker::getBps() const
{
    if (mEventList.empty()) return 0.0f;

    double wholeSec = getDeltaSecWhole();
    if (wholeSec <= 0.0) return 0.0f;

    size_t sum = getDataSizeWhole();
    if (sum == 0) return 0.0f;
    return static_cast<float>(static_cast<double>(sum) / wholeSec);
}

size_t
BandwidthTracker::getMaxSize() const
{
    size_t max = 0;
    for (auto& itr : mEventList) {
        if (itr->getDataSize() > max) max = itr->getDataSize();
    }
    return max;
}

size_t
BandwidthTracker::getDataSizeWhole() const
{
    size_t sum = 0;
    for (auto& itr : mEventList) {
        sum += itr->getDataSize();
    }
    return sum;
}

double
BandwidthTracker::getDeltaSecWhole() const
{
    return getDeltaSec(mEventList.front()->getTimeStamp(), mEventList.back()->getTimeStamp());
}

// static function
double
BandwidthTracker::getDeltaSec(const uint64_t currTime, const uint64_t oldTime) // both microSec
{
    return static_cast<double>(currTime - oldTime) * 0.000001; // return sec
}

std::string
BandwidthTracker::show() const
{
    auto showEventList = [&]() -> std::string {
        using scene_rdl2::str_util::getNumberOfDigits;
        using scene_rdl2::str_util::byteStr;
        std::ostringstream ostr;
        ostr << "mEventList (size:" << mEventList.size() << ") {\n";
        unsigned i = 0;
        int w0 = getNumberOfDigits(mEventList.size() - 1);
        int w1 = getNumberOfDigits(getMaxSize());
        for (auto& itr : mEventList) {
            ostr << "  i:" << std::setw(w0) << i
                 << " mDataSize:" << std::setw(w1) << itr->getDataSize()
                 << " mTimeStamp:" << MiscUtil::timeFromEpochStr(itr->getTimeStamp()) << '\n';
            i++;
        }
        ostr
        << "}"
        << " getDataSizeWhole():" << byteStr(getDataSizeWhole())
        << " getDeltaSecWhole():" << getDeltaSecWhole() << " sec";
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "BandwidthTracker {\n"
         << "  mKeepIntervalSec:" << mKeepIntervalSec << " sec : at least keep this interval data\n";
    if (mEventList.empty()) {
        ostr << "  mEventList is empty\n";
    } else {
        ostr << scene_rdl2::str_util::addIndent(showEventList()) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

} // namespace mcrt_dataio
