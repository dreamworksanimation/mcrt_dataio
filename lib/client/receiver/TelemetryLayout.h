// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "TelemetryOverlay.h"

#include <mcrt_dataio/engine/mcrt/McrtNodeInfo.h>
#include <mcrt_messages/BaseFrame.h>

namespace mcrt_dataio {

class GlobalNodeInfo;    

namespace telemetry {

class Display;
class DisplayInfo;

class LayoutBase
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using FontShPtr = std::shared_ptr<Font>;
    using OverlayShPtr = std::shared_ptr<Overlay>;
    using Parser = scene_rdl2::grid_util::Parser;

    LayoutBase(const std::string& name, OverlayShPtr overlay, FontShPtr font);
    virtual ~LayoutBase() {}

    const std::string& getName() const { return mName; }

    virtual void drawMain(const DisplayInfo& info) = 0;

    static std::string colFg(const C3& c);
    static std::string colBg(const C3& c);

    Parser& getParser() { return mParser; }

protected:

    unsigned getFontStepX() {
        return (mOverlay->getFontStepX() == 0) ? mFont->getFontSizePoint() : mOverlay->getFontStepX();
    }

    //
    // draw utility functions
    //
    std::string colReset() const { return colFg(mCharFg) + colBg(mCharBg); }

    //------------------------------

    std::string strFps(const float v) const;
    std::string strPct(const float fraction) const;
    std::string strSec(const float sec) const;
    std::string strMillisec(float millisec) const;
    std::string strByte(const size_t size, const size_t minOutStrLen = 8) const;
    std::string strBps(const float bps, const size_t minOutStrLen = 10) const;
    std::string strBar(const unsigned barWidth,
                       const unsigned fontStepX,
                       const std::string& title,
                       const float fraction,
                       const bool usageMode,
                       unsigned* barStartOffsetPixX = nullptr,
                       unsigned* barEndOffsetPixX = nullptr,
                       unsigned* barHeight = nullptr) const;
    std::string strBool(const bool flag) const;
    std::string strSimpleHostName(const std::string& hostName) const;
    std::string strFrameStatus(const mcrt::BaseFrame::Status& status,
                               const float renderPrepProgress) const;
    std::string strPassStatus(const bool isCoarsePass) const;
    std::string strExecMode(const McrtNodeInfo::ExecMode& execMode) const;

    //------------------------------

    // horizontal box bar
    void drawHBoxBar(const unsigned leftX, const unsigned leftY,
                     const unsigned barStartOffsetPixX, const unsigned barEndOffsetPixX,
                     const unsigned barHeight,
                     const float fraction,
                     const C3& c, const unsigned char alpha);

    // horizontal box bar that consists of 2 consecutive sections
    void drawHBoxBar2Sections(const unsigned leftX, const unsigned leftY,
                              const unsigned barStartOffsetPixX, const unsigned barEndOffsetPixX,
                              const unsigned barHeight,
                              const float fractionA,
                              const C3& cA, const unsigned char alphaA,
                              const float fractionB,
                              const C3& cB, const unsigned char alphaB);

    // horizontal bar with title
    void drawHBarWithTitle(const unsigned barLeftBottomX,
                           const unsigned barLeftBottomY,
                           const unsigned barWidth,
                           const std::string& title,
                           const float fraction,
                           const bool usageMode);

    // borizontal bar that consists of 2 consecutive sections with title
    void drawHBar2SectionsWithTitle(const unsigned barLeftBottomX,
                                    const unsigned barLeftBottomY,
                                    const unsigned barWidth,
                                    const std::string& title,
                                    const float fractionA,
                                    const float fractionB,
                                    const bool usageMode);

    // vertical line
    void drawVLine(const unsigned x, const unsigned minY, const unsigned maxY, const C3& c, const unsigned char alpha);

    // vertical bar graph
    void drawVBarGraph(const unsigned leftBottomX,
                       const unsigned leftBottomY,
                       const unsigned rightTopX,
                       const unsigned rightTopY,
                       const unsigned rulerYSize,
                       const ValueTimeTracker& vtt,
                       const C3& c,
                       const unsigned char alpha,
                       const float graphTopY, // auto adjust display Y range if this value is 0 or negative
                       float& maxValue,
                       float& currValue);

