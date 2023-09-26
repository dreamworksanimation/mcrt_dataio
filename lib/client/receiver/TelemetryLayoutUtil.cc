// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

namespace mcrt_dataio {
namespace telemetry {

void
LayoutUtil::drawUtilGlobalInfo(const std::string& msg,
                               unsigned x,
                               unsigned y,
                               Overlay::BBox2i& bboxGlobalInfo)
{
    if (!mOverlay->drawStr(*mFont,
                           x,
                           y,
                           msg,
                           mCharFg,
                           mError)) {
        std::cerr << ">> TelemetryLayoutUtil.cc drawUtilGlobalInfo() drawStr() failed. " << mError << '\n';
    } else {
        unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;
        bboxGlobalInfo = mOverlay->calcDrawBbox(strItemId, strItemId);
        mOverlay->drawBox(bboxGlobalInfo, mPanelBg, mPanelBgAlpha);
    }
}

void
LayoutUtil::drawUtilGlobalProgressBar(unsigned barLeftBottomX,
                                      unsigned barLeftBottomY,
                                      unsigned barWidth,
                                      const DisplayInfo& info,
                                      Overlay::BBox2i& bboxGlobalProgressBar)
{
    unsigned fontStepX = (mOverlay->getFontStepX() == 0) ? mFont->getFontSizePoint() : mOverlay->getFontStepX();

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

    unsigned x = barLeftBottomX;
    unsigned y = barLeftBottomY;

    if (!mOverlay->drawStr(*mFont, x, y, renderPrepBarStr, {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayoutUtil.cc drawUtilGlobalProgressBar() drawStr() failed. "
                  << mError << '\n';
    }
    unsigned startStrItemId = mOverlay->getDrawStrItemTotal() - 1;

    unsigned y2 = y - mFont->getFontSizePoint() * 1.1f;
    if (!mOverlay->drawStr(*mFont, x, y2, mcrtBarStr, {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayoutUtil.cc drawUtilGlobalProgressBar() drawStr() failed. "
                  << mError << '\n';
    }
    unsigned endStrItemId = mOverlay->getDrawStrItemTotal() - 1;

    bboxGlobalProgressBar = mOverlay->calcDrawBbox(startStrItemId, endStrItemId);    
    mOverlay->drawBox(bboxGlobalProgressBar, mPanelBg, mPanelBgAlpha);

    C3 cBar {255,255,0};
    unsigned char cBarAlpha = 90;
    if (info.mRenderPrepProgress < 1.0f) {
        drawBoxBar(x, y, renderPrepBarOffsetStartX, renderPrepBarOffsetEndX, renderPrepBarHeight,
                   info.mRenderPrepProgress, cBar, cBarAlpha);
    }
    if (info.mProgress < 1.0f) {
        drawBoxBar(x, y2, mcrtBarOffsetStartX, mcrtBarOffsetEndX, mcrtBarHeight,
                   info.mProgress, cBar, cBarAlpha);
    }
}

} // namespace telemetry
} // namespace mcrt_dataio
