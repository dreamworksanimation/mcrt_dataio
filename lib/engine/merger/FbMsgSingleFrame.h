// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

//
// -- Message data for single frame image --
//
// Single image might be rendered by multiple machines and each machine send
// multiple ProgressiveFrame messages for one rendering by some interval.
// This FbMsgSingleFrame keeps multiple ProgressiveFrame messages which received
// from all machines for perticular frame of image in perticular interval.
// (Not keep all of the ProgressiveFrame message from beginning of rendering
// to finish. Internal data is partially clean up by mcrt_merger by some 
// interval).
//

#include "FbMsgMultiChans.h"
#include "MergeActionTracker.h"

#include <mcrt_messages/ProgressiveFrame.h>
#include <scene_rdl2/common/grid_util/Arg.h>
#include <scene_rdl2/common/fb_util/FbTypes.h>
#include <scene_rdl2/common/grid_util/Fb.h>
#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/platform/Platform.h> // finline
#include <scene_rdl2/common/rec_time/RecTime.h>  // for timing test
#include <scene_rdl2/render/cache/CacheDequeue.h>
#include <scene_rdl2/render/cache/CacheEnqueue.h>

#include <vector>

namespace scene_rdl2 {
    namespace grid_util { class LatencyLog; }
} // namespace scene_rdl2

namespace mcrt_dataio {

class GlobalNodeInfo;

class FbMsgSingleFrame
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;

    enum class DecodeMode : unsigned {
        ONTHEFLY = 0, // on the fly decoding without data copy (for realtime context).
        DELAY         // delay decode (for interactive lighting session).
    };

    enum class TaskType : unsigned {
        NON_OVERLAPPED_TILE, // original tile based task dispatch logic
        MULTIPLEX_PIX        // multiplex pixel distribution mode
    };

    FbMsgSingleFrame()
    {
        parserConfigure();
    }

    void setGlobalNodeInfo(GlobalNodeInfo *globalNodeInfo) { mGlobalNodeInfo = globalNodeInfo; }
    void setTunnelMachineIdStaged(int* tunnelMachineId) { mTunnelMachineIdStaged = tunnelMachineId; }

    finline bool init(const int numMachines);
    finline bool initFb(const scene_rdl2::math::Viewport &rezedViewport); // original w, h. not needed tile aligned
    void changeTaskType(const TaskType &type);

    finline void resetWholeHistory(const uint32_t syncId);
    finline void resetLastHistory();
    finline void resetLastInfoOnlyHistory() { mReceivedInfoOnlyMessagesTotal = 0; }
    finline void resetAllReceivedMessagesCount()
    {
        mReceivedInfoOnlyMessagesAll = 0;
        mReceivedMessagesAll = 0;
    }
    void resetFeedback(bool feedbackActive);

    finline bool isInitialFrameMessage(const mcrt::ProgressiveFrame& progressive,
                                       bool& forceSend) const;

    bool push(const mcrt::ProgressiveFrame &progressive);
    void decodeAll();
    void merge(const unsigned partialMergeTilesTotal, // 0:non-partial-merge-mode
               scene_rdl2::grid_util::Fb &fb,
               scene_rdl2::grid_util::LatencyLog &latencyLog); // fb is always clear internally and
                                                               // return fresh combined result

    bool getFeedbackActive() const { return mFeedbackActive; }
    MergeActionTracker& getMergeActionTracker(unsigned machineId) { return mMergeActionTracker[machineId]; }
    void encodeMergeActionTracker(scene_rdl2::cache::CacheEnqueue& enqueue); // for feedback
    static std::string decodeMergeActionTrackerAndDump(scene_rdl2::cache::CacheDequeue& dequeue,
                                                       unsigned targetMachineId); // for test

    uint32_t getSyncId() const { return mMySyncId; }
    TaskType getTaskType() const { return mTaskType; }

    size_t getReceivedMessagesTotal() const { return mReceivedMessagesTotal; }
    float getProgressTotal() const { return mProgressTotal; }
    finline float getProgressFraction() const;
    mcrt::BaseFrame::Status getStatus() const { return mStatus; }