    // vertical bar graph with title for bps value
    void drawBpsVBarGraphWithTitle(const unsigned leftBottomX,
                                   const unsigned leftBottomY,
                                   const unsigned rightTopX,
                                   const unsigned rightTopY,
                                   const unsigned rulerYSize,
                                   const ValueTimeTracker& vtt,
                                   const C3& c,
                                   const unsigned char alpha,
                                   const float graphTopY, // auto adjust display Y range if this val is 0 or negative
                                   const std::string& title);

    //------------------------------

    Overlay::BBox2i setBBox(const int minX, const int minY, const int maxX, const int maxY) const {
        return Overlay::BBox2i {scene_rdl2::math::Vec2i {minX, minY}, scene_rdl2::math::Vec2i {maxX, maxY}};
    }

    unsigned calcMaxSimpleMcrtHostNameLen(const GlobalNodeInfo* gNodeInfo) const;

    std::string showC3(const C3& c) const;

    unsigned char getArgC0255(Arg& arg) const;
    C3 getArgC3(Arg& arg) const;

    //------------------------------

    void parserConfigure();

    //==============================

    std::string mName;

    C3 mCharFg {255, 255, 255};
    C3 mCharBg {0, 0, 0};

    C3 mPanelBg {32, 32, 32};    
    float mPanelBgAlpha {200};

    OverlayShPtr mOverlay;
    FontShPtr mFont;

    unsigned mMaxYLines {0};
    unsigned mOffsetBottomPixY {0};
    unsigned mStepPixY {0}; // single char height w/ spaceY computed by Overlay::getMaxYLines()

    std::string mError;

    Parser mParser;
};

class LayoutPanel : public LayoutBase
{
public:
    using FontShPtr = std::shared_ptr<Font>;
    using OverlayShPtr = std::shared_ptr<Overlay>;

    LayoutPanel(const std::string& name, OverlayShPtr overlay, FontShPtr font)
        : LayoutBase(name, overlay, font)
    {}

protected:
    // Telemetry overlay Title sub-panel
    void subPanelTitle(const DisplayInfo& info);

    // Single string sub-panel
    void subPanelMessage(const unsigned x,
                         const unsigned y,
                         const std::string& str,
                         Overlay::BBox2i& bbox);
    void subPanelMessageUpperRight(const unsigned x, // upper right X
                                   const unsigned y, // upper right Y
                                   const std::string& str,
                                   Overlay::BBox2i& bbox);
    void subPanelMessageCenter(const unsigned x, // center X
                               const unsigned y, // center Y
                               const std::string& str,
                               Overlay::BBox2i& bbox);

    // Global Info
    void subPanelGlobalInfo(const unsigned x,
                            const unsigned y,
                            const DisplayInfo& info,
                            Overlay::BBox2i& bbox);

    // Global progress bar
    void subPanelGlobalProgressBar(const unsigned barLeftBottomX,
                                   const unsigned barLeftBottomY,
                                   const unsigned barWidth,
                                   const DisplayInfo& info,
                                   Overlay::BBox2i& bboxGlobalProgressBar);

    // NetIO, Cpu/Mem monitor, RenderPrep/MCRT progress
    void subPanelNetIOCpuMemAndProgress(const unsigned leftBottomX,
                                        const unsigned leftBottomY,
                                        const unsigned rightTopX,
                                        const unsigned rightTopY,
                                        const float graphTopY, // auto adjust Y if this value is 0 or negative
                                        const unsigned rulerYSize,
                                        const std::string& title,
                                        const int cpuTotal,
                                        const float cpuFraction,
                                        const size_t memTotal, // byte
                                        const float memFraction,
                                        const float renderPrepFraction,
                                        const float mcrtProgress,
                                        const float mcrtGlobalProgress,
                                        const ValueTimeTracker& sendVtt,
                                        const ValueTimeTracker& recvVtt,
                                        const bool activeBgColFlag,
                                        Overlay::BBox2i& bbox);

    //------------------------------

    Overlay::BBox2i mBBoxTitle;
    Overlay::BBox2i mBBoxElapsedSecFromStart;
};

class LayoutDevel : public LayoutPanel
//
// Telemetry Layout for "devel" (most standard) panel.
//
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using FontShPtr = std::shared_ptr<Font>;
    using OverlayShPtr = std::shared_ptr<Overlay>;

    LayoutDevel(const std::string& name, OverlayShPtr overlay, FontShPtr font)
        : LayoutPanel(name, overlay, font)
    {}

