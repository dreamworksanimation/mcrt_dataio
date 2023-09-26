// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "TelemetryDisplay.h"

#include <scene_rdl2/common/except/exceptions.h>
#include <scene_rdl2/common/grid_util/RenderPrepStats.h>
#include <scene_rdl2/render/util/GetEnv.h>
#include <mcrt_dataio/engine/merger/GlobalNodeInfo.h>

#include <tbb/parallel_for.h>

#include <iostream>

namespace mcrt_dataio {
namespace telemetry {

std::string
DisplayInfo::show() const
{
    std::ostringstream ostr;
    ostr << "DisplayInfo {\n"
         << "  mOverlayWidth:" << mOverlayWidth << '\n'
         << "  mOverlayHeight:" << mOverlayHeight << '\n'
         << "  mImageWidth:" << mImageWidth << '\n'
         << "  mImageHeight:" << mImageHeight << '\n'
         << "  mViewId:" << mViewId << '\n'
         << "  mFrameId:" << mFrameId << '\n'
         << "  mStatus:" << static_cast<int>(mStatus) << '\n'
         << "  mRenderPrepProgress:" << mRenderPrepProgress << '\n'
         << "  mProgress:" << mProgress << '\n'
         << "  mFbActivityCounter:" << mFbActivityCounter << '\n'
         << "  mDecodeProgressiveFrameCounter:" << mDecodeProgressiveFrameCounter << '\n'
         << "  mIsCoarsePass:" << scene_rdl2::str_util::boolStr(mIsCoarsePass) << '\n'
         << "  mCurrentLatencySec:" << mCurrentLatencySec << '\n'
         << "  mReceiveImageDataFps:" << mReceiveImageDataFps << '\n'
         << "  mGlobalNodeInfo:0x" << std::hex << reinterpret_cast<uintptr_t>(mGlobalNodeInfo) << std::dec << '\n'
         << "}";
    return ostr.str();
}

//------------------------------------------------------------------------------------------

void
Display::bakeOverlayRgb888(std::vector<unsigned char>& rgbFrame,
                           const bool top2bottomFlag,
                           const DisplayInfo& info,
                           bool bakeWithPrevArchive)
{
    if (!mActive) return; // early exit

    if (mTimingProfile) mRecTime.start();

    unsigned overlayWidth = (mOverwriteWidth > 0) ? mOverwriteWidth : info.mOverlayWidth;
    unsigned overlayHeight = (mOverwriteHeight > 0) ? mOverwriteHeight : info.mOverlayHeight;
    if (!mOverlay) {
        mOverlay = std::make_shared<Overlay>(overlayWidth, overlayHeight);
    } else {
        mOverlay->resize(overlayWidth, overlayHeight);
    }

    if (mTestMode) {
        testBakeOverlayRgb888(info, rgbFrame, top2bottomFlag, bakeWithPrevArchive);
    } else {
        if (!stdBakeOverlayRgb888(info, rgbFrame, top2bottomFlag, bakeWithPrevArchive)) {
            mActive = false;    // disable telemetry display
        }
    }
}

std::string
Display::show() const
{
    using scene_rdl2::str_util::addIndent;
    using scene_rdl2::str_util::boolStr;

    std::ostringstream ostr;
    ostr << "telemetry::Display {\n"
         << "  mActive:" << boolStr(mActive) << '\n'
         << "  mDoParallel:" << boolStr(mDoParallel) << '\n'
         << "  mTimingProfile:" << boolStr(mTimingProfile) << '\n'
         << "  mTestMode:" << boolStr(mTestMode) << '\n'
         << "  mOverwriteWidth:" << mOverwriteWidth << '\n'
         << "  mOverwriteHeight:" << mOverwriteHeight << '\n'
         << "  mError:>" << mError << "<\n"
         << addIndent(showTestInfo()) << '\n'
         << "}";
    return ostr.str();
}

//------------------------------------------------------------------------------------------

void
Display::setupLayout()
{
    auto setNewLayout = [&](std::shared_ptr<LayoutBase> ptr) {
        mLayoutMap[ptr->getName()] = ptr;
    };

    std::shared_ptr<LayoutBase> defaultLayout = std::make_shared<LayoutDevel>(mOverlay, mFont);
    setNewLayout(defaultLayout);
    mCurrLayout = mLayoutMap[defaultLayout->getName()];

    // adding extra layout here
    setNewLayout(std::make_shared<LayoutCorePerf>(mOverlay, mFont));
}

bool
Display::setupFont()
{
    if (!mFont) {
        constexpr const char* ttfFileNameEnvKey = "TELEMETRY_OVERLAY_FONTTTF";

        // TTF file should be monospace font otherwise internal layout computation does not work properly.
        std::string ttfFilename = scene_rdl2::util::getenv<std::string>(ttfFileNameEnvKey);
        if (ttfFilename.empty()) {
            std::cerr << ">> Telemetry overlay font is empty -> disable telemetry overlay\n";
            return false;
        }

        std::cerr << ">> Telemetry overlay ttfFile:" << ttfFilename << '\n';
        try {
            mFont = std::make_shared<Font>(ttfFilename, calcFontSize());
        }
        catch (scene_rdl2::except::RuntimeError& e) {
            std::cerr << ">> TelemetryDisplay.cc ERROR : Telemetry overlay setup font failed."
                      << " -> disable telemetry overlay. RuntimeError:" << e.what() << '\n';
            mFont.reset(); // Just in case
            return false;
        }
    }
    return true;
}

bool
Display::setupTestFont()
{
    if (!mTestFont ||
        mTestFontTTFFileName != mTestFont->getFontTTFFileName() ||
        mTestFontPoint != mTestFont->getFontSizePoint()) {
        try {
            mTestFont.reset(new Font(mTestFontTTFFileName, mTestFontPoint));
        }
        catch (scene_rdl2::except::RuntimeError& e) {
            std::cerr << ">> TelemetryDisplay.cc ERROR : setupTestFont() failed."
                      << " RuntimeError:" << e.what() << '\n';
            mTestFont.reset(nullptr);
            return false;
        }
    }
    return true;
}

unsigned
Display::calcFontSize() const
{
    unsigned winHeight = mOverlay->getHeight();

    // This is a hard limitation at this moment. We can only have totalLines display areas.
    // All information should be within these limits.
    constexpr unsigned totalLines = 72;

    unsigned fontSizePoint = winHeight / totalLines;
    return fontSizePoint;
}

void
Display::testBakeOverlayRgb888(const DisplayInfo& info,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottomFlag,
                               bool bakeWithPrevArchive)
{
    if (!setupTestFont()) return;
    if (!mTestFont) return;

    mOverlay->clear({static_cast<unsigned char>(mTestBgCol[0]),
                     static_cast<unsigned char>(mTestBgCol[1]),
                     static_cast<unsigned char>(mTestBgCol[2])},
                    static_cast<unsigned char>(mTestBgCol[3]),
                    mDoParallel);

    mOverlay->drawStrClear();
    if (!mOverlay->drawStr(*mTestFont, mTestStrX, mTestStrY,
                           mTestMsg,
                           {static_cast<unsigned char>(mTestStrCol[0]),
                            static_cast<unsigned char>(mTestStrCol[1]),
                            static_cast<unsigned char>(mTestStrCol[2])},
                           mError)) {
        std::cerr << ">> TelemetryDisplay.cc testBakeOverlayRgb888 mOverlay->drawStr() failed. " << mError << '\n';
        return;
    }
    mOverlay->drawStrFlush(mDoParallel);

    finalizeOverlayRgb888(info, rgbFrame, top2bottomFlag, mTestHAlign, mTestVAlign, bakeWithPrevArchive);
}

bool
Display::stdBakeOverlayRgb888(const DisplayInfo& info,
                              std::vector<unsigned char>& rgbFrame,
                              const bool top2bottomFlag,
                              bool bakeWithPrevArchive)
{
    //
    // Font setup
    //
    if (!mFont) {
        if (!setupFont()) {
            return false;;
        }
    }

    //
    // draw overlay info
    //
    float sectionStartTime = 0.0f;
    if (mTimingProfile) sectionStartTime = mRecTime.end();
    {
        mOverlay->clear({static_cast<unsigned char>(mTestBgCol[0]),
                         static_cast<unsigned char>(mTestBgCol[1]),
                         static_cast<unsigned char>(mTestBgCol[2])},
                        static_cast<unsigned char>(mTestBgCol[3]),
                        mDoParallel);
    }
    if (mTimingProfile) {
        mOverlayClear.set(mRecTime.end() - sectionStartTime);
        sectionStartTime = mRecTime.end();
    }
    {
        mOverlay->drawBoxClear();
        mOverlay->drawStrClear();
        drawOverlay(info);
        mOverlay->drawBoxFlush(mDoParallel);
        mOverlay->drawStrFlush(mDoParallel);
    }
    if (mTimingProfile) mDrawStrTime.set(mRecTime.end() - sectionStartTime);

    //
    // bake overlay info into output buffer
    //
    finalizeOverlayRgb888(info, rgbFrame, top2bottomFlag, mTestHAlign, mTestVAlign, bakeWithPrevArchive);

    return true;
}

void
Display::drawOverlay(const DisplayInfo& info)
{
    if (!mCurrLayout) {
        setupLayout();
    }
    if (mCurrLayout) mCurrLayout->drawMain(info);
}

void
Display::finalizeOverlayRgb888(const DisplayInfo& info,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottomFlag,
                               Align hAlign,
                               Align vAlign,
                               bool bakeWithPrevArchive)
{
    if (!mOverlay) return;

    unsigned rgbFrameWidth, rgbFrameHeight;
    if (!info.mImageWidth && !info.mImageHeight) {
        rgbFrameWidth = info.mOverlayWidth;
        rgbFrameHeight = info.mOverlayHeight;
    } else {
        rgbFrameWidth = info.mImageWidth;
        rgbFrameHeight = info.mImageHeight;
    }

    if (bakeWithPrevArchive) {
        float sectionStartTime = 0.0f;
        if (mTimingProfile) sectionStartTime = mRecTime.end();

        copyArchive(rgbFrame);

        if (mTimingProfile) mCopyArchiveTime.set(mRecTime.end() - sectionStartTime);
    }

    float sectionStartTime = 0.0;
    if (mTimingProfile) sectionStartTime = mRecTime.end();

    mOverlay->finalizeRgb888(rgbFrame, rgbFrameWidth, rgbFrameHeight, top2bottomFlag, hAlign, vAlign,
                             &mBgArchive,
                             mDoParallel);

    if (mTimingProfile) mFinalizeRgb888Time.set(mRecTime.end() - sectionStartTime);
}

void
Display::copyArchive(std::vector<unsigned char>& rgbFrame) const
{
    //
    // This solution might not work with resolution change action during sessions. We need more deep
    // consideration of how to control buffer resolution change.
    //    
    if (!mDoParallel) {
        if (mBgArchive.empty() || rgbFrame.size() != mBgArchive.size()) {
            for (size_t i = 0; i < rgbFrame.size(); ++i) {
                rgbFrame[i] = 0x0;
            }
        } else {
            rgbFrame = mBgArchive;
        }
    } else {
        constexpr size_t grainSize = 128;
        if (mBgArchive.empty() || rgbFrame.size() != mBgArchive.size()) {
            tbb::blocked_range<size_t> range(0, rgbFrame.size(), grainSize);
            tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                    for (size_t i = range.begin(); i < range.end(); ++i) {
                        rgbFrame[i] = 0x0;
                    }
                });
        } else {
            tbb::blocked_range<size_t> range(0, rgbFrame.size(), grainSize);
            tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                    for (size_t i = range.begin(); i < range.end(); ++i) {
                        rgbFrame[i] = mBgArchive[i];
                    }
                });
        }
    }
}

