// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "FbMsgSingleFrame.h"

#include <scene_rdl2/common/grid_util/LatencyLog.h>
#include <scene_rdl2/scene/rdl2/ValueContainerEnq.h>

#include <iomanip>
#include <sstream>

#include <stdint.h>
#include <sys/time.h>

// Basically we should use multi-thread version.
// This single thread mode is used debugging and performance comparison reason mainly.
//#define SINGLE_THREAD

// This is for timing test 
//#define DEBUG_TIMING_LOG

//#define DEBUG_MSG

namespace mcrt_dataio {

void
FbMsgSingleFrame::changeTaskType(const TaskType &type)
{
    if (mTaskType == type) return;

    mTaskType = type;
}

void
FbMsgSingleFrame::resetFeedback(bool feedbackActive)
{
    mFeedbackActive = feedbackActive;

    if (!mFeedbackActive) return;

    //
    // initialize MergeActionTrackers
    //
    for (auto& currMergeActionTracker: mMergeActionTracker) {
        currMergeActionTracker.resetEncode(); // free previous memory and reset all
    }
}

bool
FbMsgSingleFrame::push(const mcrt::ProgressiveFrame &progressive)
{
    int currMachineId = progressive.mMachineId;
    if (currMachineId < 0 || static_cast<int>(mMessage.size()) <= currMachineId) {
        return false; // out of machineId range
    }

    bool delayDecode = (mDecodeMode == DecodeMode::DELAY)? true: false;

#   ifdef DEBUG_TIMING_LOG
    uint64_t cMicroSec = 0;
    if (currMachineId == 0) cMicroSec = getCurrentMicroSec();        
#   endif // end DEBUG_TIMING_LOG

    if (!mMessage[currMachineId].push(delayDecode, progressive, mFb[currMachineId])) {
        return false; // error
    }

#   ifdef DEBUG_TIMING_LOG
    if (currMachineId == 0) {
        timeLogUpdate("--- push --", mDebugTimeLogPush, cMicroSec);
    }
#   endif // end DEBUG_TIMING_LOG

    if (progressive.getProgress() < 0.0f) {
        //
        // Special progressiveFrame data which does not include image information
        //
        mReceivedInfoOnlyMessagesTotal++;
        mReceivedInfoOnlyMessagesAll++;
        return true;
    }

    if (progressive.getStatus() == mcrt::BaseFrame::STARTED) {
        //
        // This is very first snapshot of current rendering frame on single frame mode.
        // Also we need to reset about whole iteration status.
        // This started condition is only happen when using single frame mode. If using multi-frame mode (i.e.
        // activated sync mode using synId case), this condition never happend because,
        // new STARTED condition should have new syncId and new syncId accesses new FbMsgSingleFrame data.
        //

        // reset whole iteration related information
        if (mReceivedAll[currMachineId]) {
            mActiveMachines--;  // We need to reset condition (subtract 1 from total) regarding this machineId
        }
        mReceivedAll[currMachineId] = static_cast<char>(false); // reset condition to false

        mReceivedMessagesTotalAll[currMachineId] = 0; // reset
        mRenderStartTime[currMachineId] = 0;
        mGarbageCollectReady[currMachineId] = static_cast<char>(false);
        mGarbageCollectCompleted[currMachineId] = static_cast<char>(false);

        mCoarsePassAll[currMachineId] = static_cast<char>(true); // reset condition to coarse pass

        // update denoiser albedo/normal input name
        if (mDenoiserAlbedoInputName.empty() && progressive.mDenoiserAlbedoInputName.size()) {
            mDenoiserAlbedoInputName = progressive.mDenoiserAlbedoInputName;
        }
        if (mDenoiserNormalInputName.empty() && progressive.mDenoiserNormalInputName.size()) {
            mDenoiserNormalInputName = progressive.mDenoiserNormalInputName;
        }

        // update tunnelMachineIdRuntime when the new render started
        mTunnelMachineIdRuntime = (mTunnelMachineIdStaged) ? (*mTunnelMachineIdStaged) : -1;
        if (mTunnelMachineIdRuntime >= 0) {
            std::cerr << "TunnelMahcineIdRuntime:" << mTunnelMachineIdRuntime << '\n';
        }

    } else if (progressive.getStatus() == mcrt::BaseFrame::FINISHED) {
#       ifdef DEBUG_MSG
        std::cerr << ">> FbMsgSingleFrame.cc push mId:" << currMachineId << " FINISHED" << std::endl;
#       endif // end DEBUG_MSG
    }
    mReceivedMessagesTotal++;   // increment total received message count
    mReceivedMessagesAll++;

    /* useful debug message
    std::cerr << ">> FbMsgSingleFrame.cc"
              << " mySyncId:" << mMySyncId
              << " currMachineId:" << currMachineId
              << " receivedMessateTotal:" << mReceivedMessagesTotal << std::endl;
    */

    // update received condition flag and total active machine info
    mReceived[currMachineId] = static_cast<char>(true);
    if (!mReceivedAll[currMachineId]) {
        mReceivedAll[currMachineId] = static_cast<char>(true);
        mActiveMachines++;
        if (mActiveMachines == 1) {
            mFirstMachineId = currMachineId; // This is a very first data to receive
        }
    }

    // GarbageCollect status tracking
    mReceivedMessagesTotalAll[currMachineId]++;
    if (mReceivedMessagesTotalAll[currMachineId] == 1) {
        // This is a very first message for this rendering frame.
        mRenderStartTime[currMachineId] = getCurrentMicroSec();
    } else {
        if (!mGarbageCollectReady[currMachineId]) {
            if (mReceivedMessagesTotalAll[currMachineId] > 5) {
                uint64_t currTime = getCurrentMicroSec();
                uint64_t deltaTime = currTime - mRenderStartTime[currMachineId];
                float deltaMilliSec = static_cast<float>(deltaTime) / 1000.0f;
                if (deltaMilliSec > 500) {
                    // After 500milSec with more than 5 messages received,
                    // we consider fb is ready to do garbage collection
                    mGarbageCollectReady[currMachineId] = static_cast<char>(true);
                    mGarbageCollectCompleted[currMachineId] = static_cast<char>(false);
                }
            }
        }
    }

    // coarse pass tracking
    if (mCoarsePassAll[currMachineId]) {
        if (!mMessage[currMachineId].isCoarsePass()) {
            mCoarsePassAll[currMachineId] = static_cast<char>(false);
        }
    }

    // update progress value table
    if (mTunnelMachineIdRuntime < 0 ||
        (mTunnelMachineIdRuntime >= 0 && mTunnelMachineIdRuntime == currMachineId)) {
        mProgressAll[currMachineId] = mMessage[currMachineId].getProgress();
    }
    mProgressTotal = calcProgressiveTotal();

    // update mStatusAll for this frame
    if (mMessage[currMachineId].hasStartedStatus()) {
        mStatusAll[currMachineId] = mcrt::BaseFrame::STARTED;
    } else {
        mStatusAll[currMachineId] = mMessage[currMachineId].getStatus();
    }

    // update current frame's status
    if ((mStatus = calcCurrentFrameStatus()) == mcrt::BaseFrame::STARTED) {
        mProgressTotal = 0.0f;  // just in case
    }

    return true;
}

void
FbMsgSingleFrame::decodeAll()
//
// Decode progressiveFrame message for delay decode mode
//
{
    if (!mReceivedMessagesTotal) return; // empty messages

    if (mDecodeMode == DecodeMode::DELAY) {
        // decode all
        decodeAllPushedData();
        mDecodeCountTotal++;
    }
}

void
FbMsgSingleFrame::merge(const unsigned partialMergeTilesTotal,
                        scene_rdl2::grid_util::Fb &fb,
                        scene_rdl2::grid_util::LatencyLog &latencyLog)
{
    if (!mReceivedMessagesTotal) return; // empty messages

    //------------------------------
    //
    // garbage collection 
    //
    for (size_t machineId = 0; machineId < static_cast<size_t>(mNumMachines); ++machineId) {
        if (!mReceived[machineId]) continue;

        // Garbage Collection for fb data
        if (mGarbageCollectReady[machineId]) {
            // We are ready to do garbage collection for fb.
            if (mGarbageCollectCompleted[machineId] == static_cast<char>(false)) {
#               ifdef DEBUG_MSG
                std::cerr << "FbMsgSingleFrame.cc garbageCollectUnusedBuffers() " << machineId << std::endl;
#               endif // end DEBUG_MSG
                mFb[machineId].garbageCollectUnusedBuffers();
                mGarbageCollectCompleted[machineId] = static_cast<char>(true);
            }
        }
    }
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_GC);