    void drawMain(const DisplayInfo& info) override;

private:    

    void drawGlobalInfo(const DisplayInfo& info);
    void drawDispatchMergeComputation(const DisplayInfo& info);
    void drawGlobalProgressBar(const DisplayInfo& info);
    void drawMcrtComputation(const DisplayInfo& info);

    //------------------------------

    Overlay::BBox2i mBBoxGlobalInfo;
    Overlay::BBox2i mBBoxGlobalProgressBar;
    Overlay::BBox2i mBBoxDispatchMergeComputation;
    Overlay::BBox2i mBBoxMcrtComputation;

    struct BarPos {
        unsigned mY;
        unsigned mXoffset[3] {0,0,0};
        unsigned mXmin[3] {0,0,0};
        unsigned mXmax[3] {0,0,0};
        unsigned mHeight[3] {0,0,0};
        float mFraction[3] {0.0f, 0.0f, 0.0f};
        bool mActiveBgFlag {true};
        bool mExtraBarFlag {false};
        float mFractionExtra {0.0f};
    };
    std::vector<BarPos> mBarPosArray;
};

class LayoutCorePerf : public LayoutPanel
//
// Telemetry Layout for "corePerf" (Core Performance) panel.
//
{
public:
    using FontShPtr = std::shared_ptr<Font>;
    using OverlayShPtr = std::shared_ptr<Overlay>;

    LayoutCorePerf(const std::string& name, OverlayShPtr overlay, FontShPtr font)
        : LayoutPanel(name, overlay, font)
    {}

    void drawMain(const DisplayInfo& info) override;

private:
    struct McrtPos {
        unsigned mMaxY {0}; // Left top character output position
        unsigned mYStep {0};

        unsigned mRowCoreNum {0};
        unsigned mNumOfRows {0}; // row count

        unsigned mTitleWidthChar {0};

        unsigned mCoreWinXMin {0};
        unsigned mCoreWinXMax {0};
        unsigned mCoreWinYMin {0};
        unsigned mCoreWinYMax {0};
        unsigned mSingleCoreGapX {0};
        unsigned mSingleCoreWidth {0};

        bool mActiveBgFlag {true};
    };

    void drawGlobalInfo(const DisplayInfo& info);
    void drawGlobalProgressBar(const DisplayInfo& info);
    void drawMcrtComputation(const DisplayInfo& info);
    
    bool setupCorePerfRowInfo(const GlobalNodeInfo* gNodeinfo, unsigned yMax);
    unsigned calcMinRowCoreNum(const GlobalNodeInfo* gNodeInfo, unsigned yMax) const;
    unsigned calcMinRowMcrtComputation(const GlobalNodeInfo* gNodeInfo) const;

    std::string drawSingleNodeTitle(size_t mcrtTotal,
                                    std::shared_ptr<McrtNodeInfo> node,
                                    unsigned numOfRow) const;
    void drawCorePerfSingleNode(std::shared_ptr<McrtNodeInfo> node, const McrtPos& mcrtPos);

    //------------------------------

    unsigned mComputeRowInfoMcrtTotal {0};
    unsigned mRowCoreNum {0};
    unsigned mMinRowMcrtComputation {0};

    Overlay::BBox2i mBBoxGlobalInfo;
    Overlay::BBox2i mBBoxGlobalProgressBar;
    Overlay::BBox2i mBBoxMcrtComputation;

    std::vector<McrtPos> mMcrtPosArray;
};

class LayoutNetIO : public LayoutPanel
//
// Telemetry Layout for "netIO" (Network I/O) panel.
//
{
public:
    LayoutNetIO(const std::string& name, OverlayShPtr overlay, FontShPtr font)
        : LayoutPanel(name, overlay, font)
    {
        parserConfigure();
    }

    void drawMain(const DisplayInfo& info) override;

private:
    void parserConfigure();

    void setupPanelPosition(const DisplayInfo& info);
    int getMcrtTotal(const GlobalNodeInfo* gNodeInfo) const;

    void drawGlobalInfo(const DisplayInfo& info);
    void drawGlobalProgressBar(const DisplayInfo& info);
    void drawClient(const DisplayInfo& info);
    void drawMerge(const DisplayInfo& info);
    void drawMCRT(const DisplayInfo& info);

    //------------------------------

    int mMcrtTotalOverwrite {0};

    unsigned mGapX {10};
    unsigned mGapY {10};
    unsigned mPanelCountX {3};
    unsigned mPanelCountY {4};
    unsigned mPanelWidth {0};
    unsigned mPanelHeight {0};
    unsigned mPanelTopY {0};
    unsigned mPanelCenterY {0};
    unsigned mPanelMcrtLeftX {0};

    float mBpsGraphMax {0.0f};
    unsigned mBpsRulerYSize {5};

    Overlay::BBox2i mBBoxGlobalInfo;
    Overlay::BBox2i mBBoxGlobalProgressBar;
    Overlay::BBox2i mBBoxClient;
    Overlay::BBox2i mBBoxMerge;
    std::vector<Overlay::BBox2i> mBBoxMcrt;
};

