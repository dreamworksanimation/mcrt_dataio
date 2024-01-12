// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ValueTimeTracker.h"

#include <scene_rdl2/render/util/StrUtil.h>

#include <iostream>
#include <sstream>

// This directive set memory pool logic ON for ValueTimeEvent and this should be
// enabled for the release version.
#define ENABLE_MEMPOOL

namespace mcrt_dataio {

auto secToMicrosec = [](double sec) { return static_cast<uint64_t>(sec * 1000000.0); };
auto microsecToSec = [](uint64_t microsec) { return static_cast<double>(microsec) * 0.000001; };

//------------------------------------------------------------------------------------------
    
float
ValueTimeEvent::getResidualSec() const
{
    double secDouble = microsecToSec(mTimeStamp);
    int secInt = static_cast<int>(secDouble);
    return static_cast<float>(secDouble - static_cast<double>(secInt));
}

std::string
ValueTimeEvent::show() const
{
    std::ostringstream ostr;
    ostr << "mTimeStamp:" << mTimeStamp << " mValue:" << mValue;
    return ostr.str();
}

std::string
ValueTimeEvent::show2(uint64_t baseTimeStamp) const
{
    uint64_t delta = mTimeStamp - baseTimeStamp;
    float deltaSec = static_cast<float>(delta) * 0.000001f;

    std::ostringstream ostr;
    ostr << "mTimeStamp:" << mTimeStamp << " (" << scene_rdl2::str_util::secStr(deltaSec) << ")"
         << " mValue:" << mValue;
    return ostr.str();
}

//------------------------------------------------------------------------------------------

void
ValueTimeTracker::push(float val)
{
    std::lock_guard<std::mutex> lock(mEventListMutex);

    if (mEventMaxVal < val) mEventMaxVal = val;

    ValueTimeEventShPtr event = getEvent();
    event->set(val);
    mEventList.push_front(event);
    cleanUpOverflow();
}

void
ValueTimeTracker::push(uint64_t timeStamp, float val)
{
    std::lock_guard<std::mutex> lock(mEventListMutex);

    if (mEventMaxVal < val) mEventMaxVal = val;

    ValueTimeEventShPtr event = getEvent();
    event->set(timeStamp, val);
    mEventList.push_front(event);
    cleanUpOverflow();
}

float
ValueTimeTracker::getMax() const
{
    std::lock_guard<std::mutex> lock(mEventListMutex);
    return mEventMaxVal;
}

float
ValueTimeTracker::getResampleValue(size_t totalResampleCount,
                                   std::vector<float>& outValTbl,
                                   float* max) const
{
    std::lock_guard<std::mutex> lock(mEventListMutex);
    
    if (outValTbl.size() < totalResampleCount) {
        outValTbl.resize(totalResampleCount);
    }
    std::memset(&outValTbl[0], 0x0, outValTbl.size() * sizeof(float)); // set all 0.0f

    if (mEventList.empty()) return 0.0f;
    if (!totalResampleCount) return 0.0f;

    double timeStepSec = static_cast<double>(mValueKeepDurationSec) / static_cast<double>(totalResampleCount);

    auto rValItr = mEventList.rbegin(); // reverse iterator
    float prevVal = 0.0f;
    uint64_t prevValTimeStamp = 0;

    /* Useful debug code
    auto showTimeStamp = [&](uint64_t timeStamp) {
        const auto& itr = mEventList.crbegin();
        uint64_t baseTimeStamp = (*itr)->getTimeStamp();
        uint64_t deltaTimeStamp = timeStamp - baseTimeStamp;
        float deltaTimeStampSec = microsecToSec(deltaTimeStamp);
        std::ostringstream ostr;
        ostr << timeStamp << " (" << scene_rdl2::str_util::secStr(deltaTimeStampSec) << ")";
        return ostr.str();
    };
    */

    auto getVal = [&](uint64_t startTimeStamp, uint64_t endTimeStamp) {
        auto calcWeight = [&](uint64_t deltaTimeStamp) { return microsecToSec(deltaTimeStamp) / timeStepSec; };
        auto incrementCurr = [&](float currVal, uint64_t currValTimeStamp) {
            prevVal = currVal;
            prevValTimeStamp = currValTimeStamp;
            if (rValItr == mEventList.rend()) return false; // end of list.
            ++rValItr;
            return true;
        };
        auto max = [](uint64_t a, uint64_t b) { return (a < b) ? b : a; };

        if (rValItr == mEventList.rend()) {
            return prevVal;
        }

        float valTotal = 0.0f;
        while (true) {
            uint64_t currValTimeStamp = (*rValItr)->getTimeStamp();
            float currVal = (*rValItr)->getValue();

            /* Useful debug code
            std::cerr << "= = = = =\n"
                      << "prevVal:" << prevVal << " prevValTimeStamp:" << showTimeStamp(prevValTimeStamp)
                      << " currVal:" << currVal << " currValTimeStamp:" << showTimeStamp(currValTimeStamp) << '\n';
            std::cerr << "startTimeStamp:" << showTimeStamp(startTimeStamp)
                      << " endTimeStamp:" << showTimeStamp(endTimeStamp) << '\n';
            */

            if (endTimeStamp <= currValTimeStamp) {
                if (prevValTimeStamp <= startTimeStamp) {
                    valTotal = prevVal;
                } else {
                    valTotal += prevVal * calcWeight(endTimeStamp - prevValTimeStamp);
                }
                break;
            } else if (startTimeStamp < currValTimeStamp) {
                valTotal += prevVal * calcWeight(currValTimeStamp - max(startTimeStamp, prevValTimeStamp));
                if (!incrementCurr(currVal, currValTimeStamp)) {
                    valTotal += prevVal * calcWeight(endTimeStamp - prevValTimeStamp);
                    break;
                }
            } else { // currValTimeStamp <= startTimeStamp
                if (!incrementCurr(currVal, currValTimeStamp)) {
                    valTotal = prevVal;
                    break;
                }                        
            }
        }
        return valTotal;
    };

    uint64_t endTimeStamp = mEventList.front()->getTimeStamp();
    uint64_t startTimeStamp = endTimeStamp - secToMicrosec(static_cast<double>(mValueKeepDurationSec));

    for (size_t id = 0; id < totalResampleCount; ++id) {
        double currStartOffsetSec = timeStepSec * static_cast<double>(id);
        double currEndOffsetSec = currStartOffsetSec + timeStepSec;
        uint64_t currStart = startTimeStamp + secToMicrosec(currStartOffsetSec);
        uint64_t currEnd = startTimeStamp + secToMicrosec(currEndOffsetSec);
        outValTbl[id] = getVal(currStart, currEnd);
    }

    if (max) *max = mEventMaxVal;

    return mEventList.front()->getResidualSec();
}

void
ValueTimeTracker::getResampleValueExhaust(size_t totalResampleCount, std::vector<float>& outValTbl) const
//
// This function is the same as getResampleValue() but the implementation is brute force and slow.
// This should be used for debugging/verifying purposes only.
//
{
    std::lock_guard<std::mutex> lock(mEventListMutex);

    if (outValTbl.size() < totalResampleCount) {
        outValTbl.resize(totalResampleCount);
    }
    std::memset(&outValTbl[0], 0x0, outValTbl.size() * sizeof(float)); // set all 0.0f

    if (mEventList.empty()) return;
    if (!totalResampleCount) return;

    double timeStepSec = static_cast<double>(mValueKeepDurationSec) / static_cast<double>(totalResampleCount);

    auto getValExhaust = [&](uint64_t startTimeStamp, uint64_t endTimeStamp) {
        auto calcWeight = [&](uint64_t deltaTimeStamp) { return microsecToSec(deltaTimeStamp) / timeStepSec; };
        auto calcSegmentVal = [&](uint64_t prevValTimeStamp, float prevVal,
                                  uint64_t currValTimeStamp, float currVal) {
            if (endTimeStamp <= prevValTimeStamp) return 0.0f;
            if (currValTimeStamp <= startTimeStamp) return 0.0f;
            if (prevValTimeStamp <= startTimeStamp) {
                if (currValTimeStamp < endTimeStamp) {
                    return static_cast<float>(prevVal * calcWeight(currValTimeStamp - startTimeStamp));
                } else {
                    return prevVal; // endTimeStamp <= currValTimeStamp
                }
            } else {  // startTimeStamp < prevValTimeStamp
                if (currValTimeStamp <= endTimeStamp) {
                    return static_cast<float>(prevVal * calcWeight(currValTimeStamp - prevValTimeStamp));
                } else { // endTimeStamp < currValTimeStamp
                    return static_cast<float>(prevVal * calcWeight(endTimeStamp - prevValTimeStamp));
                }
            }
        };

        if (mEventList.size() < 1) return 0.0f;
        if (mEventList.size() == 1) {
            uint64_t currValTimeStamp = mEventList.front()->getTimeStamp();
            float currVal = mEventList.front()->getValue();
            if (endTimeStamp < currValTimeStamp) return 0.0f;
            if (currValTimeStamp < startTimeStamp) return currVal;
            return static_cast<float>(currVal * calcWeight(endTimeStamp - currValTimeStamp));
        } else {
            auto currValItr = mEventList.begin();
            float valTotal = 0.0f;
            while (true) {
                uint64_t currValTimeStamp = (*currValItr)->getTimeStamp();
                float currVal = (*currValItr)->getValue();
                ++currValItr;
                if (currValItr == mEventList.end()) break; // end of data
                uint64_t prevValTimeStamp = (*currValItr)->getTimeStamp();
                float prevVal = (*currValItr)->getValue();
                valTotal += calcSegmentVal(prevValTimeStamp, prevVal, currValTimeStamp, currVal);
            }

            uint64_t lastValTimeStamp = mEventList.front()->getTimeStamp();
            float lastVal = mEventList.front()->getValue();
            if (lastValTimeStamp <= startTimeStamp) {
                valTotal = lastVal;
            } else if (lastValTimeStamp < endTimeStamp) {
                valTotal += lastVal * calcWeight(endTimeStamp - lastValTimeStamp);
            }
            return valTotal;
        }
    };

    uint64_t endTimeStamp = mEventList.front()->getTimeStamp();
    uint64_t startTimeStamp = endTimeStamp - secToMicrosec(static_cast<double>(mValueKeepDurationSec));

    for (size_t id = 0; id < totalResampleCount; ++id) {
        double currStartOffsetSec = timeStepSec * static_cast<double>(id);
        double currEndOffsetSec = currStartOffsetSec + timeStepSec;
        uint64_t currStart = startTimeStamp + secToMicrosec(currStartOffsetSec);
        uint64_t currEnd = startTimeStamp + secToMicrosec(currEndOffsetSec);
        outValTbl[id] = getValExhaust(currStart, currEnd);
    }
}

std::string
ValueTimeTracker::show() const
{
    std::ostringstream ostr;
    ostr << "ValueTimeTracker {\n"
         << "  mValueKeepDurationSec:" << mValueKeepDurationSec << '\n'
         << "  mEventMaxVal:" << mEventMaxVal << '\n'
         << scene_rdl2::str_util::addIndent(showEventList()) << '\n'
         << "  mMaxEventMemPool:" << mMaxEventMemPool << '\n'
         << "  mEventMemPool size:" << getEventMemPoolTotal() << '\n'
         << "}";
    return ostr.str();
}

void
ValueTimeTracker::cleanUpOverflow()
{
    ValueTimeEventShPtr lastEvent = nullptr;
    while (mEventList.size() > 2) {
        if (getDeltaSecWhole() <= static_cast<double>(mValueKeepDurationSec)) break;
        if (lastEvent) setEvent(lastEvent);
        lastEvent = mEventList.back();
        mEventList.pop_back();
    }
    if (lastEvent) mEventList.push_back(lastEvent);
}

double
ValueTimeTracker::getDeltaSecWhole() const
{
    return getDeltaSec(mEventList.front()->getTimeStamp(), mEventList.back()->getTimeStamp());
}

// static function
double
ValueTimeTracker::getDeltaSec(const uint64_t currTime, const uint64_t oldTime) // both microSec
{
    return static_cast<double>(currTime - oldTime) * 0.000001; // return sec
}

size_t
ValueTimeTracker::getTotal() const
{
    std::lock_guard<std::mutex> lock(mEventListMutex);
    return mEventList.size();
}

size_t
ValueTimeTracker::getEventMemPoolTotal() const
{
    std::lock_guard<std::mutex> lock(mEventListMutex);
    return mEventMemPool.size();
}

ValueTimeTracker::ValueTimeEventShPtr
ValueTimeTracker::getEvent()
{
    if (mEventMemPool.empty()) {
        return std::make_shared<ValueTimeEvent>();
    }

    ValueTimeEventShPtr event = mEventMemPool.front();
    mEventMemPool.pop_front();
    return event;
}

void
ValueTimeTracker::setEvent(ValueTimeEventShPtr ptr)
{
#   ifdef ENABLE_MEMPOOL
    mEventMemPool.push_front(ptr);
    if (mEventMemPool.size() > mMaxEventMemPool) {
        mMaxEventMemPool = mEventMemPool.size();
    }
#   else // else ENABLE_MEMPOOL
    ptr.reset();
#   endif // end else ENABLE_MEMPOOL
}

std::string
ValueTimeTracker::showEventList() const
{
    std::lock_guard<std::mutex> lock(mEventListMutex);

    if (mEventList.empty()) {
        return "mEventList is empty";
    }

    int w = scene_rdl2::str_util::getNumberOfDigits(mEventList.size() - 1);
    
    uint64_t baseTimeStamp = mEventList.back()->getTimeStamp();

    std::ostringstream ostr;
    ostr << "mEventList (size:" << mEventList.size() << ") {\n";
    size_t id = 0;
    for (const auto& itr : mEventList) {
        ostr << "  id:" << std::setw(w) << id << ' ' << itr->show2(baseTimeStamp) << '\n';
        ++id;
    }
    ostr << "}";
    return ostr.str();
}

std::string
ValueTimeTracker::showEventListReverse() const
{
    std::lock_guard<std::mutex> lock(mEventListMutex);

    if (mEventList.empty()) {
        return "mEventList is empty";
    }

    int w = scene_rdl2::str_util::getNumberOfDigits(mEventList.size() - 1);
    
    uint64_t baseTimeStamp = mEventList.back()->getTimeStamp();

    std::ostringstream ostr;
    ostr << "mEventList reverse list (size:" << mEventList.size() << ") {\n";
    size_t id = mEventList.size() - 1;
    for (auto rItr = mEventList.rbegin() ; rItr != mEventList.rend(); ++rItr) {
        ostr << "  id:" << std::setw(w) << id << ' ' << (*rItr)->show2(baseTimeStamp) << '\n';        
        --id;
    }
    ostr << "}";
    return ostr.str();
}

std::string
ValueTimeTracker::showTotal() const
{
    std::ostringstream ostr;
    ostr << getTotal();
    return ostr.str();
}

std::string
ValueTimeTracker::showLastResidualSec() const
{
    std::lock_guard<std::mutex> lock(mEventListMutex);

    float sec = mEventList.front()->getResidualSec();
    return std::to_string(sec);
}

void
ValueTimeTracker::parserConfigure()
{
    mParser.description("valueTimeTracker command");

    mParser.opt("totalEventList", "", "show total event list",
                [&](Arg& arg) { return arg.msg(showTotal() + '\n'); });
    mParser.opt("showEventList", "", "show all eventList",
                [&](Arg& arg) { return arg.msg(showEventList() + '\n'); });
    mParser.opt("showEventListReverse", "", "reverse show all eventList",
                [&](Arg& arg) { return arg.msg(showEventListReverse() + '\n'); });
    mParser.opt("max", "", "show max value",
                [&](Arg& arg) { return arg.msg(std::to_string(getMax()) + '\n'); });
    mParser.opt("residualSec", "", "show last event's residual sec",
                [&](Arg& arg) { return arg.msg(showLastResidualSec() + '\n'); });
    mParser.opt("show", "", "show all info",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
}

} // namespace mcrt_dataio