    //------------------------------
    //
    // all received fb resolution check
    //
    for (size_t machineId = 0; machineId < static_cast<size_t>(mNumMachines); ++machineId) {
        if (!mReceivedAll[machineId]) continue;

        if (fb.getRezedViewport() != mFb[machineId].getRezedViewport()) {
            // resolution mismatch between received fb reso and output fb reso
            // viewport message is not received yet ? We can not process this data anyway.
            return;             // skip combine
        }
    }
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_RESOCHECK);

    //
    // We don't check about ROI because currently all progmcrt computation send entire frame
    // even setup ROI viewport. (Data size wise, there is no advantage for ROI viewport because
    // outside ROI viewport is always the same between prev and the current frame.)
    // In other words, we merge all fb regardless of ROI condition when all fb resolution is same.
    //

    //------------------------------
    //
    // Merge all current MCRT's receive fb into one image
    //
    if (mMergeCountTotal == 0) {
        // Very first received data need to be processed without partialMerge mode.
        mergeFirstFb(fb, latencyLog);
    }

    // Will merge all of the packet which received.
    if (partialMergeTilesTotal == 0) {
        mergeAllFb(fb, latencyLog);
    } else {
        mergeAllFb(partialMergeTilesTotal, fb, latencyLog);
    }
    mMergeCountTotal++;
}

