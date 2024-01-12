// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryLayout.h"
#include "TelemetryOverlay.h"

#include <mcrt_dataio/engine/merger/GlobalNodeInfo.h>
#include <mcrt_dataio/share/util/ValueTimeTracker.h>
#include <scene_rdl2/common/math/Math.h>

#include <algorithm> // clamp
#include <iomanip>
#include <sstream>

namespace mcrt_dataio {
namespace telemetry {

LayoutBase::LayoutBase(const std::string& name, OverlayShPtr overlay, FontShPtr font)
    : mName(name)
    , mOverlay(overlay)
    , mFont(font)
{
    mMaxYLines = mOverlay->getMaxYLines(*mFont, mOffsetBottomPixY, mStepPixY);

    parserConfigure();
}

//------------------------------------------------------------------------------------------

std::string
LayoutBase::colFg(const C3& c) const
{
    std::ostringstream ostr;
    ostr << "\e[38;2;"
         << static_cast<int>(c.mR) << ';' << static_cast<int>(c.mG) << ';' << static_cast<int>(c.mB) << 'm';
    return ostr.str();
}

std::string
LayoutBase::colBg(const C3& c) const
{
    std::ostringstream ostr;
    ostr << "\e[48;2;"
         << static_cast<int>(c.mR) << ';' << static_cast<int>(c.mG) << ';' << static_cast<int>(c.mB) << 'm';
    return ostr.str();
}

std::string
LayoutBase::strFps(float v) const
{
    C3 fgC {0,255,255};
    C3 bgC {0,0,0};

    std::ostringstream ostr;
    ostr << colFg(fgC) << colBg(bgC)
         << std::setw(5) << std::fixed << std::setprecision(2) << v << colReset() << "fps";
    return ostr.str();
}

std::string
LayoutBase::strPct(float fraction) const
{
    C3 bgC {0,0,0};

    std::ostringstream ostr;
    if (fraction >= 1.0f) ostr << colFg({255,255,0});
    else                  ostr << colFg({0,255,255});
    ostr << colBg(bgC)
         << std::setw(6) << std::fixed << std::setprecision(2) << (fraction * 100.0f) << colReset() << "%";
    return ostr.str();
}

std::string
LayoutBase::strSec(float sec) const
{
    float roundedSec;
    std::ostringstream ostr;

    C3 fgC {0,255,255};
    C3 bgC {0,0,0};

    // In order to displaying 1.000 sec instead of 1000.00 ms, we use rounded logic
    roundedSec = std::round(sec * 100000.0f) / 100000.0f;
    if (roundedSec < 1.0f) {
        float ms = roundedSec * 1000.0f;
        ostr << colFg(fgC) << colBg(bgC)
             << std::setw(6) << std::fixed << std::setprecision(2) << ms << colReset() << "ms";
        return ostr.str();
    }

    // Without using a rounded sec value, we get 1 min 60.000 sec when the original sec is 119.9996f
    // With using a rounded sec value, the result would be 2 min 0.000 sec
    roundedSec = std::round(sec * 1000.0f) / 1000.0f;
    if (roundedSec < 60.0f) {
        ostr << colFg(fgC) << colBg(bgC)
             << std::setw(6) << std::fixed << std::setprecision(3) << roundedSec << colReset() << "s";
    } else {
        int m = static_cast<int>(roundedSec / 60.0f);
        float s = roundedSec - static_cast<float>(m) * 60.0f;
        ostr << colFg(fgC) << colBg(bgC) << m << colReset() << "m "
             << colFg(fgC) << colBg(bgC)
             << std::setw(6) << std::fixed << std::setprecision(3) << s << colReset() << "s";
    }
    return ostr.str();
}

std::string
LayoutBase::strMillisec(float millisec) const
{
    return strSec(millisec / 1000.0f);
}

std::string
LayoutBase::strByte(size_t numByte, size_t minOutStrLen) const
//
// minOutStrLen : minimum output string length
//
// This API creates a string of at least minOutStrLen size.
// If the output string is less than minOutStrLen, space padding is added at the front of the string.
// If the output string is more than minOutStrLen, just return the string.
// All the functionality of auto padding logic is canceled when minOutStrLen is 0.
// And just returns a string without any padding.
//
{
    C3 fgC {0,255,255};
    C3 bgC {0,0,0};

    std::ostringstream ostr;
    if (numByte < (size_t)1024) {
        ostr << colFg(fgC) << colBg(bgC) << numByte << colReset() << "B";
    } else if (numByte < (size_t)1024 * (size_t)1024) {
        float f = (float)numByte * (1.0f / 1024.0f);
        ostr << colFg(fgC) << colBg(bgC)
             << std::setw(3) << std::fixed << std::setprecision(2) << f << colReset() << "KB";
    } else if (numByte < (size_t)1024 * (size_t)1024 * (size_t)1024) {
        float f = (float)numByte * (1.0f / (1024.0f * 1024.0f));
        ostr << colFg(fgC) << colBg(bgC)
             << std::setw(3) << std::fixed << std::setprecision(2) << f << colReset() << "MB";
    } else {
        float f = (float)numByte * (1.0f / (1024.0f * 1024.0f * 1024.0f));
        ostr << colFg(fgC) << colBg(bgC)
             << std::setw(3) << std::fixed << std::setprecision(2) << f << colReset() << "GB";
    }

    if (minOutStrLen == 0) {
        return ostr.str();
    }

    std::string msg = ostr.str();
    size_t msgLen = Overlay::msgDisplayLen(msg);
    if (msgLen >= minOutStrLen) {
        return msg; // clear minimum output string length. just return string without padding
    }

    // The output string is too short. Adding padding
    ostr.str("");
    ostr << std::setw(minOutStrLen - msgLen + msg.size()) << msg;
    return ostr.str();
}

std::string
LayoutBase::strBps(float bps, size_t minOutStrLen) const
{
    size_t byte = static_cast<size_t>(bps);
    return strByte(byte, (minOutStrLen > 2) ? minOutStrLen - 2 : 0) + "/s";
}

std::string
LayoutBase::strBar(unsigned barWidth,
                   unsigned fontStepX,
                   const std::string& inTitle,
                   float fraction,
                   bool usageMode,
                   unsigned* barStartOffsetPixX,
                   unsigned* barEndOffsetPixX,
                   unsigned* barHeight) const
{
    auto resizeBar = [](std::string& str, size_t size, char c) {
        if (size == 0) {
            str.clear();
        } else {
            str.resize(size);
            for (size_t i = 0; i < size; ++i) str[i] = c;
        }
    };

    std::string title = inTitle;
    unsigned titleDisplaySize = Overlay::msgDisplayLen(title);

    int barSize = static_cast<int>(barWidth / fontStepX) - (static_cast<int>(titleDisplaySize) + 3);
    if (barSize <= 0) return "";

    std::string barL, barR;
    if (fraction < 1.0f) {
        size_t countL = static_cast<size_t>(static_cast<float>(barSize) * fraction);
        size_t countR = barSize - countL;
        resizeBar(barL, countL, '=');
        resizeBar(barR, countR, ' ');
        if (!usageMode && !barL.empty()) barL[barL.size() - 1] = '>';
    } else {
        if (usageMode) {
            resizeBar(barL, barSize, '*');
        } else {            
            resizeBar(barL, barSize, ' ');
            std::string msg("-- completed --");
            if (msg.size() < barSize) {
                size_t offsetX = (barSize - msg.size()) / 2;
                for (size_t i = 0; i < msg.size(); ++i) {
                    barL[offsetX + i] = msg[i];
                }
            }
        }
    }

    C3 titleC3 {255,255,255};
    C3 barC3 {255, 255, 0};
    C3 black {0, 0, 0};
        
    std::ostringstream ostr;
    ostr << colFg(titleC3) << title << colReset() << ":[";
    if (barL.size() > 0) {
        if (usageMode) {
            ostr << ((fraction > 0.9) ? colFg({255,0,0}) : colFg(barC3)) << colBg(black) << barL;
        } else {
            ostr << colFg(barC3) << colBg(black) << barL;
        }
    }
    if (barR.size() > 0) {
        ostr << colFg(black) << colBg(black) << barR;
    }
    ostr << colReset() << "]";

    if (barStartOffsetPixX && barEndOffsetPixX && barHeight) {
        unsigned barCharStart = titleDisplaySize + 2;
        unsigned barCharEnd = barCharStart + barSize;
        *barStartOffsetPixX = barCharStart * fontStepX;
        *barEndOffsetPixX = barCharEnd * fontStepX - 1;
        *barHeight = mStepPixY;
    }

    return ostr.str();
}

std::string
LayoutBase::strBool(bool flag) const
{
    C3 bgC {0,0,0};

    std::ostringstream ostr;
    if (flag) ostr << colFg({0,0,255}) << colBg(bgC) << "True " << colReset();
    else      ostr << colFg({255,0,0}) << colBg(bgC) << "False" << colReset();
    return ostr.str();
}

std::string
LayoutBase::strSimpleHostName(const std::string& hostName) const
{
    size_t p = hostName.find('.');
    if (p == std::string::npos) return hostName;
    return hostName.substr(0, p);
}

std::string
LayoutBase::strFrameStatus(const mcrt::BaseFrame::Status& status, const float renderPrepProgress) const
{
    std::ostringstream ostr;
    switch (status) {
    case mcrt::BaseFrame::Status::STARTED :
        ostr << colFg({0,0,255}) << "STARTED    " << colReset();
        break;
    case mcrt::BaseFrame::Status::RENDERING :
        ostr << colFg({255,255,0}) << colBg({255,0,0}) << "    MCRT   " << colReset();
        break;
    case mcrt::BaseFrame::Status::FINISHED :
        if (renderPrepProgress < 1.0f) {
            ostr << colFg({0,0,255}) << "RENDER-PREP" << colReset();
        } else {
            ostr << colFg({0,0,255}) << "FINISHED   " << colReset();
        }
        break;
    case mcrt::BaseFrame::Status::CANCELLED :
        ostr << colFg({0,0,0}) << colBg({255,255,0}) << "CANCELED   " << colReset();
        break;
    case mcrt::BaseFrame::Status::ERROR :
        ostr << colFg({255,0,0}) << "ERROR      " << colReset();
        break;
    }
    return ostr.str();
}

std::string
LayoutBase::strPassStatus(bool isCoarsePass) const
{
    std::ostringstream ostr;
    if (isCoarsePass) ostr << colFg({255,255,0}) << "COARSE" << colReset();
    else              ostr << colFg({0,255,0})   << "FINE  " << colReset();
    return ostr.str();
}

std::string
LayoutBase::strExecMode(const McrtNodeInfo::ExecMode& execMode) const
{
    std::ostringstream ostr;
    ostr << colFg({255,255,0}) << colBg({255,0,0});
    switch (execMode) {
    case McrtNodeInfo::ExecMode::SCALAR  : ostr << "SCALAR"; break;
    case McrtNodeInfo::ExecMode::VECTOR  : ostr << "VECTOR"; break;
    case McrtNodeInfo::ExecMode::XPU     : ostr << " XPU  "; break;
    case McrtNodeInfo::ExecMode::AUTO    : ostr << " AUTO "; break;
    default                              : ostr << " ???? "; break;
    }
    ostr << colReset();
    return ostr.str();
}

void
LayoutBase::drawHBoxBar(unsigned barLeftBottomX,
                        unsigned barLeftBottomY,
                        unsigned barStartOffsetPixX,
                        unsigned barEndOffsetPixX,
                        unsigned barHeight,
                        float fraction,
                        const C3& c,
                        unsigned char alpha)
//
// draw horizontal box bar
//
{
    if (fraction <= 0.0f) return;
    if (barEndOffsetPixX < barStartOffsetPixX) return;

    float currFraction = fraction;
    if (currFraction > 1.0f) currFraction = 1.0f;
    int barSize = (barEndOffsetPixX - barStartOffsetPixX) * currFraction;

    int yOffset = barHeight * mFont->getBgYAdjustScale();
    int ySubTarget = 3;
    int ySub = (barHeight > ySubTarget * 2) ? ySubTarget : 0;

    int xMin = barLeftBottomX + barStartOffsetPixX;
    int yMin = barLeftBottomY - yOffset + ySub;
    int xMax = xMin + barSize;
    int yMax = barLeftBottomY + barHeight - ySub;
    Overlay::BBox2i bbox {scene_rdl2::math::Vec2i {xMin, yMin}, scene_rdl2::math::Vec2i {xMax, yMax}};

    mOverlay->drawBoxBar(bbox, c, alpha);
}

void
LayoutBase::drawHBoxBar2Sections(unsigned barLeftBottomX,
                                 unsigned barLeftBottomY,
                                 unsigned barStartOffsetPixX,
                                 unsigned barEndOffsetPixX,
                                 unsigned barHeight,
                                 float fractionA,
                                 const C3& cA,
                                 unsigned char alphaA,
                                 float fractionB,
                                 const C3& cB,
                                 unsigned char alphaB)
//
// draw horizontal box bar that consists of 2 consecutive sections
//
{
    if (fractionA <= 0.0f && fractionB <= 0.0f) return;

    auto calcBBox = [&](float minFraction, float maxFraction) {
        auto clamp01 = [](float v) { return (v > 1.0f) ? 1.0f : ((v < 0.0f) ? 0.0f : v); };

        unsigned barWidth = barEndOffsetPixX - barStartOffsetPixX + 1;
        unsigned barStartX = barLeftBottomX + barStartOffsetPixX;
        int minOffset = barWidth * clamp01(minFraction);
        int maxOffset = barWidth * clamp01(maxFraction);

        int yOffset = barHeight * mFont->getBgYAdjustScale();
        int ySubTarget = 3;
        int ySub = (barHeight > ySubTarget * 2) ? ySubTarget : 0;

        int xMin = barStartX + minOffset;
        int yMin = barLeftBottomY - yOffset + ySub;
        int xMax = barStartX + maxOffset - 1;
        int yMax = barLeftBottomY + barHeight - ySub;
        return Overlay::BBox2i {scene_rdl2::math::Vec2i {xMin, yMin}, scene_rdl2::math::Vec2i {xMax, yMax}};
    };

    if (fractionA > 0.0f) mOverlay->drawBoxBar(calcBBox(0.0f, fractionA), cA, alphaA);
    if (fractionB > 0.0f && fractionA < fractionB) {
        mOverlay->drawBoxBar(calcBBox(fractionA, fractionB), cB, alphaB);
    }
}

void
LayoutBase::drawHBarWithTitle(unsigned barLeftBottomX, unsigned barLeftBottomY, unsigned barWidth,
                              const std::string& title,
                              float fraction,
                              bool usageMode)
{
    float x = barLeftBottomX;
    float y = barLeftBottomY;

    unsigned barStartOffsetPixX;
    unsigned barEndOffsetPixX;
    unsigned barHeight;
    std::string barStr = strBar(barWidth,
                                getFontStepX(),
                                title,
                                fraction,
                                usageMode,
                                &barStartOffsetPixX,
                                &barEndOffsetPixX,
                                &barHeight);
    if (!mOverlay->drawStr(*mFont, x, y, barStr, {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayout.cc drawHBarWithTitle() failed. " << mError << '\n';
    }
    unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;

    if (usageMode || fraction < 1.0f) {
        C3 cBar {255,255,0};
        C3 cRed {255,0,0};
        unsigned char cBarAlpha = 90;
        drawHBoxBar(x, y, barStartOffsetPixX, barEndOffsetPixX, barHeight, fraction,
                    ((fraction < 0.9f) ? cBar : cRed), cBarAlpha);
    }
}

void
LayoutBase::drawHBar2SectionsWithTitle(unsigned barLeftBottomX, unsigned barLeftBottomY, unsigned barWidth,
                                       const std::string& title,
                                       float fractionA,
                                       float fractionB,
                                       bool usageMode)
{
    float x = barLeftBottomX;
    float y = barLeftBottomY;

    unsigned barStartOffsetPixX;
    unsigned barEndOffsetPixX;
    unsigned barHeight;
    std::string barStr = strBar(barWidth,
                                getFontStepX(),
                                title,
                                fractionA,
                                usageMode,
                                &barStartOffsetPixX,
                                &barEndOffsetPixX,
                                &barHeight);
    if (!mOverlay->drawStr(*mFont, x, y, barStr, {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayout.cc drawHBar2SectionsWithTitle() failed. " << mError << '\n';
    }
    unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;

    if (usageMode || fractionA < 1.0f) {
        C3 cBarA {255,255,0};
        C3 cMaxA {255,0,0};
        C3 cBarB {170,200,220}; // light blue
        C3 cMaxB {255,255,255};
        unsigned char cBarAlpha = 128;
        drawHBoxBar2Sections(x, y, barStartOffsetPixX, barEndOffsetPixX, barHeight,
                             fractionA, ((fractionA < 0.9f) ? cBarA : cMaxA), cBarAlpha,
                             fractionB, ((fractionB < 0.9f) ? cBarB : cMaxB), cBarAlpha);
    }
}

void
LayoutBase::drawVLine(unsigned x, unsigned yMin, unsigned yMax,
                      const C3& c, unsigned char alpha)
{
    mOverlay->drawVLine(x, yMin, yMax, c, alpha);
}

void
LayoutBase::drawVBarGraph(unsigned leftBottomX,
                          unsigned leftBottomY,
                          unsigned rightTopX,
                          unsigned rightTopY,
                          unsigned rulerYSize,
                          const ValueTimeTracker& vtt,
                          const C3& c, unsigned char alpha,
                          float graphTopY, // auto adjust display Y range if this value 0 or negative
                          float& maxValue,
                          float& currValue)
{
    constexpr unsigned rulerYGap = 1;

    unsigned width = rightTopX - leftBottomX + 1;
    unsigned height = rightTopY - leftBottomY + 1;

    //
    // resample value
    //
    std::vector<float> tbl;
    float residualSec = vtt.getResampleValue(width, tbl, &maxValue);
    currValue = tbl.back();

    //
    // bar graph
    //
    float yMax = (graphTopY <= 0.0f) ? maxValue : graphTopY;
    auto calcRatio = [&](float v) { return v / yMax; };
    unsigned barHeight = height - rulerYSize - rulerYGap;
    unsigned barMinY = leftBottomY;
    for (unsigned x = leftBottomX; x <= rightTopX; ++x) {
        size_t id = x - leftBottomX;
        float ratio = calcRatio(tbl[id]);
        if (ratio > 1.0f) {
            // clip bar and change color to white
            unsigned y = barHeight + leftBottomY;
            drawVLine(x, barMinY, y, {200,200,200}, alpha);
        } else {
            unsigned y = static_cast<unsigned>(barHeight * ratio) + leftBottomY;
            drawVLine(x, barMinY, y, c, alpha);
        }
    }

    //
    // ruler : 1.0 sec interval
    //
    unsigned rulerMinY = rightTopY - rulerYSize;
    unsigned rulerMaxY = rightTopY;
    C3 cSecBound {255, 255, 255};
    unsigned char alphaSecBound = 255;

    float durationSec = vtt.getValueKeepDurationSec();
    float stepSec = durationSec / static_cast<float>(width);
    float currPlotSec = residualSec;
    while (currPlotSec <= durationSec) {
        unsigned offsetX = static_cast<unsigned>(currPlotSec / stepSec);
        drawVLine(rightTopX - offsetX, rulerMinY, rulerMaxY, cSecBound, alphaSecBound);
        currPlotSec += 1.0f;
    }
}

void
LayoutBase::drawBpsVBarGraphWithTitle(unsigned leftBottomX,
                                      unsigned leftBottomY,
                                      unsigned rightTopX,
                                      unsigned rightTopY,
                                      unsigned rulerYSize,
                                      const ValueTimeTracker& vtt,
                                      const C3& c,
                                      unsigned char alpha,
                                      float graphTopY, // auto adjust Y if this value is 0 or negative
                                      const std::string& title)
{
    unsigned width = rightTopX - leftBottomX + 1;
    unsigned height = rightTopY - leftBottomY + 1;

    float maxValue, currVal;
    drawVBarGraph(leftBottomX, leftBottomY, rightTopX, rightTopY, rulerYSize,
                  vtt,
                  c,
                  alpha,
                  graphTopY,
                  maxValue, currVal);

    std::ostringstream ostr;
    ostr << colReset()
         << title
         << ' ' << strBps(currVal, 10)
         << " peak:" << strBps(maxValue, 10);

    unsigned infoX = leftBottomX;
    unsigned infoY = rightTopY - mStepPixY - rulerYSize;
    if (!mOverlay->drawStr(*mFont, infoX, infoY, ostr.str(), mCharFg, mError)) {
        std::cerr << ">> TelemetryLayout.cc drawBpsVBarGraphWithTitle() drawStr() failed. " << mError << '\n';
    }
}

unsigned
LayoutBase::calcMaxSimpleMcrtHostNameLen(const GlobalNodeInfo* gNodeInfo) const
{
    if (!gNodeInfo) return static_cast<unsigned>(0);

    unsigned maxSimpleSize = 0;
    gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
            unsigned simpleSize = static_cast<unsigned>(strSimpleHostName(node->getHostName()).size());
            if (maxSimpleSize < simpleSize) maxSimpleSize = simpleSize;
            return true;
        });
    return maxSimpleSize;
}

std::string
LayoutBase::showC3(const C3& c) const
{
    std::ostringstream ostr;
    ostr << "(r:" << std::setw(3) << static_cast<int>(c.mR)
         << " g:" << std::setw(3) << static_cast<int>(c.mG)
         << " b:" << std::setw(3) << static_cast<int>(c.mB) << ')';
    return ostr.str();
}

unsigned char
LayoutBase::getArgC0255(Arg& arg) const
{
    int v = (arg++).as<int>(0);
    return static_cast<unsigned char>(scene_rdl2::math::clamp(v, 0, 255));
}

C3
LayoutBase::getArgC3(Arg& arg) const
{
    C3 c;
    c.mR = getArgC0255(arg);
    c.mG = getArgC0255(arg);
    c.mB = getArgC0255(arg);
    return c;
}

void
LayoutBase::parserConfigure()
{
    mParser.description("layout command");

    mParser.opt("charFg", "<r> <g> <b>", "set default char fg color",
                [&](Arg& arg) {
                    mCharFg = getArgC3(arg);
                    return arg.msg("charFg " + showC3(mCharFg) + '\n');
                });
    mParser.opt("charBg", "<r> <g> <b>", "set default char bg color",
                [&](Arg& arg) {
                    mCharBg = getArgC3(arg);
                    return arg.msg("charBg " + showC3(mCharBg) + '\n');
                });
    mParser.opt("panelBgCol", "<r> <g> <b> <a>", "set panel bg color and alpha",
                [&](Arg& arg) {
                    mPanelBg = getArgC3(arg);
                    mPanelBgAlpha = getArgC0255(arg);
                    return arg.fmtMsg("panelBg %s %d\n", showC3(mPanelBg).c_str(), static_cast<int>(mPanelBgAlpha));
                });
}

} // namespace telemetry
} // namespace mcrt_dataio
