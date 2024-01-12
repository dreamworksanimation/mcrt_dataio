// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "FbMsgMultiFrames.h"
#include "GlobalNodeInfo.h"

#include <scene_rdl2/scene/rdl2/ValueContainerDeq.h>

#include <iomanip>
#include <sstream>

//#define DEBUG_MSG_PUSH

namespace mcrt_dataio {

bool
FbMsgMultiFrames::initTotalCacheFrames(const size_t totalCacheFrames)
{
    if (mMergeType == MergeType::SEAMLESS_COMBINE ||
        mMergeType == MergeType::PICKUP_LATEST) {
        // This case, we don't use mPtrTable because mFbMsgMultiFrames is stable and only keep one item.
        mFbMsgMultiFrames.resize(1); // we don't need more than 1 in this case
        mFbMsgMultiFrames[0].setGlobalNodeInfo(mGlobalNodeInfo);
        mFbMsgMultiFrames[0].setTunnelMachineIdStaged(mTunnelMachineId);

        mDisplaySyncFrameInitialize = false;
        mDisplaySyncFrameId = 0;
        mDisplayFrame = &mFbMsgMultiFrames[0];

        if (mMergeType == MergeType::SEAMLESS_COMBINE) {
            // does not support feedback logic under SEAMLESS_COMBINE mode
            mFbMsgMultiFrames[0].resetFeedback(false);
        }

    } else if (mMergeType == MergeType::SYNCID_LINEUP) {
        mFbMsgMultiFrames.resize(totalCacheFrames);

        mPtrTable.resize(totalCacheFrames);
        for (size_t frameId = 0; frameId < mFbMsgMultiFrames.size(); ++frameId) {
            mFbMsgMultiFrames[frameId].setGlobalNodeInfo(mGlobalNodeInfo);
            mFbMsgMultiFrames[frameId].setTunnelMachineIdStaged(mTunnelMachineId);
            if (!mFbMsgMultiFrames[frameId].init(mNumMachines)) return false;
            if (!mFbMsgMultiFrames[frameId].initFb(mRezedViewport)) return false;
            mPtrTable[frameId] = &mFbMsgMultiFrames[frameId];
        }
        mStartSyncFrameId = 0;
        mEndSyncFrameId = 0;

        mDisplaySyncFrameInitialize = false;
        mDisplaySyncFrameId = 0;
        mDisplayFrame = nullptr;
    }

    return true;
}

bool
FbMsgMultiFrames::initNumMachines(const int numMachines)
{
    mNumMachines = numMachines;

    for (size_t frameId = 0; frameId < mFbMsgMultiFrames.size(); ++frameId) {
        if (!mFbMsgMultiFrames[frameId].init(mNumMachines)) return false;
        if (!mFbMsgMultiFrames[frameId].initFb(mRezedViewport)) return false;

        if (mMergeType == MergeType::SYNCID_LINEUP) {
            mPtrTable[frameId] = &mFbMsgMultiFrames[frameId];
        }
    }

    return true;
}

bool
FbMsgMultiFrames::initFb(const scene_rdl2::math::Viewport &rezedViewport)
//
// origianl w and h (and no need to be tile aligned)
//
{
    mRezedViewport = rezedViewport;

    for (size_t frameId = 0; frameId < mFbMsgMultiFrames.size(); ++frameId) {
        if (!mFbMsgMultiFrames[frameId].initFb(rezedViewport)) {
            return false;
        }
    }
    return true;
}

bool
FbMsgMultiFrames::changeMergeType(const MergeType type,
                                  const size_t totalCacheFrames)
{
    if (mMergeType == type) {
        if (type == MergeType::SYNCID_LINEUP && mFbMsgMultiFrames.size() != totalCacheFrames) {
            return initTotalCacheFrames(totalCacheFrames);
        }
        return true;            // skip : we don't need to modify
    }

    mMergeType = type;
    return initTotalCacheFrames(totalCacheFrames);
}    

void
FbMsgMultiFrames::changeTaskType(const FbMsgSingleFrame::TaskType &taskType)
{
    for (FbMsgSingleFrame &frame : mFbMsgMultiFrames) {
        frame.changeTaskType(taskType);
    }
}

bool
FbMsgMultiFrames::push(const mcrt::ProgressiveFrame &progressive,
                       const std::function<bool()>& feedbackInitCallBack)
{
    if (progressive.getProgress() < 0.0f) {
        //
        // Special case, this message only contains auxInfo data (no image information).
        // And it handles by special way
        //
        if (mGlobalNodeInfo) {
            static const std::string auxInfoName("auxInfo");
            for (const mcrt::BaseFrame::DataBuffer &buffer: progressive.mBuffers) {
                if (!std::strcmp(buffer.mName, auxInfoName.c_str())) {
                    scene_rdl2::rdl2::ValueContainerDeq cDeq(buffer.mData.get(), buffer.mDataLength);
                    std::vector<std::string> infoDataArray = cDeq.deqStringVector();
                    mGlobalNodeInfo->decode(infoDataArray);
                }
            }
        }
        return true;
    }

    bool rt = true;
    switch (mMergeType) {
    case MergeType::SEAMLESS_COMBINE: rt = push_seamlessCombine(progressive); break;
    case MergeType::PICKUP_LATEST: rt = push_pickupLatest(progressive, feedbackInitCallBack); break;
    case MergeType::SYNCID_LINEUP: rt = push_syncidLineup(progressive); break;
    }
    return rt;
}

void
FbMsgMultiFrames::resetDisplayFbMsgSingleFrame()
{
    FbMsgSingleFrame *currDisplayFrame = getDisplayFbMsgSingleFrame();
    if (currDisplayFrame) {
        currDisplayFrame->resetWholeHistory(0);
    }
}

std::string
FbMsgMultiFrames::show(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd << "FbMsgMultiFrames {\n";
    ostr << hd << "  mNumMachines:" << mNumMachines << '\n';
    ostr << showPtrTable(hd + "  ") << '\n';
    ostr << hd << "}";
    return ostr.str();
}

std::string
FbMsgMultiFrames::showPtrTableInfo(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd << "FbMsgMultiFrames {\n";
    ostr << hd << "  mStartSyncFrameId:" << mStartSyncFrameId << '\n';
    ostr << hd << "  mEndSyncFrameId:" << mEndSyncFrameId << '\n';
    ostr << hd << "  mDisplaySyncFrameId:" << mDisplaySyncFrameId << '\n';
    ostr << hd << "  mPtrTable (total:" << mPtrTable.size() << ") {\n";
    for (size_t i = 0; i < mPtrTable.size(); ++i) {
        ostr << hd << "    i:" << std::setw(2) << std::setfill('0') << i
             << " 0x" << std::hex << mPtrTable[i] << std::endl;
    }
    ostr << hd << "  }\n";
    ostr << hd << "}";
    return ostr.str();
}

//-------------------------------------------------------------------------------------------------------------

bool
FbMsgMultiFrames::push_seamlessCombine(const mcrt::ProgressiveFrame &progressive)
//
// This is a push logic for MergeType::SEAMLESS_COMBINE
//
//   All individual received MCRT data are simply combine regardress of each syncFrameId.
//   mDisplaySyncFrameId is always pick newest id from all of the MCRT computation result.
//   This means combined image might includes older information than mDisplaySyncFrameId.
//
//   This seamlessCombine mode is working well with uniform sampling but not working with
//   adaptive sampling by using feedback logic. There is fundamental restriction and hard
//   to support the current version of feedback logic by SEAMLESS_COMBINE mode..
//   So, SEAMLESS_COMBINE does not support image synchronization feedback logic at this moment.
//    
{
    uint32_t syncFrameId = progressive.mHeader.mFrameId;
    
    if (!mDisplaySyncFrameInitialize) {
        // initialize start/end/display frameIds
        mDisplaySyncFrameId = syncFrameId; // set syncFrameId as display for initial condition.
        mDisplaySyncFrameInitialize = true;
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> FbMsg.cc FbMsgMultiFrames::push_seamlessCombine() INIT :"
                  << " displaySyncFrameId:" << mDisplaySyncFrameId << std::endl;
#       endif // end DEBUG_MSG_PUSH

        /* Probably need to think about resetWholeHistory and resetAllReceivedMessagesCount
           for seamless mode as well here. Work in progress Toshi. (Mar/11/2019)
        */
    }

    if (mDisplaySyncFrameId < syncFrameId) {
        mDisplaySyncFrameId = syncFrameId; // update display syncFrameId when get newer id
    }

    FbMsgSingleFrame *currFbMsgSingleFrame = &mFbMsgMultiFrames[0]; // SEAMLESS_COMBINE mode only has 1 item
    if (!currFbMsgSingleFrame->push(progressive)) {
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> Fbmsg.cc FbMsgMultiFrames::push_seamlessCombine() push() failed" << std::endl;
#       endif // end DEBUG_MSG_PUSH
        return false; // failed to store progressive messages data
    }

    return true;
}

bool
FbMsgMultiFrames::push_pickupLatest(const mcrt::ProgressiveFrame &progressive,
                                    const std::function<bool()>& feedbackInitCallBack)
//
// This is a push logic for MergeType::PICKUP_LATEST
//
//   push always pick most newest syncFrameId data only. Old data is immediately removed
//   when processed newer syncFrameId data. This logic is designed for "Multiplex pixel"
//   task distribution mode.
//   mDisplaySyncFrameId is always set by newest syncFrameid.
//    
//   This PICK_LATEST mode is considered with image synchronization feedback logic that is
//   designed for multi-machine adaptive sampling.
//    
{
    uint32_t syncFrameId = progressive.mHeader.mFrameId;
    
    if (!mDisplaySyncFrameInitialize) {
        // initialize start/end/display frameIds
        mDisplaySyncFrameId = syncFrameId; // set syncFrameId as display for initial condition.
        mDisplaySyncFrameInitialize = true;
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> FbMsg.cc FbMsgMultiFrames::push_pickupLatest() INIT :"
                  << " displaySyncFrameId:" << mDisplaySyncFrameId << std::endl;
#       endif // end DEBUG_MSG_PUSH

        FbMsgSingleFrame *currFbMsgSingleFrame = &mFbMsgMultiFrames[0]; // SEAMLESS_COMBINE only has 1 item
        currFbMsgSingleFrame->resetWholeHistory(syncFrameId); // reset whole history
        currFbMsgSingleFrame->resetAllReceivedMessagesCount();
        currFbMsgSingleFrame->resetFeedback(*mFeedback);
        if (!feedbackInitCallBack()) return false;
    }

    if (syncFrameId < mDisplaySyncFrameId) {
        // We don't care about old message
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> FbMsg.cc FbMsgMultiFrames::push_pickupLatest() RM OLD :"
                  << " syncFrameId:" << syncFrameId
                  << " < mDisplaySyncFrameId:" << mDisplaySyncFrameId << std::endl;
#       endif // end DEBUG_MSG_PUSH
        return true;            // early exit
    }

    FbMsgSingleFrame *currFbMsgSingleFrame = &mFbMsgMultiFrames[0]; // PICKUP_LATEST mode only has 1 item

    if (mDisplaySyncFrameId < syncFrameId) {
        // We got new syncFrameId and need to work on this syncFrameId with full reset
        mDisplaySyncFrameId = syncFrameId;
        currFbMsgSingleFrame->resetWholeHistory(syncFrameId); // reset whole history
        currFbMsgSingleFrame->resetAllReceivedMessagesCount();
        currFbMsgSingleFrame->resetFeedback(*mFeedback);
        if (!feedbackInitCallBack()) return false;
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> Fbmsg.cc FbMsgMultiFrames::push_pickupLatest() NEW-SYNCID :"
                  << " mDisplaySyncFrameId:" << mDisplaySyncFrameId << std::endl;
#       endif // end DEBUG_MSG_PUSH            
    }

    if (!currFbMsgSingleFrame->push(progressive)) {
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> Fbmsg.cc FbMsgMultiFrames::push_seamlessCombine() push() failed" << std::endl;
#       endif // end DEBUG_MSG_PUSH
        return false; // failed to store progressive messages data
    }

    return true;
}

bool
FbMsgMultiFrames::push_syncidLineup(const mcrt::ProgressiveFrame &progressive)
//
// This is a push logic for MergeType::SYNCID_LINEUP
//
//   Keep all syncFrameId data and try to show each frame as old to new order. Each frame is ready
//   to send to down stream computation when each frame received from data from all MCRT computations.
//   But we have limited memory buffer size which defined by configuration stage and manage all frames
//   merge operation inside this already defined buffer size. This means some of the frame might be
//   dropped under some extream situation and skip to display due to could not received all nesessary
//   result from all MCRT computations.
//
//   This SYNCID_LINEUP mode should be only used for realtime rendering context.
//   
//   This SYNCID_LINEUP mode does not support image synchronization feedback logic due to this mode is
//   designed only for real-time rendering (i.e. pretty small rendering time budget) and does not make
//   sense to support image synchronization feedback logic.
//    
{
    uint32_t syncFrameId = progressive.mHeader.mFrameId;
    
    if (!mDisplaySyncFrameInitialize) {
        // initialize start/end/display frameIds
        mStartSyncFrameId = syncFrameId;
        mEndSyncFrameId = mStartSyncFrameId + static_cast<uint32_t>(mFbMsgMultiFrames.size()) - 1;
        mDisplaySyncFrameId = mStartSyncFrameId; // set oldest frame as display for initial condition.
        mDisplayFrame = getFbMsgSingleFrame(mDisplaySyncFrameId);

        mDisplaySyncFrameInitialize = true;

#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> FbMsg.cc FbMsgMultiFrames::push_syncidLineup() INIT :"
                  << " startSyncFrameId:" << mStartSyncFrameId
                  << " endSyncFrameId:" << mEndSyncFrameId
                  << " displaySyncFrameId:" << mDisplaySyncFrameId << std::endl;
#       endif // end DEBUG_MSG_PUSH
    }

    // Early exit test
    if (syncFrameId < mDisplaySyncFrameId) {
        // We don't care about messages older than displaySyncFrameId
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> FbMsg.cc FbMsgMultiFrames::push_syncidLineup() RM OLD :"
                  << " syncFrameId:" << syncFrameId
                  << " < mDisplaySyncFrameId:" << mDisplaySyncFrameId << std::endl;
#       endif // end DEBUG_MSG_PUSH
        return true;            // early exit
    }

    // Update FbMsgFrame pointer table
    if (syncFrameId > mEndSyncFrameId) {
        uint32_t shiftOffset = syncFrameId - mEndSyncFrameId;

#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> FbMsg.cc FbMsgMultiFrames::push_syncidLineup() SHIFT "
                  << " syncFrameId:" << syncFrameId
                  << " shiftOffset:" << shiftOffset << std::endl;
#       endif // end DEBUG_MSG_PUSH
        for (uint32_t i = 0; i < shiftOffset; ++i) {
            shiftPtrTable();    // shift 1 sync frame advance.
        }
    }

    FbMsgSingleFrame *currFbMsgSingleFrame = getFbMsgSingleFrame(syncFrameId);
    bool prevReadyCondition = currFbMsgSingleFrame->isReadyAll(); // save previous status for condition check
    if (!currFbMsgSingleFrame->push(progressive)) {
#       ifdef DEBUG_MSG_PUSH
        std::cerr << ">+> Fbmsg.cc FbMsgMultiFrames::push() currFbMsgSingleFrame->push() failed" << std::endl;
#       endif // end DEBUG_MSG_PUSH
        return false; // failed to store progressive messages data
    }

    if (!prevReadyCondition && currFbMsgSingleFrame->isReadyAll()) {
        // previous condition is OFF and current condition is ON.
        // This means this timing is completed to received at least on message from all MCRT
        // computations.
        if (mDisplaySyncFrameId < syncFrameId) {
                // If current syncFrameId is newer than mDisplaySyncFrameId, we should update
                // displaySyncFrameId as current syncFrameId
            mDisplaySyncFrameId = syncFrameId;
            mDisplayFrame = getFbMsgSingleFrame(mDisplaySyncFrameId);
        }
    }

    return true;
}

void
FbMsgMultiFrames::dropOldFrameMessage()
{
    std::cerr << ">> drop frame. (start syncFrameId:" << mStartSyncFrameId << ")" << std::endl;
}
    
std::string
FbMsgMultiFrames::showPtrTable(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd << "mPtrTable info {\n";
    ostr << hd << "  mStartSyncFrameId:" << mStartSyncFrameId << '\n';
    ostr << hd << "  mEndSyncFrameId:" << mEndSyncFrameId << '\n';
    ostr << hd << "  mDisplaySyncFrameId:" << mDisplaySyncFrameId << '\n';
    ostr << hd << "  ptrTable (total:" << mPtrTable.size() << ") {\n";
    for (size_t i = 0; i < mPtrTable.size(); ++i) {
        ostr << hd << "    i:" << std::setw(2) << std::setfill('0') << i << '\n';
        ostr << mPtrTable[i]->show(hd + "    ") << '\n';
    }
    ostr << hd << "  }\n";
    ostr << hd << "}";
    return ostr.str();
}

} // namespace mcrt_dataio