void
Display::clearBgArchive()
//
// non performance oriented function
//
{
    for (size_t i = 0; i < mBgArchive.size(); ++i) {
        mBgArchive[i] = 0x0;
    }
}

void
Display::switchLayoutToNextOnTheList()
{
    if (!mCurrLayout) return; // current layout is empty
    if (mLayoutMap.empty()) return; // empty

    auto itr = mLayoutMap.find(mCurrLayout->getName());
    if (itr == mLayoutMap.end()) return; // can not find item

    ++itr;
    if (itr == mLayoutMap.end()) {
        itr = mLayoutMap.begin();
    }
    mCurrLayout = itr->second;
}

void
Display::switchLayoutByName(const std::string& name,
                            const std::function<void(const std::string& msg)> msgOut)
{
    auto itr = mLayoutMap.find(name);
    if (itr == mLayoutMap.end()) {
        // can not find
        if (msgOut) {
            std::ostringstream ostr;
            ostr << "Can not find layout. name:" << name;
            msgOut(ostr.str());
        }
    } else {
        mCurrLayout = itr->second;
    }
}

void
Display::parserConfigure()
{
    using scene_rdl2::str_util::boolStr;

    auto setFlag = [&](Arg& arg, bool& flag, const std::string& msgFlagName) {
        if (arg() == "show") arg++;
        else flag = (arg++).as<bool>(0);
        return arg.fmtMsg("%s %s\n", msgFlagName.c_str(), boolStr(flag).c_str());
    };
    auto setInt = [&](Arg& arg, int& v, const std::string& msgValName) {
        if (arg() == "show") arg++;
        else v = (arg++).as<int>(0);
        return arg.fmtMsg("%s %d\n", msgValName.c_str(), v);
    };
    auto setAlign = [&](Arg& arg, Align& align, const std::string& msgFlagName) {
        if (arg() == "show") arg++;
        else {
            std::string key = (arg++)();
                 if (key == "small" ) { align = Align::SMALL; }
            else if (key == "middle") { align = Align::MIDDLE; }
            else if (key == "big"   ) { align = Align::BIG; }
            // else align is unchanged
        }
        return arg.fmtMsg("%s:%s\n", msgFlagName.c_str(), showAlign(align).c_str());
    };

    mParser.description("telemetry display command");

    mParser.opt("active", "<on|off|show>", "set or show telemetry display mode",
                [&](Arg& arg) { return setFlag(arg, mActive, "telemetryDisplayActive"); });
    mParser.opt("parallel", "<on|off|show>", "set parallel execution condition",
                [&](Arg& arg) { return setFlag(arg, mDoParallel, "doParallel"); });
    mParser.opt("overwriteSize", "<width> <height>", "set telemetry overwrite overlay reso. ZERO disable overwrite",
                [&](Arg& arg) {
                    mOverwriteWidth = (arg++).as<unsigned>(0);
                    mOverwriteHeight = (arg++).as<unsigned>(0);
                    return arg.fmtMsg("overwiteSize %dx%d\n", mOverwriteWidth, mOverwriteHeight);
                });
    mParser.opt("overlay", "...command...", "overlay command",
                [&](Arg& arg) {
                    if (!mOverlay) return arg.msg("mOverlay is empty\n");
                    return mOverlay->getParser().main(arg.childArg()); 
                });
    mParser.opt("layout", "...command...", "current layout command",
                [&](Arg& arg) {
                    if (!mCurrLayout) return arg.msg("current layout is empty\n");
                    return mCurrLayout->getParser().main(arg.childArg());
                });
    mParser.opt("layoutNameList", "", "list of all available layout name",
                [&](Arg& arg) { return arg.msg(showLayoutNameList() + '\n'); });
    mParser.opt("layoutNext", "", "switch layout to the next",
                [&](Arg& arg) {
                    switchLayoutToNextOnTheList();
                    return arg.msg(showCurrentLayoutName() + '\n');
                });
    mParser.opt("layoutName", "<name|show>", "set layout by name",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else {
                        switchLayoutByName((arg++)(), [&](const std::string& msg) { arg.msg(msg + '\n'); });
                    }
                    return arg.msg(showCurrentLayoutName() + '\n');
                });
    mParser.opt("testMode", "<on|off|show>", "set testmode",
                [&](Arg& arg) { return setFlag(arg, mTestMode, "telemetryTestMode"); });
    mParser.opt("testMsg", "<x> <y> <string> <r0255> <g0255> <b0255>", "set testmode info",
                [&](Arg& arg) {
                    mTestStrX = (arg++).as<unsigned>(0);
                    mTestStrY = (arg++).as<unsigned>(0);
                    mTestMsg = (arg++)();
                    mTestStrCol[0] = (arg++).as<unsigned>(0);
                    mTestStrCol[1] = (arg++).as<unsigned>(0);
                    mTestStrCol[2] = (arg++).as<unsigned>(0);
                    return arg.msg(showTestInfo() + '\n');
                });
    mParser.opt("testBg", "<r0255> <g0255> <b0255> <a0255>", "set test overlay background color and alpha",
                [&](Arg& arg) {
                    mTestBgCol[0] = (arg++).as<unsigned>(0);
                    mTestBgCol[1] = (arg++).as<unsigned>(0);
                    mTestBgCol[2] = (arg++).as<unsigned>(0);
                    mTestBgCol[3] = (arg++).as<unsigned>(0);
                    return arg.msg(showTestInfo() + '\n');
                });
    mParser.opt("testHAlign", "<small|middle|big|show>", "set hAlign",
                [&](Arg& arg) { return setAlign(arg, mTestHAlign, "mTestHAlign"); });
    mParser.opt("testVAlign", "<small|middle|big|show>", "set vAlign",
                [&](Arg& arg) { return setAlign(arg, mTestVAlign, "mTestVAlign"); });
    mParser.opt("testFont", "<TTFfileName>", "set testFont TTF filename",
                [&](Arg& arg) { mTestFontTTFFileName = (arg++)(); return arg.msg(showTestInfo() + '\n'); });
    mParser.opt("testFontSize", "<point|show>", "set testFont size",
                [&](Arg& arg) { return setInt(arg, mTestFontPoint, "testFontSize"); });
    mParser.opt("show", "", "show internal parameters",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
    mParser.opt("timingProfile", "<on|off|show>", "set timingProfile mode",
                [&](Arg& arg) { return setFlag(arg, mTimingProfile, "timingProfile"); });
    mParser.opt("timingProfileResult", "", "show timing profile result",
                [&](Arg& arg) { return arg.msg(showTimingProfile() + '\n'); });
    mParser.opt("timingProfileReset", "", "reset timing profile",
                [&](Arg& arg) { resetTimingProfile(); return arg.msg("reset-timing-profile\n"); });
    mParser.opt("clearBgArchive", "", "clear bgArchive data",
                [&](Arg& arg) { clearBgArchive(); return arg.msg("clearBgArchive\n"); });
}