void
FbMsgSingleFrame::encodeMergeActionTracker(scene_rdl2::cache::CacheEnqueue& enqueue)
{
    for (size_t machineId = 0; machineId < static_cast<size_t>(mNumMachines); ++machineId) {
        if (!mReceivedAll[machineId]) continue;

        enqueue.enqVLInt(static_cast<int>(machineId));
        mMergeActionTracker[machineId].encodeData(enqueue);
    }
    enqueue.enqVLInt(-1); // terminator
    enqueue.finalize();
}

// static function
std::string
FbMsgSingleFrame::decodeMergeActionTrackerAndDump(scene_rdl2::cache::CacheDequeue& dequeue,
                                                  unsigned targetMachineId)
{
    while (true) {
        int machineId = dequeue.deqVLInt();
        if (machineId < 0) break;

        if (machineId != static_cast<int>(targetMachineId)) {
            MergeActionTracker::decodeDataSkipOnMCRTComputation(dequeue);
        } else {
            MergeActionTracker currMergeActionTracker;
            currMergeActionTracker.decodeDataOnMCRTComputation(dequeue);
            return currMergeActionTracker.dumpData();
        }
    }

    std::ostringstream ostr;
    ostr << "Can not decode MergeActionTracker data (no data for targetMachineId:" << targetMachineId << ")";
    return ostr.str();
}

void
FbMsgSingleFrame::encodeLatencyLog(scene_rdl2::rdl2::ValueContainerEnq &vContainerEnq)
{
    if (mEncodeLatencyLogCountTotal == 0) {
        // Only encode 1st received data info for 1st try.
        if (mActiveMachines > 0) {
            int machineId = mFirstMachineId;
            vContainerEnq.enqVLInt(machineId); // same machineId as int (0 <= machineId)
            mMessage[machineId].encodeLatencyLog(vContainerEnq);
        }
    } else {
        // encode all info for 2nd or later execution of this function.
        for (int machineId = 0; machineId < mNumMachines; ++machineId) {
            if (!mReceived[machineId]) continue;
            vContainerEnq.enqVLInt(machineId); // same machineId as int (0 <= machineId)
            mMessage[machineId].encodeLatencyLog(vContainerEnq);
        }
    }

    vContainerEnq.enqVLInt(-1); // end marker

    mEncodeLatencyLogCountTotal++;
}

std::string
FbMsgSingleFrame::show(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd << "FbMsgSingleFrame {\n";
    ostr << showMessageAndReceived(hd + "  ") << '\n';
    ostr << showAllReceivedAndProgress(hd + "  ") << '\n';
    ostr << hd << "}";
    return ostr.str();
}

//-------------------------------------------------------------------------------------------------------------

