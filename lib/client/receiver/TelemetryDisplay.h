// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "TelemetryLayout.h"
#include "TelemetryOverlay.h"
#include "TelemetryPanel.h"

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

    const std::string* mClientMessage {nullptr};

    unsigned mImageWidth {0};
    unsigned mImageHeight {0};

    size_t mViewId {0};
    uint32_t mFrameId {0};
    float mElapsedSecFromStart {0.0f}; // sec
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

    std::vector<std::string> getAllPanelName();
    void setTelemetryInitialPanel(const std::string& panelName);
    bool switchPanelByName(const std::string& panelName);
    void switchPanelToNext();
    void switchPanelToPrev();
    void switchPanelToParent();
    void switchPanelToChild();

    Parser& getParser() { return mParser; }

    std::string show() const;

private:
    using FontShPtr = std::shared_ptr<Font>;
    using LayoutBaseShPtr = std::shared_ptr<LayoutBase>;
    using OverlayShPtr = std::shared_ptr<Overlay>;
    using PanelShPtr = std::shared_ptr<Panel>;
    using PanelTableShPtr = std::shared_ptr<PanelTable>;

    void setupRootPanelTable();
    PanelShPtr genPanel(const std::string& panelName,
                        const std::string& layoutName,
                        const std::string& setupOptions) const;
    LayoutBaseShPtr genLayout(const std::string& panelName,
                              const std::string& layoutName) const;

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

    bool findPanelTest(const std::string& panelName) const;

    void parserConfigure();

    std::string showTestInfo() const;
    std::string showAlign(Align align) const;
    std::string showCurrentLayoutName() const;
    std::string showTimingProfile() const;
    std::string showFindPanelTest(const std::string& panelName) const;
    std::string showCurrentPanelName() const;
    std::string showAllPanelName();
    void resetTimingProfile();

    //------------------------------

    bool mActive {false};
    bool mDoParallel {true};
    bool mTimingProfile {false};
    bool mTestMode {false};

    unsigned mOverwriteWidth {0}; // telemetry overlay size overwrite value
    unsigned mOverwriteHeight {0}; // telemetry overlay size overwrite value

    OverlayShPtr mOverlay;
    FontShPtr mFont;

    std::vector<unsigned char> mBgArchive; // initially empty

    std::string mError;

    //
    // overlay data layout
    //
    std::string mInitialPanelName;
    PanelTableShPtr mRootPanelTable {nullptr};
    PanelTableStack mPanelTableStack;

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
