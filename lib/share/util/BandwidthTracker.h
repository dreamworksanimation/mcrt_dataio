// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "MiscUtil.h"

#include <list>
#include <string>
#include <memory>               // shared_ptr
#include <cstddef>              // size_t
#include <stdint.h>             // uint64_t

namespace mcrt_dataio {

class BandwidthEvent
//
// Single event of bandwidth tracking logic at particular timing.
//
{
public:
    BandwidthEvent(size_t size) :
        mTimeStamp(MiscUtil::getCurrentMicroSec()),
        mDataSize(size)
    {}

    uint64_t getTimeStamp() const { return mTimeStamp; }
    size_t getDataSize() const { return mDataSize; }

private:
    uint64_t mTimeStamp;
    size_t mDataSize;
};

class BandwidthTracker
//
// This class is designed to track the bandwidth of data size.
// This class keeps all history of bandwidth data size at particular timing as a record
// of the event list. If the event list size is more than 10, we keep event-items which
// are inside some particular user defined interval (=keepIntervalSec) and we remove
// event-items which are older than this length.
// This logic avoids using huge memory of event-item lists.
// We need enough event-item list length to measure accurate bandwidth value. So you
// need to set a proper keepIntervalSec value depending on the how frequently call set()
// API. If set() API is called 1, 2 times during keepIntervalSec, This case bandwidth
// estimation is not so high precision enough. 10~15 set() calls during keepIntervalSec
// might better and return accurate results.
//
{
public:
    BandwidthTracker(float keepIntervalSec)
        : mKeepIntervalSec(keepIntervalSec)
    {}

    void setKeepIntervalSec(float sec) { mKeepIntervalSec = sec; }

    void set(size_t dataSize); // byte
    float getBps() const; // byte/sec

    std::string show() const;

private:
    using BandwidthEventShPtr = std::shared_ptr<BandwidthEvent>;

    float mKeepIntervalSec;

    // This is simple FIFO data however we also need to access all items inside
    // mEventList. This is why this FIFO is implemented by std::list<T> instead
    // std::queue<T>.
    std::list<BandwidthEventShPtr> mEventList;

    size_t getMaxSize() const;
    size_t getDataSizeWhole() const;
    double getDeltaSecWhole() const;
    static double getDeltaSec(const uint64_t currTime, const uint64_t oldTime); // both microSec
};

} // namespace mcrt_dataio