float
FbMsgSingleFrame::calcProgressiveTotal() const
{
    float total = 0.0f;
    for (size_t machineId = 0; machineId < mMessage.size(); ++machineId) {
        total += mProgressAll[machineId];
    }
    return total;
}

mcrt::BaseFrame::Status    
FbMsgSingleFrame::calcCurrentFrameStatus() const
//
// Compute frame status condition based on received all MCRT computation status condition.
//
{
    int numStart = 0;
    int numRendering = 0;
    int numFinished = 0;
    int numCanceled = 0;
    int numError = 0;

    // Count each status enum condition hosts count
    for (size_t machineId = 0; machineId < mMessage.size(); ++machineId) { // loop over all machines
        switch (mStatusAll[machineId]) {
        case mcrt::BaseFrame::STARTED   : numStart++;     break;
        case mcrt::BaseFrame::RENDERING : numRendering++; break;
        case mcrt::BaseFrame::FINISHED  : numFinished++;  break;
        case mcrt::BaseFrame::CANCELLED : numCanceled++;  break;
        case mcrt::BaseFrame::ERROR     : numError++;     break;
        default : break;
        }
    }

    if (numError) {
        return mcrt::BaseFrame::ERROR;     // if some hosts has ERROR -> return ERROR status
    }
    if (numCanceled) {
        return mcrt::BaseFrame::CANCELLED; // if some hosts has CANCELED -> return CANCELED status
    }

    if (numStart && mReceivedMessagesAll == 1) {
        return mcrt::BaseFrame::STARTED; // very fist data -> return STARTED
    }

    if (numFinished == mNumMachines) {
        return mcrt::BaseFrame::FINISHED; // if all hosts have FINISHED -> return FINISHED
    }

    if (numRendering) {
        return mcrt::BaseFrame::RENDERING; // if some host has RENDERING condition -> return RENDERING
    }

    // Otherwise ... This is a situation of
    // 1) This is not a very fist received message.
    // 2) All hosts do not have FINISHED condition. Some hosts might have FINISHED condition.
    // 3) Some hosts might have STARTED condition but it not recognise as STARTED because it's not
    //    a 1st received message.
    // 4) Non FINISHED condition host does not have any of RENDERING, CANCELLED and ERROR.
    //    This means non FINISHED condition host does not return any messages yet (or dead).
    // In this case, we assume that this frame is start rendering and some of the hosts are STARTED
    // then FINISHED quickly.
    // And some of other hosts are also started but not send STARTED message yet somehow (heavy
    // computation or connection dead).
    // Probably most reliable return status at this case might be RENDERING.

    return mcrt::BaseFrame::RENDERING;
}

uint64_t
FbMsgSingleFrame::getCurrentMicroSec() const
{
    struct timeval tv;
    gettimeofday(&tv, 0x0);
    uint64_t cTime = static_cast<uint64_t>(tv.tv_sec) * 1000 * 1000 + static_cast<uint64_t>(tv.tv_usec);
    return cTime;
}

void
FbMsgSingleFrame::decodeFirstPushedData()
//
// Only decode 1st received data for this frame
//
{
    if (mActiveMachines == 0) return;

    //
    // only decode first received data
    //
    int machineId = mFirstMachineId;
    if (!mReceived[machineId]) return;

#   ifdef DEBUG_TIMING_LOG
    uint64_t startMicroSec = getCurrentMicroSec();
#   endif // end DEBUG_TIMING_LOG

    MergeActionTracker* mergeActionTrackerPtr = (mFeedbackActive) ? &mMergeActionTracker[machineId] : nullptr;
    mMessage[machineId].decodeAll(mFb[machineId], mergeActionTrackerPtr);

#   ifdef DEBUG_TIMING_LOG
    uint64_t deltaMicroSec = getCurrentMicroSec() - startMicroSec;
    float deltaMillSec = static_cast<float>(deltaMicroSec) / 1000.0f;
    std::cerr << ">> FbMsgSingleFrame.cc decodeFirst " << deltaMillSec << " ms" << std::endl;
#   endif // end DEBUG_TIMING_LOG
}

