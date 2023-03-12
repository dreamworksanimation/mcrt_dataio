// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

//
// -- Message data for multiple frame images --
//
// This is a data structure for keep messages aboue multiple frames for multiple machine
// environment. FbMsgMultiFrames is designed for some situation which needs to track all 
// messages independently by syncId or frameId (this is a case of realtime rendering context).
// Non realtime case (i.e. most of the current multi-machine arras_gui type application), 
// We don't use multi-frame settings to combine messages from each machine. (i.e. we are
// simply combine all syncId messages into one image. This is known as stream mode).
// Actually we only use stream mode for arras_gui type application so far and multiframe
// mode is not tested well by arras_vr yet.
//

#include "FbMsgSingleFrame.h"

namespace mcrt_dataio {

class GlobalNodeInfo;
    
class FbMsgMultiFrames
{
public:    
    enum class MergeType : unsigned {
        SEAMLESS_COMBINE = 0,   // ignore syncId, combine all info
        PICKUP_LATEST,          // always pick latest syncId info
        SYNCID_LINEUP           // get all syncId info to line up
    };

    FbMsgMultiFrames(GlobalNodeInfo *globalNodeInfo) :
        mGlobalNodeInfo(globalNodeInfo),
        mNumMachines(0),
        mMergeType(MergeType::PICKUP_LATEST),
        mStartSyncFrameId(0), mEndSyncFrameId(0),
        mDisplayFrame(nullptr),
        mDisplaySyncFrameInitialize(false),
        mDisplaySyncFrameId(0)
    {}

    bool initTotalCacheFrames(const size_t totalCacheFrames);
    bool initNumMachines(const int numMachines);
    bool initFb(const scene_rdl2::math::Viewport &rezedViewport);

    bool changeMergeType(const MergeType type, const size_t totalCacheFrames);
    void changeTaskType(const FbMsgSingleFrame::TaskType &taskType);

    bool push(const mcrt::ProgressiveFrame &progressive);

    FbMsgSingleFrame *getDisplayFbMsgSingleFrame() { return mDisplayFrame; }
    uint32_t getDisplaySyncFrameId() const { return mDisplaySyncFrameId; }

    void resetDisplayFbMsgSingleFrame(); // reset current displayFbMsgSingleFrame

    std::string show(const std::string &hd) const;
    std::string showPtrTableInfo(const std::string &hd) const;

protected:
    GlobalNodeInfo *mGlobalNodeInfo;

    int mNumMachines;
    scene_rdl2::math::Viewport mRezedViewport;

    MergeType mMergeType;

    std::vector<FbMsgSingleFrame> mFbMsgMultiFrames;

    uint32_t mStartSyncFrameId;   // oldest syncFrameId which keeps data in memory
    uint32_t mEndSyncFrameId;     // newest syncFrameId which keeps data in memory
    std::vector<FbMsgSingleFrame *> mPtrTable; // pointer table to mFbMsgMultiFrames items

    FbMsgSingleFrame *mDisplayFrame;
    bool mDisplaySyncFrameInitialize;
    uint32_t mDisplaySyncFrameId; // current display syncFrameId

    bool push_seamlessCombine(const mcrt::ProgressiveFrame &progressive);
    bool push_pickupLatest(const mcrt::ProgressiveFrame &progressive);
    bool push_syncidLineup(const mcrt::ProgressiveFrame &progressive);

    finline void shiftPtrTable();

    finline int getLocalSyncFrameId(const uint32_t syncFrameId) const;
    finline FbMsgSingleFrame *getFbMsgSingleFrame(const uint32_t syncFrameId);

    void dropOldFrameMessage();

    std::string showPtrTable(const std::string &hd) const;
}; // FbMsgMultiFrames

finline void
FbMsgMultiFrames::shiftPtrTable()
{
    FbMsgSingleFrame *ptr = mPtrTable[0];
    if (!ptr) return;
    if (ptr->getActiveMachines() > 0) dropOldFrameMessage();

    size_t lastId = mPtrTable.size() - 1;
    for (size_t i = 0; i < lastId; ++i) {
        mPtrTable[i] = mPtrTable[i+1];
    }

    ptr->resetWholeHistory(mEndSyncFrameId + 1); // reset whole history about new recycled entry
    ptr->resetAllReceivedMessagesCount();
    mPtrTable[lastId] = ptr;

    mStartSyncFrameId++;
    mEndSyncFrameId++;
    if (mDisplaySyncFrameId < mStartSyncFrameId) {
        // If displaySyncFrameId is wiped out, set oldest syncFrameId in the memory as display candidate
        mDisplaySyncFrameId = mStartSyncFrameId;
        mDisplayFrame = getFbMsgSingleFrame(mDisplaySyncFrameId);
    }

    /* useful debug message
    std::cerr << ">+> shiftPtrTable() start:" << mStartSyncFrameId
              << " end:" << mEndSyncFrameId << "/last:" << mPtrTable[lastId]->getSyncId()
              << " disp:" << mDisplaySyncFrameId << std::endl;
    */
}

finline int
FbMsgMultiFrames::getLocalSyncFrameId(const uint32_t syncFrameId) const
{
    return syncFrameId - mStartSyncFrameId;
}

finline FbMsgSingleFrame *
FbMsgMultiFrames::getFbMsgSingleFrame(const uint32_t syncFrameId)
{
    return mPtrTable[getLocalSyncFrameId(syncFrameId)];
}

} // namespace mcrt_dataio

