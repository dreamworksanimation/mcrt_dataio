// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mcrt_dataio/share/util/MiscUtil.h>
#include <scene_rdl2/common/grid_util/Parser.h>

#include <cstdint> // uint64_t
#include <deque>
#include <list>
#include <memory> // shared_ptr
#include <mutex>
#include <vector>

namespace mcrt_dataio {

class ValueTimeEvent
{
public:
    ValueTimeEvent() = default;

    void set(float val) { set(MiscUtil::getCurrentMicroSec(), val); }
    void set(uint64_t timeStamp, float val) { mTimeStamp = timeStamp; mValue = val; }

    uint64_t getTimeStamp() const { return mTimeStamp; }
    float getResidualSec() const;
    float getValue() const { return mValue; }

    std::string show() const;
    std::string show2(uint64_t baseTimeStamp) const;

private:
    uint64_t mTimeStamp {0};
    float mValue {0.0f};
};

class ValueTimeTracker
//
// This class keeps float values with timestamps for user-defined duration.
// We can resample these values by particular time resolution.
// This class is used by telemetry bar-graph panel display logic mainly.
//
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;

    ValueTimeTracker(float valueKeepDurationSec)
        : mValueKeepDurationSec(valueKeepDurationSec)
    {
        parserConfigure();
    }

    float getValueKeepDurationSec() const { return mValueKeepDurationSec; }

    void push(float val); // MTsafe
    void push(uint64_t timeStamp, float val); // only for debug : MTsafe

    float getMax() const; // MTsafe
    float getResampleValue(size_t totalResampleCount, std::vector<float>& outValTbl, float* max = nullptr) const; // MTsafe
    void getResampleValueExhaust(size_t totalResampleCount, std::vector<float>& outValTbl) const; // MTsafe

    std::string show() const; // MTsafe

    Parser& getParser() { return mParser; }

private:
    using ValueTimeEventShPtr = std::shared_ptr<ValueTimeEvent>;

    void cleanUpOverflow();
    double getDeltaSecWhole() const;
    static double getDeltaSec(const uint64_t currTime, const uint64_t oldTime); // both microSec

    size_t getTotal() const; // MTsafe
    size_t getEventMemPoolTotal() const; // MTsafe

    ValueTimeEventShPtr getEvent();
    void setEvent(ValueTimeEventShPtr ptr);

    std::string showEventList() const; // MTsafe
    std::string showEventListReverse() const; // MTsafe
    std::string showTotal() const; // MTsafe
    std::string showLastResidualSec() const; // MTsafe

    void parserConfigure();

    //------------------------------

    float mValueKeepDurationSec {10.0f};

    mutable std::mutex mEventListMutex;
    float mEventMaxVal {0.0f};
    std::list<ValueTimeEventShPtr> mEventList;

    unsigned mMaxEventMemPool {0};
    std::deque<ValueTimeEventShPtr> mEventMemPool;

    Parser mParser;
};

} // namespace mcrt_dataio
