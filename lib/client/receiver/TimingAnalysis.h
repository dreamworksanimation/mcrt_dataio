// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "TimingRecorderHydra.h"

#include <mcrt_dataio/engine/merger/GlobalNodeInfo.h>
#include <scene_rdl2/common/grid_util/Parser.h>

namespace mcrt_dataio {

class TimingLogEvent
//
// A single event of the analysis result
//
{
public:
    TimingLogEvent(int rankId, float timeStamp, float localTimeStamp, const std::string& description)
        : mRankId(rankId)
        , mTimeStamp(timeStamp)
        , mLocalTimeStamp(localTimeStamp)
        , mDescription(description)
    {}

    int rankIdLen() const;
    int secStrLenTimeStamp() const { return secStrLen(mTimeStamp); }
    int secStrLenLocalTimeStamp() const { return secStrLen(mLocalTimeStamp); }
    int secStrLenDeltaTimeStamp(const TimingLogEvent* prev) const;

    std::string show(int rankIdLen = 0,
                     int maxTimeLen = 0,
                     int maxLocalTimeLen = 0,
                     int maxDeltaTimeLen = 0,
                     const TimingLogEvent* prev = nullptr) const;

private:

    std::string showClient() const;
    std::string showRankId() const;

    static int secStrLen(float sec);

    //------------------------------

    int mRankId; // -1:client 0orPositive:mcrt
    float mTimeStamp; // sec from baseTime
    float mLocalTimeStamp; // sec from local block
    std::string mDescription;
};

class TimingLog
//
// Timing analysis result from the beginning of render starts at the client to the end of displaying
// 1st received an image from backend via backend computation.
//
{
public:
    TimingLog()
        : mBaseTime(0)
    {}

    void setBaseTime(uint64_t time) { mBaseTime = time; }
    void setRecvImgSenderMachineId(const std::string& name) { mRecvImgSenderMachineId = name; }

    // rankId = -1 : client
    // rankId >= 0 : mcrt computation
    void enqEvent(int rankId, float timeStamp, float localTimeStamp, const std::string& description) {
        mEventTable.emplace_back(rankId, timeStamp, localTimeStamp, description);
    }

    std::string show() const;

private:
    uint64_t mBaseTime;
    std::string mRecvImgSenderMachineId;

    std::vector<TimingLogEvent> mEventTable;
};

class TimingAnalysis
//
// Timing analysis of TimingRecorderHydra.
//
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;

    TimingAnalysis(GlobalNodeInfo& globalNodeInfo);

    void setTimingRecorderHydra(std::shared_ptr<TimingRecorderHydra> timingRecorderHydra);

    Parser& getParser() { return mParser; }

private:
    using TimingLogShPtr = std::shared_ptr<TimingLog>;

    void parserConfigure();

    TimingLogShPtr make1stRecvImgTimingLogHydra() const;
    void makeTimingLogMcrt(std::shared_ptr<McrtNodeInfo> nodeInfo, TimingLogShPtr log) const;

    float deltaSecMcrtToClient(float mcrtDeltaSec, std::shared_ptr<McrtNodeInfo> nodeInfo) const;

    std::string show1stRecvImgLogHydra() const;

    //------------------------------

    GlobalNodeInfo& mGlobalNodeInfo;
    std::shared_ptr<TimingRecorderHydra> mTimingRecorderHydra;

    Parser mParser;
};

} // namespace mcrt_dataio
