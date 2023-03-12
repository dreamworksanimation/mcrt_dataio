// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
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

    void set();
    float getFps(); // frame/sec

    std::string show() const;

private:
    float deltaSecWhole() { return deltaSec(mEventQueue.back(), mEventQueue.front()); }
    static float deltaSec(const uint64_t currTime, const uint64_t oldTime) // both microSec
    {
        return (float)(currTime - oldTime) * 1.0e-6f; // return sec
    }

    //------------------------------

    float mKeepIntervalSec;
    std::queue<uint64_t> mEventQueue;
};

} // namespace mcrt_dataio

