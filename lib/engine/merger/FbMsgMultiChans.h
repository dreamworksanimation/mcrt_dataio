// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

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
#include <scene_rdl2/common/grid_util/Arg.h>
#include <scene_rdl2/common/grid_util/Fb.h>
#include <scene_rdl2/common/grid_util/Parser.h>
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
class MergeActionTracker;

class FbMsgMultiChans
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;

    static constexpr const char* const latencyLogName         = "latencyLog";
    static constexpr const char* const latencyLogUpstreamName = "latencyLogUpstream";
    static constexpr const char* const auxInfoName            = "auxInfo";

    using FbMsgSingleChanShPtr = std::shared_ptr<FbMsgSingleChan>;
    using FbAovShPtr = std::shared_ptr<scene_rdl2::grid_util::FbAov>;
    using DataPtr = std::shared_ptr<const uint8_t>;

    explicit FbMsgMultiChans(bool debugMode = false)
        : mDebugMode(debugMode)
    {
        parserConfigure();
    }

    void setGlobalNodeInfo(GlobalNodeInfo *globalNodeInfo) { mGlobalNodeInfo = globalNodeInfo; }

    finline void reset();
    bool push(const bool delayDecode,
              const mcrt::ProgressiveFrame &progressive,
              scene_rdl2::grid_util::Fb &fb,
              const bool parallelExec = true,
              const bool skipLatencyLog = false);
    void decodeAll(scene_rdl2::grid_util::Fb& fb, MergeActionTracker* mergeActionTracker);

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
    std::string show() const;

    Parser& getParser() { return mParser; }

protected:
    bool mDebugMode;

    GlobalNodeInfo *mGlobalNodeInfo {nullptr};

    // sendImageActionId is a unique increment id from starting the process and never reset
    std::vector<unsigned> mSendImageActionIdData;

    float mProgress {0.0f};
    mcrt::BaseFrame::Status mStatus {mcrt::BaseFrame::STARTED};

    bool mHasStartedStatus {false}; // Does include STARTED status message ?
    bool mCoarsePass {true};

    bool mHasBeauty {false};          // valid by decodeData()
    bool mHasPixelInfo {false};       // valid by decodeData()
    bool mHasHeatMap {false};         // valid by decodeData()
    bool mHasRenderBufferOdd {false}; // valid by decodeData()
    bool mHasRenderOutput {false};    // valid by decodeData()

    bool mRoiViewportStatus {false}; // so far we keep roi info but not used
    scene_rdl2::math::Viewport mRoiViewport; 

    uint64_t mSnapshotStartTime {0};

    //
    // We have 2 options to process the received message.
    //
    // a) Immediate decode mode
    // All the received progressiveFrame messages are immediately decoded and stored in the frame buffer.
    // This is a better solution for real-time rendering. Because we need to process all the received
    // messages as soon as possible under real-time conditions. 
    // In this case, Only "LatencyLog" info is saved into mMsgArray and does not store image info into them.
    // Also, we need immediate decode mode for processing progressiveFeedback messages at MCRT computation
    // as well.
    //
    // b) Delaied decode mode 
    // All the received messages are processed when needed at the merge action (i.e. not decoded at the
    // received time). We keep received data as just binary data without decoding. This is good for most
    // of the interactive lighting sessions. Because decode action is basically a pretty CPU intensive task
    // and we should decode necessary data only. Sometimes, we received out of date info and want to skip
    // decode action for them due to depend on the network latency and multi-machine timing issue. This is
    // happening a lot if the camera is quickly updated under many MCRT computations configurations.
    // In this case, all received data is stored into mMsgArray and properly selected then decoded at
    // merge time.
    //
    std::unordered_map<std::string, FbMsgSingleChanShPtr> mMsgArray;

    Parser mParser;

    //------------------------------

    bool pushBuffer(const bool delayDecode,
                    const bool skipLatencyLog,
                    const char *name,
                    DataPtr dataPtr,
                    const size_t dataSize,
                    scene_rdl2::grid_util::Fb &fb);
    void pushAuxInfo(const void *data, const size_t dataSize);

    void decodeData(const char* name, const void* data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeBeautyWithNumSample(const void* data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeBeauty(const void* data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeBeautyOddWithNumSample(const void* data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeBeautyOdd(const void* data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodePixelInfo(const char* name,
                         const void* data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeHeatMapWithNumSample(const char* name,
                                    const void *data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeHeatMap(const char* name,
                       const void *data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeWeight(const char* name,
                      const void *data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeReference(const char* name,
                         const void *data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);
    void decodeRenderOutputAOV(const char* name,
                               const void *data, const size_t dataSize, scene_rdl2::grid_util::Fb& fb);

    void parserConfigure();
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
