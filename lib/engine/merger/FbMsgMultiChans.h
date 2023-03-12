// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

//
// -- Message data for Multiple buffer --
//
// One ProgressiveFrame message consistes of multiple buffers and this FbMsgMultiChans keeps
// all of them internally over the multiple ProgressiveFrame messages.
//

#include "FbMsgSingleChan.h"

#include <mcrt_messages/BaseFrame.h>
#include <mcrt_messages/ProgressiveFrame.h>
#include <scene_rdl2/common/grid_util/Fb.h>
#include <scene_rdl2/common/platform/Platform.h> // finline

#include <tbb/parallel_for.h>

#include <memory>               // shared_ptr
#include <unordered_map>

// Basically we should use multi-thread version.
// This single thread mode is used debugging and performance comparison reason mainly.
//#define SINGLE_THREAD

namespace scene_rdl2 {
    namespace rdl2 { class ValueContainerEnq; }
    namespace grid_util { class FbAov; }
} // namespace scene_rdl2

namespace mcrt_dataio {

class GlobalNodeInfo;

class FbMsgMultiChans
{
public:
    static const char *latencyLogName;
    static const char *auxInfoName;

    using FbMsgSingleChanShPtr = std::shared_ptr<FbMsgSingleChan>;
    using FbAovShPtr = std::shared_ptr<scene_rdl2::grid_util::FbAov>;
    using DataPtr = std::shared_ptr<const uint8_t>;

    FbMsgMultiChans() :
        mGlobalNodeInfo(nullptr),
        mProgress(0.0f), mStatus(mcrt::BaseFrame::STARTED),
        mHasStartedStatus(false), mCoarsePass(true),
        mHasBeauty(false), mHasPixelInfo(false), mHasHeatMap(false), mHasRenderBufferOdd(false),
        mHasRenderOutput(false),
        mRoiViewportStatus(false),
        mSnapshotStartTime(0)
    {}

    void setGlobalNodeInfo(GlobalNodeInfo *globalNodeInfo) { mGlobalNodeInfo = globalNodeInfo; }

    finline void reset();
    bool push(const bool delayDecode, const mcrt::ProgressiveFrame &progressive, scene_rdl2::grid_util::Fb &fb);
    void decodeAll(scene_rdl2::grid_util::Fb &fb);

    float getProgress() const { return mProgress; }
    mcrt::BaseFrame::Status getStatus() const { return mStatus; }

    bool hasStartedStatus() const { return mHasStartedStatus; }

    /* Following APIs are valid after decodeData() : so far not used but might be useful for future enhancement
    bool hasBeauty() const { return mHasBeauty; }
    bool hasPixelInfo() const { return mHasPixelInfo; }
    bool hasHeatMap() const { return mHasHeatMap; }
    bool hasRenderBufferOdd() const { return mHasRenderBufferOdd; }
    bool hasRenderOutput() const { return mHasRenderOutput; }
    */
    bool isCoarsePass() const { return mCoarsePass; }

    uint64_t getSnapshotStartTime() const { return mSnapshotStartTime; }

    void encodeLatencyLog(scene_rdl2::rdl2::ValueContainerEnq &vContainerEnq); // only encode latencyLog info

    std::string show(const std::string &hd) const;

protected:
    GlobalNodeInfo *mGlobalNodeInfo;

    float mProgress;
    mcrt::BaseFrame::Status mStatus;

    bool mHasStartedStatus; // Does include STARTED status message ?
    bool mCoarsePass;

    bool mHasBeauty;          // valid by decodeData()
    bool mHasPixelInfo;       // valid by decodeData()
    bool mHasHeatMap;         // valid by decodeData()
    bool mHasRenderBufferOdd; // valid by decodeData()
    bool mHasRenderOutput;    // valid by decodeData()

    bool mRoiViewportStatus; // so far we keep roi info but not used
    scene_rdl2::math::Viewport mRoiViewport; 

    uint64_t mSnapshotStartTime;

    // currently we only used this mMsgArray for "LatencyLog" info.
    // Other buffer is immediately decoded into fb when received and not stored into mMsgArray as
    // binary stream.
    std::unordered_map<std::string, FbMsgSingleChanShPtr> mMsgArray;

    bool pushBuffer(const bool delayDecode,
                    const char *name,
                    DataPtr dataPtr,
                    const size_t dataSize,
                    scene_rdl2::grid_util::Fb &fb);
    void pushAuxInfo(const void *data, const size_t dataSize);
    void decodeData(const char *name, const void *data, const size_t dataSize,
                    scene_rdl2::grid_util::Fb &fb);
}; // FbMsgMultiChans

finline void
FbMsgMultiChans::reset()
{
    mProgress = 0.0f;
    mStatus = mcrt::BaseFrame::STARTED;

    mHasStartedStatus = false;
    mHasBeauty = false;
    mHasPixelInfo = false;
    mHasHeatMap = false;
    mHasRenderBufferOdd = false;
    mHasRenderOutput = false;
    mCoarsePass = true;

    mSnapshotStartTime = 0;     // initialize

    mMsgArray.clear();
}

} // namespace mcrt_dataio