void
FbMsgSingleFrame::decodeAllPushedData()
//
// Decode all received data which is not decoded for this frame.
//
{
#   ifdef DEBUG_TIMING_LOG
    uint64_t cMicroSec = getCurrentMicroSec();
#   endif // end DEBUG_TIMING_LOG

#   ifdef SINGLE_THREAD
    for (int machineId = 0; machineId < mNumMachines; ++machineId) {
        if (!mReceived[machineId]) continue;
        MergeActionTracker* mergeActionTrackerPtr =
            (mFeedbackActive) ? &mMergeActionTracker[machineId] : nullptr;
        mMessage[machineId].decodeAll(mFb[machineId], mergeActionTrackerPtr);
    }
#   else // else SINGLE_THREAD
    tbb::blocked_range<size_t> range(0, mNumMachines);
    tbb::parallel_for(range, [&](const tbb::blocked_range<size_t> &r) {
            for (size_t machineId = r.begin(); machineId < r.end(); ++machineId) {
                if (!mReceived[machineId]) continue;
                MergeActionTracker* mergeActionTrackerPtr =
                    (mFeedbackActive) ? &mMergeActionTracker[machineId] : nullptr;
                mMessage[machineId].decodeAll(mFb[machineId], mergeActionTrackerPtr);
            }
        });
#   endif // end !SINGLE_THREAD

#   ifdef DEBUG_TIMING_LOG
    timeLogUpdate("-- decode -", mDebugTimeLogDecode, cMicroSec);
#   endif // end DEBUG_TIMING_LOG
}

void
FbMsgSingleFrame::mergeFirstFb(scene_rdl2::grid_util::Fb &fb,
                               scene_rdl2::grid_util::LatencyLog &latencyLog)
//
// Only merge first received data
//
{
#   ifdef DEBUG_TIMING_LOG
    uint64_t startMicroSec = getCurrentMicroSec();
#   endif // end DEBUG_TIMING_LOG

    fb.reset();
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_FBRESET);
    int machineId = mFirstMachineId;
    mergeSingleFb(nullptr, machineId, fb);
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_ACCUMULATE);

#   ifdef DEBUG_TIMING_LOG
    uint64_t deltaMicroSec = getCurrentMicroSec() - startMicroSec;
    float deltaMillSec = static_cast<float>(deltaMicroSec) / 1000.0f;
    std::cerr << ">> FbMsgSingleFrame.cc mergeFirst " << deltaMillSec << " ms" << std::endl;
#   endif // end DEBUG_TIMING_LOG
}

void
FbMsgSingleFrame::mergeAllFb(scene_rdl2::grid_util::Fb &fb,
                             scene_rdl2::grid_util::LatencyLog &latencyLog)
//
// Merge all received data without using partialMergeTile logic (i.e. merge whole image at onece)
//
{
#   ifdef DEBUG_TIMING_LOG
    uint64_t cMicroSec = getCurrentMicroSec();
#   endif // end DEBUG_TIMING_LOG

    fb.reset(); // clear beauty and set nonactive condition to all other buffers.
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_FBRESET);
    for (int machineId = 0; machineId < mNumMachines; ++machineId) {
        mergeSingleFb(nullptr, machineId, fb);

        /* useful debug code
        if (mReceivedAll[machineId]) {
            if (!verifyMergedResultNumSampleSingleHost(machineId, fb)) {
                std::cerr << ">> FbMsgSingleFrame.cc mergeAllFb RUNTIME-VERIFY failed. machineId:" << machineId << " +++++++++++++\n";
            }
        }
        */
    }
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_ACCUMULATE);

    /* useful debug code
    { // verify merge result of numSample
        if (!verifyMergedResultNumSample(fb)) {
            std::cerr << ">> FbMsgSingleFrame.cc mergeAllFb RUNTIME-VERIFY failed <<<<<<<<<<<<<<<<<<\n";
        }
    }
    */

#   ifdef DEBUG_TIMING_LOG
    timeLogUpdate("-- merge --", mDebugTimeLogMerge, cMicroSec);
#   endif // end DEBUG_TIMING_LOG
}

void
FbMsgSingleFrame::mergeAllFb(const unsigned partialMergeTilesTotal,
                             scene_rdl2::grid_util::Fb &fb,
                             scene_rdl2::grid_util::LatencyLog &latencyLog)
