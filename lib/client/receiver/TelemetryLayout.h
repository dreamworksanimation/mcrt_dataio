// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "TelemetryOverlay.h"

#include <mcrt_dataio/engine/mcrt/McrtNodeInfo.h>
#include <mcrt_messages/BaseFrame.h>

namespace mcrt_dataio {

class GlobalNodeInfo;    

namespace telemetry {

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

    Parser& getParser() { return mParser; }

protected:

    void parserConfigure();

    unsigned getFontStepX() {
        return (mOverlay->getFontStepX() == 0) ? mFont->getFontSizePoint() : mOverlay->getFontStepX();
    }

    //
    // draw utility functions
    //
    std::string colFg(const C3& c) const;
    std::string colBg(const C3& c) const;
    std::string colReset() const { return colFg(mCharFg) + colBg(mCharBg); }

    //------------------------------

    std::string strFps(float v) const;
    std::string strPct(float fraction) const;
    std::string strSec(float sec) const;
    std::string strMillisec(float millisec) const;
    std::string strByte(size_t size, size_t minOutStrLen = 8) const;
    std::string strBps(float bps, size_t minOutStrLen = 10) const;
    std::string strBar(unsigned barWidth,
                       unsigned fontStepX,
                       const std::string& title,
                       float fraction,
                       bool usageMode,
                       unsigned* barStartOffsetPixX = nullptr,
                       unsigned* barEndOffsetPixX = nullptr,
                       unsigned* barHeight = nullptr) const;
    std::string strBool(bool flag) const;
    std::string strSimpleHostName(const std::string& hostName) const;
    std::string strFrameStatus(const mcrt::BaseFrame::Status& status,
                               const float renderPrepProgress) const;
    std::string strPassStatus(bool isCoarsePass) const;
    std::string strExecMode(const McrtNodeInfo::ExecMode& execMode) const;

    //------------------------------

    // horizontal box bar
    void drawHBoxBar(unsigned leftX, unsigned leftY,
                     unsigned barStartOffsetPixX, unsigned barEndOffsetPixX,
                     unsigned barHeight,
                     float fraction,
                     const C3& c, unsigned char alpha);

    // horizontal box bar that consists of 2 consecutive sections
    void drawHBoxBar2Sections(unsigned leftX, unsigned leftY,
                              unsigned barStartOffsetPixX, unsigned barEndOffsetPixX,
                              unsigned barHeight,
                              float fractionA,
                              const C3& cA, unsigned char alphaA,
                              float fractionB,
                              const C3& cB, unsigned char alphaB);

    // horizontal bar with title
    void drawHBarWithTitle(unsigned barLeftBottomX,
                           unsigned barLeftBottomY,
                           unsigned barWidth,
                           const std::string& title,
                           float fraction,
                           bool usageMode);

    // borizontal bar that consists of 2 consecutive sections with title
    void drawHBar2SectionsWithTitle(unsigned barLeftBottomX,
                                    unsigned barLeftBottomY,
                                    unsigned barWidth,
                                    const std::string& title,
                                    float fractionA,
                                    float fractionB,
                                    bool usageMode);

    // vertical line
    void drawVLine(unsigned x, unsigned minY, unsigned maxY, const C3& c, unsigned char alpha);

    // vertical bar graph
    void drawVBarGraph(unsigned leftBottomX,
                       unsigned leftBottomY,
                       unsigned rightTopX,
                       unsigned rightTopY,
                       unsigned rulerYSize,
                       const ValueTimeTracker& vtt,
                       const C3& c,
                       unsigned char alpha,
                       float graphTopY, // auto adjust display Y range if this value is 0 or negative
                       float& maxValue,
                       float& currValue);

    // vertical bar graph with title for bps value
    void drawBpsVBarGraphWithTitle(unsigned leftBottomX,
                                   unsigned leftBottomY,
                                   unsigned rightTopX,
                                   unsigned rightTopY,
                                   unsigned rulerYSize,
                                   const ValueTimeTracker& vtt,
                                   const C3& c,
                                   unsigned char alpha,
                                   float graphTopY, // auto adjust display Y range if this val is 0 or negative
                                   const std::string& title);

    //------------------------------

    Overlay::BBox2i setBBox(int minX, int minY, int maxX, int maxY) const {
        return Overlay::BBox2i {scene_rdl2::math::Vec2i {minX, minY}, scene_rdl2::math::Vec2i {maxX, maxY}};
    }

    unsigned calcMaxSimpleMcrtHostNameLen(const GlobalNodeInfo* gNodeInfo) const;

    std::string showC3(const C3& c) const;

    unsigned char getArgC0255(Arg& arg) const;
    C3 getArgC3(Arg& arg) const;

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
    unsigned mStepPixY {0};

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
    void subPanelMessage(unsigned x,
                         unsigned y,
                         const std::string& str,
                         Overlay::BBox2i& bbox);

    // Global Info
    void subPanelGlobalInfo(unsigned x,
                            unsigned y,
                            const DisplayInfo& info,
                            Overlay::BBox2i& bbox);

    // Global progress bar
    void subPanelGlobalProgressBar(unsigned barLeftBottomX,
                                   unsigned barLeftBottomY,
                                   unsigned barWidth,
                                   const DisplayInfo& info,
                                   Overlay::BBox2i& bboxGlobalProgressBar);

    // NetIO, Cpu/Mem monitor, RenderPrep/MCRT progress
    void subPanelNetIOCpuMemAndProgress(unsigned leftBottomX,
                                        unsigned leftBottomY,
                                        unsigned rightTopX,
                                        unsigned rightTopY,
                                        float graphTopY, // auto adjust Y if this value is 0 or negative
                                        unsigned rulerYSize,
                                        const std::string& title,
                                        int cpuTotal,
                                        float cpuFraction,
                                        size_t memTotal, // byte
                                        float memFraction,
                                        float renderPrepFraction,
                                        float mcrtProgress,
                                        float mcrtGlobalProgress,
                                        const ValueTimeTracker& sendVtt,
                                        const ValueTimeTracker& recvVtt,
                                        bool activeBgColFlag,
                                        Overlay::BBox2i& bbox);

    //------------------------------

    Overlay::BBox2i mBBoxTitle;
    Overlay::BBox2i mBBoxElapsedSecFromStart;
};

class LayoutDevel : public LayoutPanel
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using FontShPtr = std::shared_ptr<Font>;
    using OverlayShPtr = std::shared_ptr<Overlay>;
    using Parser = scene_rdl2::grid_util::Parser;

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

} // namespace telemetry
} // namespace mcrt_dataio
