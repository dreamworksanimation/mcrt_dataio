// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <queue>
#include <string>

namespace mcrt_dataio {

class FpsTracker
//
// This class is used for tracking down the frequency of some event and return result as fps value.
// This class keeps all the event timing information internally based on the user defined
//  keepIntervalSec duration or at least 10 events then calculate average fps.
//
// This pseudo code explains how to use this class.
//
//  FpsTracker fpsTracker;
//  for (i = 0; ; ++i) {
//    ... do something ...
//    fpsTracker.set(); // mark completion of event ... (A)
//    if (i % 100 == 99) {
//      fps = fps.Tracker.getFps() ... (B)
//    }
//  }
//
//  A saves timing information into fpsTracker.
//  B shows average fps of event A
//
{
public:
    FpsTracker(float keepIntervalSec) :
        mKeepIntervalSec(keepIntervalSec)
    {}

    void setKeepIntervalSec(float sec) { mKeepIntervalSec = sec; }

    void set();
    float getFps() const; // frame/sec

    std::string show() const;

private:
    double getDeltaSecWhole() const { return getDeltaSec(mEventQueue.back(), mEventQueue.front()); }
    static double getDeltaSec(const uint64_t currTime, const uint64_t oldTime) // both microSec
    {
        return static_cast<double>(currTime - oldTime) * 0.000001; // return sec
    }

    //------------------------------

    float mKeepIntervalSec;
    std::queue<uint64_t> mEventQueue;
};

} // namespace mcrt_dataio