//
// Merge all received data with partial merge tile logic.
//
{
#   ifdef DEBUG_TIMING_LOG
    uint64_t cMicroSec = getCurrentMicroSec();
#   endif // end DEBUG_TIMING_LOG

    // generate partialMergeTiles table first to control merge task volume
    std::vector<char> partialMergeTilesTbl;
    partialMergeTilesTblGen(partialMergeTilesTotal, partialMergeTilesTbl);

    // merge main stage
    fb.reset(partialMergeTilesTbl); // clear beauty and set nonactive condition to all other buffers.
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_FBRESET);
    for (int machineId = 0; machineId < mNumMachines; ++machineId) {
        mergeSingleFb(&partialMergeTilesTbl, machineId, fb);
    }
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_ACCUMULATE);

#   ifdef DEBUG_TIMING_LOG
    timeLogUpdate("-- merge --", mDebugTimeLogMerge, cMicroSec);
#   endif // end DEBUG_TIMING_LOG
}

void
FbMsgSingleFrame::mergeSingleFb(const std::vector<char> *partialMergeTilesTbl,
                                const int machineId,
                                scene_rdl2::grid_util::Fb &fb)
//
// Merge one mcrt info with partial merge tile logic
//
{
    if (mTunnelMachineIdRuntime >= 0 && mTunnelMachineIdRuntime == machineId) {
        // The tunnel operation is designed for debugging purposes and only specified single machine data
        // is directly sent to the client without any merge operations. This means all merge operation is
        // bypassed and the merge node simply sends incoming particular MCRT data to the client as is.
        // This operation is useful for debugging to narrow down the reason the bug is inside the merge
        // operation or not.
        return; // skip merge operation
    }

    if (!mReceivedAll[machineId]) return;

#   ifdef SINGLE_THREAD
    fb.accumulateRenderBuffer(partialMergeTilesTbl,    mFb[machineId]);
    fb.accumulatePixelInfo(partialMergeTilesTbl,       mFb[machineId]);
    fb.accumulateHeatMap(partialMergeTilesTbl,         mFb[machineId]);
    fb.accumulateWeightBuffer(partialMergeTilesTbl,    mFb[machineId]);
    fb.accumulateRenderBufferOdd(partialMergeTilesTbl, mFb[machineId]);
    fb.accumulateRenderOutput(partialMergeTilesTbl,    mFb[machineId]);
#   else // else SINGLE_THREAD
    tbb::parallel_for(0, 6, [&](unsigned id) {
            switch (id) {
            case 0 : fb.accumulateRenderBuffer(partialMergeTilesTbl,    mFb[machineId]); break;
            case 1 : fb.accumulatePixelInfo(partialMergeTilesTbl,       mFb[machineId]); break;
            case 2 : fb.accumulateHeatMap(partialMergeTilesTbl,         mFb[machineId]); break;
            case 3 : fb.accumulateWeightBuffer(partialMergeTilesTbl,    mFb[machineId]); break;
            case 4 : fb.accumulateRenderBufferOdd(partialMergeTilesTbl, mFb[machineId]); break;
            case 5 : fb.accumulateRenderOutput(partialMergeTilesTbl,    mFb[machineId]); break;
            }
        });
#   endif // end !SINGLE_THREAD

    if (mFeedbackActive) {
        //
        // Update mergeActionTracker
        //
        if (!partialMergeTilesTbl) {
            mMergeActionTracker[machineId].mergeFull();
        } else {
            mMergeActionTracker[machineId].mergePartial(*partialMergeTilesTbl);
        }
    }
}

#ifdef TEST
void
FbMsgSingleFrame::mergeAllFb(scene_rdl2::grid_util::Fb &fb,
                             scene_rdl2::grid_util::LatencyLog &latencyLog)
//
// Test merge for tile base MT task distribution.
//
// This is an on going test for new idea of MT task breakdown.
// Inside fb.accumulateAllFbs(), each thread picks new tile and merge all
// AOV's for this tile. So task is processed by tile base breakdown by parallel.
// Performance is not so excited compare with current solution (i.e. = each AOV
// is processed by parallel) but still lots of possibility to optimize and keep this
// code as TEST code for near future enhancement.
//
{
    fb.reset(); // clear beauty and set nonactive condition to all other buffers.
    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_FBRESET);

#   ifdef DEBUG_MSG
    {
        static int i = 0;
        i++;
        if (i < 10 ) {
            std::cerr << ">> FbMsgSingleFrame.cc mergeAllFb" << std::endl;
        }
    }
