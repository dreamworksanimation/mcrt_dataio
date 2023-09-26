// Copyright 2023 DreamWorks Animation LLC
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
    using Parser = scene_rdl2::grid_util::Parser;

    LayoutBase(std::shared_ptr<Overlay> overlay, std::shared_ptr<Font> font);
    virtual ~LayoutBase() {}

    virtual const char* getName() const = 0;

    virtual void drawMain(const DisplayInfo& info) = 0;

    Parser& getParser() { return mParser; }

protected:

    void parserConfigure();

    //
    // draw utility functions
    //
    std::string colFg(const C3& c) const;
    std::string colBg(const C3& c) const;
    std::string colReset() const { return colFg(mCharFg) + colBg(mCharBg); }
    std::string strFps(float v) const;
    std::string strPct(float fraction) const;
    std::string strSec(float sec) const;
    std::string strMillisec(float millisec) const;
    std::string strByte(size_t size) const;
    std::string strBps(float bps) const;
    std::string strBar(unsigned barWidth,
                       unsigned fontStepX,
                       const std::string& title,
                       float fraction,
                       bool usageMode,
                       unsigned* barStartOffsetPixX = nullptr,
                       unsigned* barEndOffsetPixX = nullptr,
                       unsigned* barHeight = nullptr) const;
    std::string strBool(bool flag) const;
    std::string strFrameStatus(const mcrt::BaseFrame::Status& status,
                               const float renderPrepProgress) const;
    std::string strPassStatus(bool isCoarsePass) const;
    std::string strExecMode(const McrtNodeInfo::ExecMode& execMode) const;

    void drawBoxBar(unsigned leftX, unsigned leftY,
                    unsigned barStartOffsetPixX, unsigned barEndOffsetPixX,
                    unsigned barHeight,
                    float fraction,
                    const C3& c, unsigned char alpha);

    std::string showC3(const C3& c) const;

    unsigned char getArgC0255(Arg& arg) const;
    C3 getArgC3(Arg& arg) const;

    //------------------------------

    C3 mCharFg {255, 255, 255};
    C3 mCharBg {0, 0, 0};

    C3 mPanelBg {32, 32, 32};    
    float mPanelBgAlpha {200};

    std::shared_ptr<Overlay> mOverlay;
    std::shared_ptr<Font> mFont;

    unsigned mMaxYLines {0};
    unsigned mOffsetBottomPixY {0};
    unsigned mStepPixY {0};

    std::string mError;

    Parser mParser;
};

class LayoutUtil : public LayoutBase
{
public:
    LayoutUtil(std::shared_ptr<Overlay> overlay, std::shared_ptr<Font> font)
        : LayoutBase(overlay, font)
    {}

protected:
    void drawUtilGlobalInfo(const std::string& msg,
                            unsigned x,
                            unsigned y,
                            Overlay::BBox2i& bboxGlobalInfo);
    void drawUtilGlobalProgressBar(unsigned barLeftBottomX,
                                   unsigned barLeftBottomY,
                                   unsigned barWidth,
                                   const DisplayInfo& info,
                                   Overlay::BBox2i& bboxGlobalProgressBar);
};

class LayoutDevel : public LayoutUtil
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;

    LayoutDevel(std::shared_ptr<Overlay> overlay, std::shared_ptr<Font> font)
        : LayoutUtil(overlay, font)
    {
        parserConfigure();
    }

    const char* getName() const override { return "devel"; }

    void drawMain(const DisplayInfo& info) override;

private:    

    void drawGlobalInfo(const DisplayInfo& info);
    void drawDispatchMergeComputation(const DisplayInfo& info);
    void drawGlobalProgressBar(const DisplayInfo& info);
    void drawMcrtComputation(const DisplayInfo& info);

    void parserConfigure();

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
    };
    std::vector<BarPos> mBarPosArray;
};

class LayoutCorePerf : public LayoutUtil
{
public:
    LayoutCorePerf(std::shared_ptr<Overlay> overlay, std::shared_ptr<Font> font)
        : LayoutUtil(overlay, font)
    {}

    const char* getName() const override { return "corePerf"; }

    void drawMain(const DisplayInfo& info) override;

private:
    struct McrtPos {
        unsigned mY {0}; // Left top character output position
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

} // namespace telemetry
} // namespace mcrt_dataio
