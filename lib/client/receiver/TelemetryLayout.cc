// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "TelemetryLayout.h"
#include "TelemetryOverlay.h"

#include <scene_rdl2/common/math/Math.h>

#include <algorithm> // clamp
#include <iomanip>
#include <sstream>

namespace mcrt_dataio {
namespace telemetry {

LayoutBase::LayoutBase(std::shared_ptr<Overlay> overlay, std::shared_ptr<Font> font)
    : mOverlay(overlay)
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
LayoutBase::strByte(size_t numByte) const
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
    return ostr.str();
}

std::string
LayoutBase::strBps(float bps) const
{
    size_t byte = static_cast<size_t>(bps);
    return strByte(byte) + "/s";
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
LayoutBase::drawBoxBar(unsigned barLeftBottomX,
                       unsigned barLeftBottomY,
                       unsigned barStartOffsetPixX,
                       unsigned barEndOffsetPixX,
                       unsigned barHeight,
                       float fraction,
                       const C3& c,
                       unsigned char alpha)
{
    if (fraction <= 0.0f) return;

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