    int getActiveMachines() const { return mActiveMachines; }
    int getFirstMachineId() const { return mFirstMachineId; }

    // has data from all MCRT computation
    bool isReadyAll() { return (mActiveMachines == static_cast<int>(mMessage.size()))? true: false; }

    finline bool isCoarsePassDone() const;
    finline const std::string& getDenoiserAlbedoInputName() const { return mDenoiserAlbedoInputName; }
    finline const std::string& getDenoiserNormalInputName() const { return mDenoiserNormalInputName; }

    finline uint64_t getSnapshotStartTime();

    void encodeLatencyLog(scene_rdl2::rdl2::ValueContainerEnq &vContainerEnq); // only encode latencyLog info

    std::string show(const std::string &hd) const;

    Parser& getParser() { return mParser; }

    const scene_rdl2::grid_util::Fb& getFb(const unsigned machineId) const { return mFb[machineId]; } // for debug
    const FbMsgMultiChans& getMultiChans(const unsigned machineId) const { return mMessage[machineId]; } // for debug

private:
    GlobalNodeInfo *mGlobalNodeInfo {nullptr};

    uint32_t mMySyncId {0};

    DecodeMode mDecodeMode {DecodeMode::DELAY};
    TaskType mTaskType {TaskType::MULTIPLEX_PIX};

    int mNumMachines {0};
    scene_rdl2::math::Viewport mRezedViewport;

    // for current (=last) iteration data
    std::vector<FbMsgMultiChans> mMessage; // mMessage[machineId]
    std::vector<char> mReceived;           // mReceived[machineId]
    //    bool mFeedback {false}; // runtime feedback control condition
    bool mFeedbackActive {false}; // runtime feedback control condition
    std::vector<MergeActionTracker> mMergeActionTracker; // mMergeActionTracker[machineId]
    size_t mReceivedInfoOnlyMessagesTotal {0}; // total recv info messages from last message sent
    size_t mReceivedInfoOnlyMessagesAll {0};   // all info messages total on this mySyncId 
    size_t mReceivedMessagesTotal {0};         // total recv msgs from last image sent on this mySyncId
    size_t mReceivedMessagesAll {0};           // all received messages count on this mySyncId
    
    // The tunnel operation is designed for debugging purposes and only specified single machine data
    // is directly sent to the client without any merge operations. This means all merge operation is
    // bypassed and the merge node simply sends incoming particular MCRT data to the client as is.
    // This operation is useful for debugging to narrow down the reason the bug is inside the merge
    // operation or not.
    int* mTunnelMachineIdStaged {nullptr};
    int mTunnelMachineIdRuntime {-1}; // negativeId : disable tunnel operation.
                                      // 0 or positiveId : this machineId data is only send to client without merge

    // regarding to entire (= from start of current frame rendering) iteration
    std::vector<char> mReceivedAll;                     // [machineId]
    std::vector<unsigned> mReceivedMessagesTotalAll;    // [machineId] : message total count
    std::vector<uint64_t> mRenderStartTime;             // [machineId] : render start time
    std::vector<char> mGarbageCollectReady;             // [machineId]
    std::vector<char> mGarbageCollectCompleted;         // [machineId]
    std::vector<char> mCoarsePassAll;                   // [machineId]
    std::vector<float> mProgressAll;                    // [machineId]
    std::vector<mcrt::BaseFrame::Status> mStatusAll;    // [machineId]
    int mActiveMachines {0};                            // active machine total
    int mFirstMachineId {-1};                           // first data received machine id
    std::string mDenoiserAlbedoInputName;
    std::string mDenoiserNormalInputName;
    float mProgressTotal {0.0f};                                  // current progress sum
    mcrt::BaseFrame::Status mStatus {mcrt::BaseFrame::CANCELLED}; // current frame's status

    // combined result for each machine from start of rendering
    std::vector<scene_rdl2::grid_util::Fb> mFb; // mFb[machineId] : auto resize by received ProgressiveFrame