std::string
Display::showTestInfo() const
{
    std::ostringstream ostr;
    ostr << "TestInfo {\n"
         << "  mTestStrX:" << mTestStrX << '\n'
         << "  mTestStrY:" << mTestStrY << '\n'
         << "  mTestStrCol:" << mTestStrCol[0] << ' ' << mTestStrCol[1] << ' ' << mTestStrCol[2] << '\n'
         << "  mTestBgCol:"
         << mTestBgCol[0] << ' ' << mTestBgCol[1] << ' ' << mTestBgCol[2] << ' ' << mTestBgCol[3] << '\n'
         << "  mTestMsg:" << mTestMsg << '\n'
         << "  mTestFontTTFFileName:" << mTestFontTTFFileName << '\n'
         << "  mTestFontPoint:" << mTestFontPoint << '\n'
         << "  mTestHAlign:" << showAlign(mTestHAlign) << '\n'
         << "  mTestVAlign:" << showAlign(mTestVAlign) << '\n'
         << "}";
    return ostr.str();
}

std::string
Display::showAlign(Align align) const
{
    switch (align) {
    case Align::SMALL : return "small";
    case Align::MIDDLE : return "middle";
    case Align::BIG : return "big";
    }
}

std::string
Display::showLayoutNameList() const
{
    auto maxNameLen = [&]() {
        size_t maxLen = 0;
        for (const auto& itr : mLayoutMap) {
            if (itr.first.size() > maxLen) maxLen = itr.first.size();
        }
        return maxLen;
    };

    int w = maxNameLen();

    std::ostringstream ostr;
    ostr << "layout (total:" << mLayoutMap.size() << ") {\n";
    for (const auto& itr : mLayoutMap) {
        ostr << "  " << std::setw(w) << itr.first
             << ((itr.second.get() == mCurrLayout.get()) ? " <== current\n" : "\n");
    }
    ostr << "}";
    return ostr.str();
}

