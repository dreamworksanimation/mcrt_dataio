// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

//
// ProgressiveFrame message construction APIs for send Fb (frame buffer) @ MCRT merger computation
//
// This class provides APIs to construct messages for sending various different image data
// to downstream by ProgressiveFrame message.
// APIs for send image data to downstream by RenderedImage is provided by grid_util::PartialFrameMerger
//

#include "FbMsgSingleFrame.h"

#include <mcrt_messages/BaseFrame.h>
#include <scene_rdl2/common/fb_util/FbTypes.h>
#include <scene_rdl2/common/grid_util/Fb.h>
#include <scene_rdl2/common/grid_util/FbActivePixels.h>
#include <scene_rdl2/common/grid_util/LatencyLog.h>
#include <scene_rdl2/common/grid_util/PackTiles.h>
#include <scene_rdl2/common/grid_util/PackTilesPassPrecision.h>
#include <scene_rdl2/common/platform/Platform.h> // finline

namespace mcrt_dataio {

class MergeFbSender
{
public:
    //
    // We have 4 different types of precision control options for PackTile encoding.
    // Basically, AUTO16 is the best option and we can achieve minimum data transfer size always.
    // Other options are mostly designed for comparison and debugging purposes.
    // The only drawback of AUTO16 is the runtime computation cost. Some AOV has
    // scene_rdl2::grid_util::CoarsePassPrecision::RUNTIME_DECISION setting and this requires
    // HDRI pixel test every time inside the encoding phase. Obviously, this test needs extra
    // overhead costs. At this moment, this overhead is in acceptable range.
    // It would be better if we add a new mode that skip RUNTIME_DECISION setting here when we can
    // use drastically increased network bandwidth in the future.
    //                                                                                                             
    enum class PrecisionControl : char {
        FULL32, // Always uses F32 for both of Coarse and Fine pass

        FULL16, // Always uses H16 if possible for both of Coarse and Fine pass.
                // However, uses F32 if minimum precision is F32

        AUTO32, // CoarsePass : Choose proper precision automatically based on the AOV data
                // FinePass   : Always uses F32

        AUTO16  // CoarsePass : Choose proper precision automatically based on the AOV data
                // FinePass   : Basically use H16. Only uses F32 if minimum precision is F32
    };

    MergeFbSender() = default;

    // Non-copyable
    MergeFbSender &operator = (const MergeFbSender) = delete;
    MergeFbSender(const MergeFbSender &) = delete;

    void setPrecisionControl(PrecisionControl &precisionControl) { mPrecisionControl = precisionControl; }

    // w, h are original size and not need to be tile size aligned
    void init(const scene_rdl2::math::Viewport &rezedViewport);

    scene_rdl2::grid_util::FbActivePixels &getFbActivePixels() { return mFbActivePixels; }
    scene_rdl2::grid_util::Fb &getFb() { return mFb; }

    void setHeaderInfoAndFbReset(FbMsgSingleFrame* currFbMsgSingleFrame,
                                 const mcrt::BaseFrame::Status* overwriteFrameStatusPtr = nullptr);
    mcrt::BaseFrame::Status getFrameStatus() const { return mFrameStatus; }
    float getProgressFraction() const { return mProgressFraction; }
    uint64_t getSnapshotStartTime() const { return mSnapshotStartTime; }
    bool getCoarsePassStatus() const { return mCoarsePassStatus; }
    const std::string& getDenoiserAlbedoInputName() const { return mDenoiserAlbedoInputName; }
    const std::string& getDenoiserNormalInputName() const { return mDenoiserNormalInputName; }

    void encodeUpstreamLatencyLog(FbMsgSingleFrame *frame);

    void addBeautyBuff(mcrt::BaseFrame::Ptr message);
    void addBeautyBuffWithNumSample(mcrt::BaseFrame::Ptr message);
    void addPixelInfo(mcrt::BaseFrame::Ptr message);
    void addHeatMap(mcrt::BaseFrame::Ptr message);
    void addHeatMapWithNumSample(mcrt::BaseFrame::Ptr message);
    void addWeightBuffer(mcrt::BaseFrame::Ptr message);
    void addRenderBufferOdd(mcrt::BaseFrame::Ptr message);
    void addRenderBufferOddWithNumSample(mcrt::BaseFrame::Ptr message);
    void addRenderOutput(mcrt::BaseFrame::Ptr message);
    void addRenderOutputWithNumSample(mcrt::BaseFrame::Ptr message);
    void addLatencyLog(mcrt::BaseFrame::Ptr message);
    void addAuxInfo(mcrt::BaseFrame::Ptr message, const std::vector<std::string> &infoDataArray);

