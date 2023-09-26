// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TelemetryLayout.h"
#include "TelemetryOverlay.h"

#include <mcrt_dataio/engine/merger/GlobalNodeInfo.h>
#include <mcrt_dataio/share/util/FloatValueTracker.h>
#include <mcrt_messages/BaseFrame.h>
#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/rec_time/RecTime.h>

#include <memory>
#include <unordered_map>

namespace mcrt_dataio {
namespace telemetry {

class DisplayInfo
{
public:
    unsigned mOverlayWidth {0};
    unsigned mOverlayHeight {0};

    unsigned mImageWidth {0};
    unsigned mImageHeight {0};

    size_t mViewId {0};
    uint32_t mFrameId {0};
    mcrt::BaseFrame::Status mStatus {mcrt::BaseFrame::FINISHED};
    float mRenderPrepProgress {0.0f};
    float mProgress {0.0f};
    unsigned mFbActivityCounter {0};
    unsigned mDecodeProgressiveFrameCounter {0};
    bool mIsCoarsePass {true};
    float mCurrentLatencySec {0.0f};
    float mReceiveImageDataFps {0.0f};

    const GlobalNodeInfo* mGlobalNodeInfo {nullptr};

    //------------------------------

    std::string show() const;
};

class Display
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;
    using Align = Overlay::Align;

    Display() { parserConfigure(); };

    void setActive(bool sw) { mActive = sw; }
    bool getActive() const { return mActive; }

    void bakeOverlayRgb888(std::vector<unsigned char>& rgbFrame,
                           const bool top2bottomFlag,
                           const DisplayInfo& info,
                           bool bakeWithPrevArchive);

    void switchLayoutNext() { switchLayoutToNextOnTheList(); }

    Parser& getParser() { return mParser; }

    std::string show() const;

private:

    void setupLayout();
    bool setupFont();
    bool setupTestFont();
    unsigned calcFontSize() const;

    void testBakeOverlayRgb888(const DisplayInfo& info,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottomFlag,
                               bool bakeWithPrevArchive);
    bool stdBakeOverlayRgb888(const DisplayInfo& info,
                              std::vector<unsigned char>& rgbFrame,
                              const bool top2bottomFlag,
                              bool bakeWithPrevArchive);

    void drawOverlay(const DisplayInfo& info);

    void finalizeOverlayRgb888(const DisplayInfo& info,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottomFlag,
                               Align hAlign,
                               Align vAlign,
                               bool bakeWithPrevArchive);
    void copyArchive(std::vector<unsigned char>& rgbFrame) const;
    void clearBgArchive();

    void switchLayoutByName(const std::string& name,
                            const std::function<void(const std::string& msg)>);
    void switchLayoutToNextOnTheList();

    void parserConfigure();

    std::string showTestInfo() const;
    std::string showAlign(Align align) const;
    std::string showLayoutNameList() const;
    std::string showCurrentLayoutName() const;
    std::string showTimingProfile() const;
    void resetTimingProfile();

    //------------------------------

    bool mActive {false};
    bool mDoParallel {true};
    bool mTimingProfile {false};
    bool mTestMode {false};

    unsigned mOverwriteWidth {0}; // telemetry overlay size overwrite value
    unsigned mOverwriteHeight {0}; // telemetry overlay size overwrite value

    std::shared_ptr<Overlay> mOverlay;
    std::shared_ptr<Font> mFont;

    std::vector<unsigned char> mBgArchive; // initially empty

    std::string mError;

    //
    // overlay data layout
    //
    std::shared_ptr<LayoutBase> mCurrLayout;
    std::unordered_map<std::string, std::shared_ptr<LayoutBase>> mLayoutMap;

    //
    // test parameters
    //
    std::unique_ptr<Font> mTestFont;
    unsigned mTestStrX {0};
    unsigned mTestStrY {0};
    unsigned mTestStrCol[3] {255, 255, 255};
    unsigned mTestBgCol[4] {0, 0, 0, 0};
    std::string mTestMsg {"This is a test"};
    std::string mTestFontTTFFileName {"/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"};
    int mTestFontPoint {12};
    Align mTestHAlign {Align::MIDDLE};
    Align mTestVAlign {Align::MIDDLE};

    //
    // for timing profile
    //
    scene_rdl2::rec_time::RecTime mRecTime;
    FloatValueTracker mOverlayClear {64}; // sec : keep 64 events total
    FloatValueTracker mDrawStrTime {64}; // sec : keep 64 events total
    FloatValueTracker mCopyArchiveTime {64}; // sec : keep 64 events total
    FloatValueTracker mFinalizeRgb888Time {64}; // sec : keep 64 events total

    Parser mParser;
};

} // namespace telemetry
} // namespace mcrt_dataio
