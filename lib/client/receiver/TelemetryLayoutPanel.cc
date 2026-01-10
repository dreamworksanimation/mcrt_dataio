// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

namespace mcrt_dataio {
namespace telemetry {

void
LayoutPanel::subPanelTitle(const DisplayInfo& info)
{
    constexpr unsigned gapX = 10;

    const unsigned y = (mMaxYLines - 1) * mStepPixY + mOffsetBottomPixY;

    std::ostringstream ostr;
    ostr << colReset()
         << "Panel:" << colFg({255,255,0}) << getName() << colReset();
    subPanelMessage(gapX, y, ostr.str(), mBBoxTitle);

    ostr.str("");
    ostr << strSec(info.mElapsedSecFromStart);
    std::string elapsedSecStr = ostr.str();
    size_t len = Overlay::msgDisplayLen(elapsedSecStr);
    unsigned posX = mOverlay->getWidth() - gapX - len * getFontStepX();
    subPanelMessage(posX, y, elapsedSecStr, mBBoxElapsedSecFromStart);
}

void
LayoutPanel::subPanelMessage(const unsigned x, const unsigned y, const std::string& str, Overlay::BBox2i& bbox)
{
    if (!mOverlay->drawStr(*mFont, x, y, str, mCharFg, mError)) {
        std::cerr << ">> TelemetryLayoutPanel.cc subPanelMessage() drawStr() failed. " << mError << '\n';
    } else {
        unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;
        bbox = mOverlay->calcDrawBbox(strItemId, strItemId);
        mOverlay->drawBox(bbox, mPanelBg, mPanelBgAlpha);
    }
}

void
LayoutPanel::subPanelMessageUpperRight(const unsigned x, const unsigned y,
                                       const std::string& str, Overlay::BBox2i& bbox)
{
    if (!mOverlay->drawStr(*mFont, x, y, str, mCharFg, mError)) {
        std::cerr << ">> TelemetryLayoutPanel.cc subPanelMessageUpperRight() drawStr() failed. " << mError << '\n';
    } else {
        const unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;
        bbox = mOverlay->calcDrawBbox(strItemId, strItemId);
        const int deltaX = x - bbox.upper.x;
        const int deltaY = y - bbox.upper.y;
        mOverlay->moveStr(strItemId, deltaX, deltaY);

        bbox.lower.x += deltaX;
        bbox.lower.y += deltaY;
        bbox.upper.x += deltaX;
        bbox.upper.y += deltaY;
        mOverlay->drawBox(bbox, mPanelBg, mPanelBgAlpha);
    }
}

void
LayoutPanel::subPanelMessageCenter(const unsigned x, const unsigned y,
                                   const std::string& str, Overlay::BBox2i& bbox)
{
    if (!mOverlay->drawStr(*mFont, x, y, str, mCharFg, mError)) {
        std::cerr << ">> TelemetryLayoutPanel.cc subPanelMessageCenter() drawStr() failed. " << mError << '\n';
    } else {
        const unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;
        bbox = mOverlay->calcDrawBbox(strItemId, strItemId);
        const int deltaX = x - (bbox.upper.x + bbox.lower.x) / 2;
        const int deltaY = y - (bbox.upper.y + bbox.lower.y) / 2;
        mOverlay->moveStr(strItemId, deltaX, deltaY);

        bbox.lower.x += deltaX;
        bbox.lower.y += deltaY;
        bbox.upper.x += deltaX;
        bbox.upper.y += deltaY;
        mOverlay->drawBox(bbox, mPanelBg, mPanelBgAlpha);
    }
}

void
LayoutPanel::subPanelGlobalInfo(const unsigned x, const unsigned y, const DisplayInfo& info, Overlay::BBox2i& bbox)
{
    std::ostringstream ostr;
    ostr << colReset()
         << "FrameId:" << info.mFrameId
         << " Status:" << strFrameStatus(info.mStatus, info.mRenderPrepProgress)
         << " Pass:" << strPassStatus(info.mIsCoarsePass) << '\n'
         << "FbActivity:" << info.mFbActivityCounter
         << " Decode:" << info.mDecodeProgressiveFrameCounter
         << " Latency:" << strSec(info.mCurrentLatencySec)
         << " RecvImgFps:" << strFps(info.mReceiveImageDataFps);
    subPanelMessage(x, y, ostr.str(), bbox);
}

void
LayoutPanel::subPanelGlobalProgressBar(const unsigned barLeftBottomX,
                                       const unsigned barLeftBottomY,
                                       const unsigned barWidth,
                                       const DisplayInfo& info,
                                       Overlay::BBox2i& bboxGlobalProgressBar)
{
    const unsigned fontStepX = (mOverlay->getFontStepX() == 0) ? mFont->getFontSizePoint() : mOverlay->getFontStepX();

    unsigned renderPrepBarOffsetStartX, renderPrepBarOffsetEndX, renderPrepBarHeight;
    std::string renderPrepBarStr = strBar(barWidth,
                                          fontStepX,
                                          "RndrPrep:" + strPct(info.mRenderPrepProgress),
                                          info.mRenderPrepProgress,
                                          false,
                                          &renderPrepBarOffsetStartX,
                                          &renderPrepBarOffsetEndX,
                                          &renderPrepBarHeight);
    unsigned mcrtBarOffsetStartX, mcrtBarOffsetEndX, mcrtBarHeight;
    std::string mcrtBarStr = strBar(barWidth,
                                    fontStepX,
                                    "    MCRT:" + strPct(info.mProgress),
                                    info.mProgress,
                                    false,
                                    &mcrtBarOffsetStartX,
                                    &mcrtBarOffsetEndX,
                                    &mcrtBarHeight);

    const unsigned x = barLeftBottomX;
    const unsigned y = barLeftBottomY;

    if (!mOverlay->drawStr(*mFont, x, y, renderPrepBarStr, {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayoutPanel.cc subPanelGlobalProgressBar() drawStr() failed. "
                  << mError << '\n';
    }
    const unsigned startStrItemId = mOverlay->getDrawStrItemTotal() - 1;

    const unsigned y2 = y - mFont->getFontSizePoint() * 1.1f;
    if (!mOverlay->drawStr(*mFont, x, y2, mcrtBarStr, {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayoutPanel.cc subPanelGlobalProgressBar() drawStr() failed. "
                  << mError << '\n';
    }
    const unsigned endStrItemId = mOverlay->getDrawStrItemTotal() - 1;

    bboxGlobalProgressBar = mOverlay->calcDrawBbox(startStrItemId, endStrItemId);    
    mOverlay->drawBox(bboxGlobalProgressBar, mPanelBg, mPanelBgAlpha);

    const C3 cBar {255,255,0};
    const unsigned char cBarAlpha = 90;
    if (info.mRenderPrepProgress < 1.0f) {
        drawHBoxBar(x, y, renderPrepBarOffsetStartX, renderPrepBarOffsetEndX, renderPrepBarHeight,
                    info.mRenderPrepProgress, cBar, cBarAlpha);
    }
    if (info.mProgress < 1.0f) {
        drawHBoxBar(x, y2, mcrtBarOffsetStartX, mcrtBarOffsetEndX, mcrtBarHeight,
                    info.mProgress, cBar, cBarAlpha);
    }
}

void
LayoutPanel::subPanelNetIOCpuMemAndProgress(const unsigned leftBottomX,
                                            const unsigned leftBottomY,
                                            const unsigned rightTopX,
                                            const unsigned rightTopY,
                                            const float graphTopY, // auto adjust Y if this value is 0 or negative
                                            const unsigned rulerYSize,
                                            const std::string& title,
                                            const int cpuTotal,
                                            const float cpuFraction,
                                            const size_t memTotal, // Byte
                                            const float memFraction,
                                            const float renderPrepFraction,
                                            const float mcrtProgress,
                                            const float mcrtGlobalProgress,
                                            const ValueTimeTracker& sendVtt,
                                            const ValueTimeTracker& recvVtt,
                                            const bool activeBgColFlag,
                                            Overlay::BBox2i& bbox)
{
    const unsigned width = rightTopX - leftBottomX;
    
    constexpr unsigned gapY = 5;

    const unsigned titleStartX = leftBottomX;
    const unsigned titleStartY = rightTopY - mStepPixY;
    const unsigned titleHeight = mStepPixY;

    const unsigned cpuStartX = leftBottomX;
    const unsigned cpuStartY = titleStartY - mStepPixY - gapY;
    const unsigned cpuBarWidth = width;

    const unsigned memStartX = leftBottomX;
    const unsigned memStartY = cpuStartY - mStepPixY - gapY;
    const unsigned memBarWidth = width;

    const unsigned progressStartX = leftBottomX;
    const unsigned progressStartY = memStartY - mStepPixY - gapY;
    const unsigned progressBarWidth = width;

    bool displayProgressBar = true;
    if (renderPrepFraction < 0.0f && mcrtProgress < 0.0f) displayProgressBar = false;

    const unsigned currY = (displayProgressBar) ? progressStartY : memStartY;
    const unsigned graphPanelHeight = (currY - gapY - leftBottomY - gapY) / 2;
        
    const unsigned sendPanelMinX = leftBottomX;
    const unsigned sendPanelMaxX = rightTopX;
    const unsigned sendPanelMaxY = currY - gapY;
    const unsigned sendPanelMinY = sendPanelMaxY - graphPanelHeight;

    const unsigned recvPanelMinX = sendPanelMinX;
    const unsigned recvPanelMaxX = sendPanelMaxX;
    const unsigned recvPanelMaxY = sendPanelMinY - gapY;
    const unsigned recvPanelMinY = recvPanelMaxY - graphPanelHeight;

    if (!mOverlay->drawStr(*mFont, titleStartX, titleStartY, title, mCharFg, mError)) {
        std::cerr << ">> TelemetryLayoutPanel.cc subPanelNetIOCpuMemAndProgress() drawStr() failed. " << mError << '\n';
    }

    std::ostringstream ostr;
    ostr << "Cpu:" << std::setw(8) << std::left << cpuTotal << '(' << strPct(cpuFraction) << ')';
    std::string cpuStr = ostr.str();
    drawHBarWithTitle(cpuStartX, cpuStartY, cpuBarWidth, cpuStr, cpuFraction, true);

    ostr.str("");
    ostr << "Mem:" << strByte(memTotal) << '(' << strPct(memFraction) << ')';
    std::string memStr = ostr.str();
    drawHBarWithTitle(memStartX, memStartY, memBarWidth, memStr, memFraction, true);

    if (displayProgressBar) {
        if (renderPrepFraction >= 0.0f && renderPrepFraction < 1.0f) {
            ostr.str("");
            ostr << "RPrep:" << strPct(renderPrepFraction);
            drawHBarWithTitle(progressStartX, progressStartY, progressBarWidth,
                              ostr.str(), renderPrepFraction, false);
        } else if (mcrtProgress >= 0.0f && mcrtGlobalProgress >= 0.0f) {
            ostr.str("");
            ostr << "MCRT:" << strPct(mcrtProgress) << '/' << strPct(mcrtGlobalProgress);
            drawHBar2SectionsWithTitle(progressStartX, progressStartY, progressBarWidth,
                                       ostr.str(), mcrtProgress, mcrtGlobalProgress, false);
        }
    }

    drawBpsVBarGraphWithTitle(sendPanelMinX,
                              sendPanelMinY,
                              sendPanelMaxX,
                              sendPanelMaxY,
                              rulerYSize,
                              sendVtt,
                              {255,165,0}, // orange
                              200, // alpha
                              graphTopY,
                              "NetSnd");

    drawBpsVBarGraphWithTitle(recvPanelMinX,
                              recvPanelMinY,
                              recvPanelMaxX,
                              recvPanelMaxY,
                              rulerYSize,
                              recvVtt,
                              {157,204,224}, // light blue
                              200, // alpha
                              graphTopY,
                              "NetRcv");

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-narrowing"
#endif

    // FIXME: non-constant-expression cannot be narrowed from type 'unsigned int' to 'int' in initializer list ( Bbox is an int , coords are unsigned ). Worked around it by disabling diagnostic

    bbox = Overlay::BBox2i(scene_rdl2::math::Vec2i {leftBottomX, leftBottomY},
                           scene_rdl2::math::Vec2i {rightTopX, rightTopY});
#if defined(__clang__)
#pragma clang diagnostic pop
#endif


    const C3 cNonActive {96,96,96};
    mOverlay->drawBox(bbox, ((activeBgColFlag) ? mPanelBg : cNonActive), mPanelBgAlpha);
}

} // namespace telemetry
} // namespace mcrt_dataio