    uint32_t mDecodeCountTotal {0};
    uint32_t mMergeCountTotal {0};
    uint32_t mEncodeLatencyLogCountTotal {0};
    uint32_t mSnapshotStartTimeTotal {0};
    uint32_t mPartialMergeStartTileId {0}; // Start tileId for next asynchronous partial merge operation

    scene_rdl2::rec_time::RecTimeLog mDebugTimeLogPush;   // for timing test
    scene_rdl2::rec_time::RecTimeLog mDebugTimeLogDecode; // for timing test
    scene_rdl2::rec_time::RecTimeLog mDebugTimeLogMerge;  // for timing test

    Parser mParser;

    //------------------------------

    float calcProgressiveTotal() const;
    mcrt::BaseFrame::Status calcCurrentFrameStatus() const;

    uint64_t getCurrentMicroSec() const;

    void decodeFirstPushedData();
    void decodeAllPushedData();
    void mergeFirstFb(scene_rdl2::grid_util::Fb &fb, scene_rdl2::grid_util::LatencyLog &latencyLog);
    void mergeAllFb(scene_rdl2::grid_util::Fb &fb, scene_rdl2::grid_util::LatencyLog &latencyLog);
    void mergeAllFb(const unsigned partialMergeTilesTotal,
                    scene_rdl2::grid_util::Fb &fb, scene_rdl2::grid_util::LatencyLog &latencyLog);
    void mergeSingleFb(const std::vector<char> *partialMergeTilesTbl, const int machineId,
                       scene_rdl2::grid_util::Fb &fb);
    bool verifyMergedResultNumSample(const scene_rdl2::grid_util::Fb& mergedFb) const;
    bool verifyMergedResultNumSampleSingleHost(int machineId,
                                               const scene_rdl2::grid_util::Fb& mergedFb) const;

    void partialMergeTilesTblGen(const unsigned partialMergeTilesTotal,
                                 std::vector<char> &partialMergeTilesTbl);

    void timeLogUpdate(const std::string &msg,
                       scene_rdl2::rec_time::RecTimeLog &timeLog, const uint64_t startMicroSec) const;

    std::string showMessageAndReceived(const std::string &hd) const;
    std::string showAllReceivedAndProgress(const std::string &hd) const;

    void parserConfigure();
    bool parserCommandMultiChan(Arg& arg);
    bool parserCommandFb(Arg& arg);
}; // FbMsgSingleFrame

finline bool
FbMsgSingleFrame::init(const int numMachines)
{
    if (mNumMachines == numMachines) {
        return true;            // no need to update
    }
    mNumMachines = numMachines;

    try {
        mMessage.resize(numMachines);
        mReceived.resize(numMachines);
        mMergeActionTracker.resize(numMachines);

        mReceivedAll.resize(numMachines);
        mReceivedMessagesTotalAll.resize(numMachines);
        mRenderStartTime.resize(numMachines);
        mGarbageCollectReady.resize(numMachines);
        mGarbageCollectCompleted.resize(numMachines);
        mCoarsePassAll.resize(numMachines);
        mProgressAll.resize(numMachines);
        mStatusAll.resize(numMachines);

        mFb.resize(numMachines);
        // We need to update fb size here
        for (size_t machineId = 0; machineId < (size_t)numMachines; ++machineId) {
            mMessage[machineId].setGlobalNodeInfo(mGlobalNodeInfo);
            mMergeActionTracker[machineId].setMachineId(static_cast<unsigned>(machineId));
            mFb[machineId].init(mRezedViewport);
        }
    }
    catch (...) {
        return false;
    }

    resetWholeHistory(0);

    return true;
}

finline bool
FbMsgSingleFrame::initFb(const scene_rdl2::math::Viewport &rezedViewport)
{
    if (mRezedViewport == rezedViewport) {
        return true;            // no need to update
    }
    mRezedViewport = rezedViewport;

    try {
        for (size_t machineId = 0; machineId < mFb.size(); ++machineId) {
            mFb[machineId].init(mRezedViewport);

            std::ostringstream ostr;
            ostr << "FbMsgSingleFrame-mId:" << machineId;
            mFb[machineId].setDebugTag(ostr.str());
        }
    }
    catch (...) {
        return false;
    }
    return true;
}