#   endif // end DEBUG_MSG

    fb.accumulateAllFbs(mNumMachines,
                        mReceivedAll,
                        mFb);

    latencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_DEQ_ACCUMULATE);
}
#endif // end TEST

bool
FbMsgSingleFrame::verifyMergedResultNumSample(const scene_rdl2::grid_util::Fb& mergedFb) const
{
    for (int machineId = 0; machineId < mNumMachines; ++machineId) {
        if (mReceivedAll[machineId]) {
            if (!verifyMergedResultNumSampleSingleHost(machineId, mergedFb)) {
                return false;
            }
        }
    }
    return true;
}

bool
FbMsgSingleFrame::verifyMergedResultNumSampleSingleHost(int machineId,
                                                        const scene_rdl2::grid_util::Fb& mergedFb) const
{
    auto verifyNumSampleBuff =
        [&](int machineId,
            unsigned totalTiles, unsigned numTileX, unsigned width, unsigned height,
            const scene_rdl2::fb_util::ActivePixels& srcActivePixels,
            const scene_rdl2::grid_util::Fb::NumSampleBuffer& srcNSBuff,
            const scene_rdl2::grid_util::Fb::NumSampleBuffer& mrgNSBuff) -> bool
        {
            constexpr unsigned tilePixSize = 8; // 8 pixels x 8 pixels
            for (unsigned tileId = 0; tileId < totalTiles; ++tileId) {
                unsigned tileX = tileId % numTileX;
                unsigned tileY = tileId / numTileX;
                unsigned tilePixOffset = tileId * 64;
                uint64_t srcMask = srcActivePixels.getTileMask(tileId);
                for (unsigned y = 0; y < tilePixSize; ++y) {
                    for (unsigned x = 0; x < tilePixSize; ++x) {
                        unsigned gx = tileX * tilePixSize + x;
                        unsigned gy = tileY * tilePixSize + y;
                        if (gx >= width || gy >= height) continue;

                        unsigned inTilePixOffset = y * tilePixSize + x;
                        bool srcActiveFlag = srcMask & ((uint64_t)(0x1) << inTilePixOffset);
                        
                        unsigned pixOffset = tilePixOffset + inTilePixOffset;
                        unsigned int srcNS = srcNSBuff.getData()[pixOffset];
                        unsigned int mrgNS = mrgNSBuff.getData()[pixOffset];
                        if (mrgNS < srcNS) {
                            std::cerr << ">> FbMsgSingleFrame.cc verifyMergeResultNumSample FAILED"
                                      << " machineId:" << machineId
                                      << " pix(" << gx << ',' << gy << ")"
                                      << " srcNS:" << srcNS
                                      << " mrgNS:" << mrgNS
                                      << " srcActiveFlag:" << scene_rdl2::str_util::boolStr(srcActiveFlag) 
                                      << '\n';
                            return false;
                        }
                    }
                }
            }
            return true;
        };

    const scene_rdl2::grid_util::Fb& srcFb = mFb[machineId];
    if (srcFb.getWidth() != mergedFb.getWidth() || srcFb.getHeight() != mergedFb.getHeight()) {
        return false;
    }

    const scene_rdl2::fb_util::ActivePixels& srcActivePixels = srcFb.getActivePixels();
    const scene_rdl2::grid_util::Fb::NumSampleBuffer& srcNSBuff = srcFb.getNumSampleBufferTiled();
    const scene_rdl2::grid_util::Fb::NumSampleBuffer& mrgNSBuff = mergedFb.getNumSampleBufferTiled();

    return verifyNumSampleBuff(machineId,
                               srcFb.getTotalTiles(), srcFb.getNumTilesX(),
                               srcFb.getWidth(), srcFb.getHeight(),
                               srcActivePixels, srcNSBuff, mrgNSBuff);
}

void
FbMsgSingleFrame::partialMergeTilesTblGen(const unsigned partialMergeTilesTotal,
                                          std::vector<char> &partialMergeTileTbl)