std::string
Display::showCurrentLayoutName() const
{
    return (mCurrLayout) ? mCurrLayout->getName() : "current layout is empty";
}

std::string
Display::showTimingProfile() const
{
    auto showPct = [](float fraction) {
        float pct = fraction * 100.0f;
        std::ostringstream ostr;
        ostr << std::setw(5) << std::fixed << std::setprecision(2) << pct << '%';
        return ostr.str();
    };

    using scene_rdl2::str_util::secStr;

    float overlayClear = mOverlayClear.getAvg();
    float drawStr = mDrawStrTime.getAvg();
    float copyArchive = mCopyArchiveTime.getAvg();
    float finalizeRgb888 = mFinalizeRgb888Time.getAvg();
    float all = overlayClear + drawStr + copyArchive + finalizeRgb888;

    float overlayClearFraction = overlayClear / all;
    float drawStrFraction = drawStr / all;
    float copyArchiveFraction = copyArchive / all;
    float finalizeRgb888Fraction = finalizeRgb888 / all;

    std::ostringstream ostr;
    ostr << "timingProfile {\n"
         << "    overlayClear:" << secStr(overlayClear) << " (" << showPct(overlayClearFraction) << ")\n"
         << "         DrawStr:" << secStr(drawStr) << " (" << showPct(drawStrFraction) << ")\n"
         << "     CopyArchive:" << secStr(copyArchive) << " (" << showPct(copyArchiveFraction) << ")\n"
         << "  FinalizeRgb888:" << secStr(finalizeRgb888) << " (" << showPct(finalizeRgb888Fraction) << ")\n"
         << "           Total:" << secStr(all) << '\n'
         << "}";
    return ostr.str();
}

void
Display::resetTimingProfile()
{
    mOverlayClear.reset();
    mDrawStrTime.reset();
    mCopyArchiveTime.reset();
    mFinalizeRgb888Time.reset();
}

} // namespace telemetry
} // namespace mcrt_dataio