    finline void timeLogReset() { mStartCondition = false; }
    finline void timeLogStart() { if (!mStartCondition){ mLatencyLog.start(); mStartCondition = true; } }
    finline void timeLogEnq(const scene_rdl2::grid_util::LatencyItem::Key key) { mLatencyLog.enq(key); }
    finline void timeLogEnq(const scene_rdl2::grid_util::LatencyItem::Key key, const std::vector<uint32_t> &data);

    scene_rdl2::grid_util::LatencyLog &getLatencyLog() { return mLatencyLog; }

protected:
    using PackTilePrecision = scene_rdl2::grid_util::PackTiles::PrecisionMode;
    using PackTilePrecisionCalcFunc = std::function<PackTilePrecision()>;
    using CoarsePassPrecision = scene_rdl2::grid_util::CoarsePassPrecision;
    using FinePassPrecision = scene_rdl2::grid_util::FinePassPrecision;

    PrecisionControl mPrecisionControl {PrecisionControl::AUTO16};

    scene_rdl2::grid_util::FbActivePixels mFbActivePixels; // snapshot result
    scene_rdl2::grid_util::Fb mFb;

    //------------------------------

    size_t mLastBeautyBufferSize {0};             // last beauty buffer packTile data size
    size_t mLastBeautyBufferNumSampleSize {0};    // last beauty buffer w/ numSample packTile data size
    size_t mLastPixelInfoSize {0};                // last pixelInfo packTile data size
    size_t mLastHeatMapSize {0};                  // last heatMap packTile data size
    size_t mLastHeatMapNumSampleSize {0};         // last heatMap packTile data size
    size_t mLastWeightBufferSize {0};             // last weightBuffer packTile data size
    size_t mLastRenderBufferOddSize {0};          // last renderBufferOdd packTile data size
    size_t mLastRenderBufferOddNumSampleSize {0}; // last renderBufferOdd w/ numSample packTile data size
    size_t mLastRenderOutputSize {0};             // last renderOutput packTile data size
    size_t mMin {0};                              // for performance analyze. packet size min info
    size_t mMax {0};                              // for performance analyze. packet size max info

    std::string mWork; // work memory for encoding

    //------------------------------

    bool mStartCondition {false};
    scene_rdl2::grid_util::LatencyLog mLatencyLog;

    //------------------------------

    mcrt::BaseFrame::Status mFrameStatus {mcrt::BaseFrame::ERROR}; // current image's frame status
    float mProgressFraction {0.0f};  // current image's progress fraction
    uint64_t mSnapshotStartTime {0}; // current image's snapshot start time on mcrt computation
    bool mCoarsePassStatus {true};   // current image's coarse pass or not status
    std::string mDenoiserAlbedoInputName;
    std::string mDenoiserNormalInputName;

    enum class HdriTestCondition : char {
        INIT,    // Initial condition. The test has not been completed yet
        HDRI,    // The test has been completed and the result included HDRI pixel
        NON_HDRI // The test has been completed and the result did not include HDRI pixel
    };
    // status of execution of HDRI test for beauty buffer after done snapshotDelta
    HdriTestCondition mBeautyHDRITest {HdriTestCondition::INIT};

    //------------------------------

    // We need separate work buffer for upstreamLatencyLog from mWork
    std::string mUpstreamLatencyLogWork; // updated by encodeUpstreamLatencyLog()

    //------------------------------

    void fbReset();

    PackTilePrecision getBeautyHDRITestResult();
    bool beautyHDRITest() const;
    bool renderOutputHDRITest(const scene_rdl2::grid_util::Fb::FbAovShPtr fbAov) const;
    PackTilePrecision calcPackTilePrecision(const CoarsePassPrecision coarsePassPrecision,
                                            const FinePassPrecision finePassPrecision,
                                            PackTilePrecisionCalcFunc runtimeDecisionFunc = nullptr) const;

    finline uint8_t *duplicateWorkData(const std::string &work);
}; // MergeFbSender

finline void
MergeFbSender::timeLogEnq(const scene_rdl2::grid_util::LatencyItem::Key key,
                          const std::vector<uint32_t> &data)
{
    mLatencyLog.enq(key, data);
}

finline uint8_t *
MergeFbSender::duplicateWorkData(const std::string &work)
{
    uint8_t *data = new uint8_t[work.size()];
    std::memcpy(data, work.data(), work.size());
    return data;
}

} // namespace mcrt_dataio