//
// partialMergeTilesTable generation function.
//
// This table defines which tile needs to merge under acynchronous partial merge mode.
// So far table is very simple and created from bottom to top order based on scanlines.
// But we can implemented other type easily and this is future work.
//
{
    if (mFb.empty()) return;    // just in case

    unsigned totalTiles = mFb[0].getTotalTiles();
    partialMergeTileTbl.resize(totalTiles, (char)false);

    if (partialMergeTilesTotal == 0) {
        // special case : set all true
        for (size_t i = 0; i < partialMergeTileTbl.size(); ++i) {
            partialMergeTileTbl[i] = true;
        }
        return;
    }
    
    unsigned activeStartId = std::min(mPartialMergeStartTileId, totalTiles - 1);
    unsigned activeEndId = activeStartId + std::min(partialMergeTilesTotal, totalTiles);

    if (activeEndId <= totalTiles) {
        for (size_t i = activeStartId; i < activeEndId; ++i) {
            partialMergeTileTbl[i] = true;
        }
    } else {
        for (size_t i = activeStartId; i < totalTiles; ++i) {
            partialMergeTileTbl[i] = true;
        }
        activeEndId -= totalTiles;
        for (size_t i = 0; i < activeEndId; ++i) {
            partialMergeTileTbl[i] = true;
        }
    }

    mPartialMergeStartTileId = activeEndId; // update next partial merge start tileId
}

void
FbMsgSingleFrame::timeLogUpdate(const std::string &msg,
                                scene_rdl2::rec_time::RecTimeLog &timeLog,
                                const uint64_t startMicroSec) const
{
    uint64_t deltaMicroSec = getCurrentMicroSec() - startMicroSec;
    float deltaMillSec = static_cast<float>(deltaMicroSec) / 1000.0f;

    timeLog.add(deltaMillSec);
    if (timeLog.getTotal() > 32) {
        std::cerr << ">> FbMsgSingleFrame.cc " << msg << ' '
                  << timeLog.getAverage() << " ms" << std::endl;
        timeLog.reset();
    }
}

std::string
FbMsgSingleFrame::showMessageAndReceived(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd << "mMessage (total:" << mMessage.size() << ") {\n";
    for (size_t machineId = 0; machineId < mMessage.size(); ++machineId) {
        ostr << hd << "  machineId:" << std::setw(2) << std::setfill('0') << machineId
             << " mReceived:" << ((mReceived[machineId])? "true ": "false") << '\n';
        if (mReceived[machineId]) {
            ostr << mMessage[machineId].show(hd + "  ") << '\n';
        }
    }
    ostr << hd << "}";
    return ostr.str();
}

std::string
FbMsgSingleFrame::showAllReceivedAndProgress(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd << "all (machineTotal:" << mReceivedAll.size() << ") {\n";
    for (size_t machineId = 0; machineId < mReceivedAll.size(); ++machineId) {
        ostr << hd << "  machineId:" << std::setw(2) << std::setfill('0') << machineId
             << " mReceivedAll:" << ((mReceivedAll[machineId])? "true ": "false")
             << " mProgressAll:" << mProgressAll[machineId] << '\n';
    }
    ostr << hd << "  mActiveMachines:" << mActiveMachines << '\n';
    ostr << hd << "  mProgressTotal:" << mProgressTotal << '\n';
    ostr << hd << "}";
    return ostr.str();
}

void
FbMsgSingleFrame::parserConfigure()
{
    mParser.description("FbMsgSingleFrame command");
    mParser.opt("multiChan", "<machineId> ...command...", "show info for particular machineId's multiChan data",
                [&](Arg& arg) -> bool { return parserCommandMultiChan(arg); });
    mParser.opt("fb", "<machineId> ...command...", "show interl received fb data",
                [&](Arg& arg) -> bool { return parserCommandFb(arg); });
}

bool
FbMsgSingleFrame::parserCommandMultiChan(Arg& arg)
{
    unsigned machineId = (arg++).as<unsigned>(0);
    if (machineId > mMessage.size() - 1) {
        arg.fmtMsg("machineId:%d is out of range. max:%d\n", machineId, mMessage.size());
        return false;
    }

    return mMessage[machineId].getParser().main(arg.childArg());
}

bool
FbMsgSingleFrame::parserCommandFb(Arg& arg)
{
    unsigned machineId = (arg++).as<unsigned>(0);
    if (machineId > mFb.size() - 1) {
        arg.fmtMsg("machineId:%d is out of range. max:%d\n", machineId, mMessage.size());
        return false;
    }

    return mFb[machineId].getParser().main(arg.childArg());
}

} // namespace mcrt_dataio