class LayoutFeedback : public LayoutPanel
//
// Telemetry Layout for "feedback" panel. This is experimental for multi-machine adaptive sampling
//
{
public:    
    using FontShPtr = std::shared_ptr<Font>;
    using OverlayShPtr = std::shared_ptr<Overlay>;

    LayoutFeedback(const std::string& name, OverlayShPtr overlay, FontShPtr font)
        : LayoutPanel(name, overlay, font)
    {}

    void drawMain(const DisplayInfo& info) override;

private:
    void drawGlobalInfo(const DisplayInfo& info);
    void drawGlobalProgressBar(const DisplayInfo& info);
    void drawMergeComputation(const DisplayInfo& info);
    void drawMcrtComputation(const DisplayInfo& info);

    Overlay::BBox2i mBBoxGlobalInfo;
    Overlay::BBox2i mBBoxGlobalProgressBar;
    Overlay::BBox2i mBBoxMergeComputation;
    Overlay::BBox2i mBBoxMcrtComputation;
};

class LayoutPathVis : public LayoutPanel
//
// Telemetry Layout for "pathVis" (path visualizer) panel
//
{
public:
    LayoutPathVis(const std::string& name, OverlayShPtr overlay, FontShPtr font,
                  const Display& display)
        : LayoutPanel(name, overlay, font)
        , mDisplay {display}
    {
        parserConfigure();
    }

    void drawMain(const DisplayInfo& info) override;

    std::string show() const;

private:
    void parserConfigure();

    void drawGlobalProgressBar(const DisplayInfo& info);
    void drawVector(const DisplayInfo& info);
    void drawVectorTest0(); // drawVectorTestPatternA() w=1
    void drawVectorTest1(); // drawVectorTestPatternA() w=0~16
    void drawVectorTest2(); // drawVectorTestPatternA() w=0~16, len=change
    void drawVectorTest3(); // clipping version of 1
    void drawVectorTest4(); // clipping version of 2
    void drawVectorTest5(); // circleFill test
    void drawVectorTest6(); // vLine/hLine and boxOutline test

    // Draw spinner test pattern based on the mTestCounter value.
    void drawVectorTestPatternA(const unsigned segmentTotal,
                                const float angleStart, const float angleEnd,
                                const float lenScale,
                                const float widthMax,
                                const bool constLen,
                                const bool constWidth);

    // Draw test pattern for VLine, HLine, and BoxOutline based on the mTestCounter value.
    void drawVectorTestPatternB(const unsigned segmentTotal, const float lineRatio);

    void drawVectorProfile(const std::function<void()>& func);

    void drawPathVisCtrlInfo(const DisplayInfo& info);
    void drawPathVisClientInfo();
    void drawPathVisCurrInfo(const DisplayInfo& info);
    void drawPathVisHotKeyHelp();

    //------------------------------

    bool mTestMode {false};
    unsigned mTestType {0};
    int mTestCounter {0};
    unsigned mProfileLoopMax {0};

    bool mLineDrawOnly {false};
    bool mHotKeyHelp {false};

    const Display& mDisplay;

    Overlay::BBox2i mBBoxGlobalInfo;
    Overlay::BBox2i mBBoxGlobalProgressBar;
    Overlay::BBox2i mBBoxPathVisCtrlInfo;
    Overlay::BBox2i mBBoxPathVisClientInfo;
    Overlay::BBox2i mBBoxPathVisCurrentInfo;
    Overlay::BBox2i mBBoxPathVisHotKeyHelp;
};

} // namespace telemetry
} // namespace mcrt_dataio