finline void
FbMsgSingleFrame::resetWholeHistory(const uint32_t syncId)
{
    mMySyncId = syncId;

    resetLastHistory();

    for (size_t machineId = 0; machineId < mMessage.size(); ++machineId) {
        mReceivedAll[machineId] = static_cast<char>(false);
        mReceivedMessagesTotalAll[machineId] = 0;
        mRenderStartTime[machineId] = 0;
        mGarbageCollectReady[machineId] = static_cast<char>(false);
        mGarbageCollectCompleted[machineId] = static_cast<char>(false);
        mCoarsePassAll[machineId] = static_cast<char>(true);
        mProgressAll[machineId] = 0.0f;
        mStatusAll[machineId] = mcrt::BaseFrame::FINISHED;
    }
    mActiveMachines = 0;
    mFirstMachineId = -1;
    mDenoiserAlbedoInputName.clear();
    mDenoiserNormalInputName.clear();
    mProgressTotal = 0.0f;
    mDecodeCountTotal = 0;
    mMergeCountTotal = 0;
    mEncodeLatencyLogCountTotal = 0;
    mSnapshotStartTimeTotal = 0;
}

finline void    
FbMsgSingleFrame::resetLastHistory()
{
    resetLastInfoOnlyHistory();

    for (size_t machineId = 0; machineId < mMessage.size(); ++machineId) {
        mMessage[machineId].reset();
        mReceived[machineId] = static_cast<char>(false);
    }
    mReceivedMessagesTotal = 0;
}

finline bool
FbMsgSingleFrame::isInitialFrameMessage(const mcrt::ProgressiveFrame& progressive,
                                        bool& forceSend) const
{
    uint32_t syncFrameId = progressive.mHeader.mFrameId;
    if (mMySyncId < syncFrameId) {
        // This progressive message is newer than this FbMsgSingleFrame data
        forceSend = false;
        return true;
    }
    if (syncFrameId < mMySyncId) {
        // This progressive message is older than this FbMsgSingleFrame data
        forceSend = false;
        return false;
    }

    int currMachineId = progressive.mMachineId;
    if (mReceivedMessagesTotalAll[currMachineId] == 0) {
        // We already received the same syncId progressiveFrame message for other hosts
        // but this is a 1st one regarding this machineId.
        // In this case, we want to send this data regardless of the interval from the previous send.
        forceSend = true;
        return true;
    }

    // we already received data from currMacineId hosts
    forceSend = false;
    return false;
}

finline float
FbMsgSingleFrame::getProgressFraction() const
{
    if (mTaskType == TaskType::MULTIPLEX_PIX) {
        return mProgressTotal;
    }
    return (!mNumMachines)? 0.0f: std::min(mProgressTotal / (float)mNumMachines, 1.0f);
}

finline bool
FbMsgSingleFrame::isCoarsePassDone() const
{
    for (size_t machineId = 0; machineId < static_cast<size_t>(mNumMachines); ++machineId) {
        if (mCoarsePassAll[machineId]) return false;
    }
    return true;
}

finline uint64_t
FbMsgSingleFrame::getSnapshotStartTime()
{
    uint64_t startTime = 0;

    if (mSnapshotStartTimeTotal == 0) {
        // for 1st packet execution for this frame.
        if (mActiveMachines > 0) {
            int machineId = mFirstMachineId;
            startTime = mMessage[machineId].getSnapshotStartTime();
        }

    } else {
        // for 2nd or later packet execution for this frame
        for (int machineId = 0; machineId < mNumMachines; ++machineId) {
            if (!mReceived[machineId]) continue;

            if (startTime == 0) {
                startTime = mMessage[machineId].getSnapshotStartTime();
            } else {
                if (mMessage[machineId].getSnapshotStartTime() < startTime) {
                    startTime = mMessage[machineId].getSnapshotStartTime();
                }
            }
        }
    }
    mSnapshotStartTimeTotal++;
    
    return startTime;
}

} // namespace mcrt_dataio
