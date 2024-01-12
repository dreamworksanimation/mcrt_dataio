// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

#include <sstream>

namespace mcrt_dataio {
namespace telemetry {

void
LayoutDevel::drawMain(const DisplayInfo& info)
{
    subPanelTitle(info);
    drawGlobalInfo(info);
    drawDispatchMergeComputation(info);
    drawGlobalProgressBar(info);
    drawMcrtComputation(info);
}

void
LayoutDevel::drawGlobalInfo(const DisplayInfo& info)
{
    std::ostringstream ostr;
    ostr << colReset()
         << "   FrameId:" << info.mFrameId << '\n'
         << "    Status:" << strFrameStatus(info.mStatus, info.mRenderPrepProgress) << '\n'
         << "FbActivity:" << info.mFbActivityCounter << '\n'
         << "    Decode:" << info.mDecodeProgressiveFrameCounter << '\n'
         << "      Pass:" << strPassStatus(info.mIsCoarsePass) << '\n'
         << "   Latency:" << strSec(info.mCurrentLatencySec) << '\n'
         << "RecvImgFps:" << strFps(info.mReceiveImageDataFps);
    subPanelMessage(10, // x
                    mBBoxTitle.lower.y - 10 - mStepPixY, // y
                    ostr.str(),
                    mBBoxGlobalInfo);
}
    
void
LayoutDevel::drawDispatchMergeComputation(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;
    if (gNodeInfo->getMcrtTotal() == 1) {
        mBBoxDispatchMergeComputation = setBBox(0, 0, 0, 0);
        return;
    }

    std::ostringstream ostr;
    ostr << "Dispatch {\n"
         << "  " << strSimpleHostName(gNodeInfo->getDispatchHostName()) << '\n'
         << "  ClockShift:" << strMillisec(gNodeInfo->getDispatchClockTimeShift()) << '\n'
         << "}\n\n";

    ostr << "Merge (Progress:" << strPct(gNodeInfo->getMergeProgress()) << ") {\n"
         << "  " << strSimpleHostName(gNodeInfo->getMergeHostName()) << '\n'
         << "   Cpu:" << gNodeInfo->getMergeAssignedCpuTotal() << '/' << gNodeInfo->getMergeCpuTotal()
         << " (" << strPct(gNodeInfo->getMergeCpuUsage()) << ")\n"
         << "   Mem:" << strByte(gNodeInfo->getMergeMemTotal())
         << " (" << strPct(gNodeInfo->getMergeMemUsage()) << ")\n"
         << "  NetRecv:" << strBps(gNodeInfo->getMergeNetRecvBps()) << '\n'
         << "     Recv:" << strBps(gNodeInfo->getMergeRecvBps()) << '\n'
         << "  NetSend:" << strBps(gNodeInfo->getMergeNetSendBps()) << '\n'
         << "     Send:" << strBps(gNodeInfo->getMergeSendBps()) << '\n'
         << "}";

    subPanelMessage(10, // x
                    mBBoxGlobalInfo.lower.y - 10 - mStepPixY, // y
                    ostr.str(),
                    mBBoxDispatchMergeComputation);
}

void
LayoutDevel::drawGlobalProgressBar(const DisplayInfo& info)
{
    constexpr unsigned gapWidth = 10;
    unsigned barLeftBottomX = ((mBBoxDispatchMergeComputation.upper.x > 0)
                               ? mBBoxDispatchMergeComputation.upper.x
                               : mBBoxGlobalInfo.upper.x) + gapWidth;
    unsigned barLeftBottomY = mBBoxTitle.lower.y - 10 - mStepPixY;
    unsigned barWidth = mOverlay->getWidth() - barLeftBottomX - gapWidth;

    subPanelGlobalProgressBar(barLeftBottomX, barLeftBottomY, barWidth, info, mBBoxGlobalProgressBar);
}

void
LayoutDevel::drawMcrtComputation(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;

    constexpr unsigned gapX = 10;
    constexpr unsigned gapY = 10;
    unsigned leftX = ((mBBoxDispatchMergeComputation.upper.x > 0)
                      ? mBBoxDispatchMergeComputation.upper.x
                      : mBBoxGlobalInfo.upper.x) + gapX;
    unsigned width = mOverlay->getWidth() - leftX - gapX;

    unsigned fontStepX = getFontStepX();
    unsigned barFontTotalX = width / fontStepX - 2;
    unsigned barStartX = leftX + fontStepX * 2;
    unsigned barA_fontTotal = barFontTotalX / 3;
    unsigned barB_fontTotal = barFontTotalX / 3;
    unsigned barC_fontTotal = barFontTotalX - barA_fontTotal - barB_fontTotal;
    unsigned barA_w = barA_fontTotal * fontStepX;
    unsigned barB_w = barB_fontTotal * fontStepX;
    unsigned barC_w = barC_fontTotal * fontStepX;

    mBarPosArray.resize(gNodeInfo->getMcrtTotal());

    int hostNameW = calcMaxSimpleMcrtHostNameLen(gNodeInfo);

    auto strNode = [&](std::shared_ptr<McrtNodeInfo> node, BarPos& barPos) {
        const scene_rdl2::grid_util::RenderPrepStats& renderPrepStats = node->getRenderPrepStats();
        float renderPrepProgress = (static_cast<float>(renderPrepStats.getCurrSteps()) /
                                    static_cast<float>(renderPrepStats.getTotalSteps()));
        float mcrtProgress = node->getProgress();
        float mcrtGlobalProgress = node->getGlobalProgress();

        int w = scene_rdl2::str_util::getNumberOfDigits(gNodeInfo->getMcrtTotal());
        barPos.mXoffset[0] = 0.0;
        barPos.mXoffset[1] = barA_w;
        barPos.mXoffset[2] = barA_w + barB_w;

        std::ostringstream ostr;
        ostr
        << "Id:" << std::setw(w) << std::setfill('0') << node->getMachineId() << ' ' 
        << std::setw(hostNameW) << std::setfill(' ') << std::left << strSimpleHostName(node->getHostName())
        << " Cpu:" << node->getAssignedCpuTotal() << '/' << node->getCpuTotal()
        << " Mem:" << strByte(node->getMemTotal())
        << " Act:" << strBool(node->getRenderActive())
        << " Exc:" << strExecMode(node->getExecMode())
        << " Syc:" << node->getSyncId()
        << " Clk:" << strMillisec(node->getClockTimeShift())
        << " NRv:" << strBps(node->getNetRecvBps())
        << " NSd:" << strBps(node->getNetSendBps())
        << " Snd:" << strBps(node->getSendBps())        
        << " Snp:" << strMillisec(node->getSnapshotToSend())
        << "\n";

        if (renderPrepProgress < 1.0f) {
            ostr << strBar(barA_w,
                           fontStepX,
                           "RPrep:" + strPct(renderPrepProgress),
                           renderPrepProgress,
                           false,
                           &barPos.mXmin[0],
                           &barPos.mXmax[0],
                           &barPos.mHeight[0]);
            barPos.mFraction[0] = renderPrepProgress;
            barPos.mExtraBarFlag = false;
        } else {
            ostr << strBar(barA_w,
                           fontStepX,
                           "MCRT:" + strPct(mcrtProgress) + '/' + strPct(mcrtGlobalProgress),
                           mcrtProgress,
                           false,
                           &barPos.mXmin[0],
                           &barPos.mXmax[0],
                           &barPos.mHeight[0]);
            barPos.mFraction[0] = mcrtProgress;
            barPos.mExtraBarFlag = true;
            barPos.mFractionExtra = mcrtGlobalProgress;
        }

        ostr << strBar(barB_w,
                       fontStepX,
                       " CPU:" + strPct(node->getCpuUsage()),
                       node->getCpuUsage(),
                       true,
                       &barPos.mXmin[1],
                       &barPos.mXmax[1],
                       &barPos.mHeight[1]);
        barPos.mFraction[1] = node->getCpuUsage();

        ostr << strBar(barC_w,
                       fontStepX,
                       " Mem:" + strPct(node->getMemUsage()),
                       node->getMemUsage(),
                       true,
                       &barPos.mXmin[2],
                       &barPos.mXmax[2],
                       &barPos.mHeight[2]);
        barPos.mFraction[2] = node->getMemUsage();

        barPos.mActiveBgFlag = (node->getSyncId() == info.mFrameId);

        return ostr.str();
    };

    unsigned yStep = mStepPixY;
    unsigned yStart = mBBoxGlobalProgressBar.lower.y - gapY - yStep;

    std::ostringstream ostr;
    ostr << "MCRT Computation (totalMcrt:" << gNodeInfo->getMcrtTotal()
         << " totalCpu:" << gNodeInfo->getMcrtTotalCpu() << ") {\n"
         << "  isAllStop:" << strBool(gNodeInfo->isMcrtAllStop())
         << " isAllStart:" << strBool(gNodeInfo->isMcrtAllStart())
         << " isAllFinishRenderPrep:" << strBool(gNodeInfo->isMcrtAllRenderPrepCompletedOrCanceled()) << '\n';
    unsigned yBase = yStart - yStep * 3;
    int id = 0;
    bool allActiveBgFlag = true;
    gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
            BarPos& currBarPos = mBarPosArray[id++];
            currBarPos.mY = yBase;
            yBase -= yStep * 2;
            
            ostr << scene_rdl2::str_util::addIndent(strNode(node, currBarPos)) << '\n';

            if (!currBarPos.mActiveBgFlag) allActiveBgFlag = false;
            return true;
        });
    ostr << "}";

    unsigned x = leftX;
    unsigned y = yStart;

    if (!mOverlay->drawStr(*mFont, x, y, ostr.str(), {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayoutDevel.cc drawMcrtComputation drawStr failed. "
                  << mError << '\n';
    }
    unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;

    mBBoxMcrtComputation = mOverlay->calcDrawBbox(strItemId, strItemId);
    if (allActiveBgFlag) {
        mOverlay->drawBox(mBBoxMcrtComputation, mPanelBg, mPanelBgAlpha);
    } else {
        int minX = mBBoxMcrtComputation.lower.x;
        int maxX = mBBoxMcrtComputation.upper.x;
        int maxY = mBBoxMcrtComputation.upper.y;
        int minY = maxY - yStep * 2;
        mOverlay->drawBox(setBBox(minX, minY, maxX, maxY), mPanelBg, mPanelBgAlpha);
        minY = mBBoxMcrtComputation.lower.y;
        maxY = minY + yStep + 1;
        mOverlay->drawBox(setBBox(minX, minY, maxX, maxY), mPanelBg, mPanelBgAlpha);
    }

    for (size_t i = 0; i < mBarPosArray.size(); ++i) {
        const BarPos& currBarPos = mBarPosArray[i];

        C3 c3 {255,255,0};
        C3 cRed {255,0,0};
        unsigned char alpha = 90;
        if (!currBarPos.mExtraBarFlag) {
            drawHBoxBar(barStartX + currBarPos.mXoffset[0],
                        currBarPos.mY,
                        currBarPos.mXmin[0],
                        currBarPos.mXmax[0],
                        currBarPos.mHeight[0],
                        currBarPos.mFraction[0],
                        c3,
                       alpha);
        } else {
            C3 cBar {170,200,220}; // light blue
            C3 cMax {255,255,255};
            drawHBoxBar2Sections(barStartX + currBarPos.mXoffset[0],
                                 currBarPos.mY,
                                 currBarPos.mXmin[0],
                                 currBarPos.mXmax[0],
                                 currBarPos.mHeight[0],
                                 currBarPos.mFraction[0],
                                 c3,
                                 alpha,
                                 currBarPos.mFractionExtra,
                                 ((currBarPos.mFractionExtra < 0.9f) ? cBar : cMax),
                                 alpha);
        }
        drawHBoxBar(barStartX + currBarPos.mXoffset[1],
                    currBarPos.mY,
                    currBarPos.mXmin[1],
                    currBarPos.mXmax[1],
                    currBarPos.mHeight[1],
                    currBarPos.mFraction[1],
                    ((currBarPos.mFraction[1] < 0.9) ? c3 : cRed),
                    alpha);
        drawHBoxBar(barStartX + currBarPos.mXoffset[2],
                    currBarPos.mY,
                    currBarPos.mXmin[2],
                    currBarPos.mXmax[2],
                    currBarPos.mHeight[2],
                    currBarPos.mFraction[2],
                    ((currBarPos.mFraction[2] < 0.9) ? c3 : cRed),
                    alpha);
        if (!allActiveBgFlag) {
            int minX = mBBoxMcrtComputation.lower.x;
            int maxX = mBBoxMcrtComputation.upper.x;
            int minY = currBarPos.mY;
            int maxY = minY + yStep * 2;
            C3 nonActiveBg {96,96,96};
            mOverlay->drawBox(setBBox(minX, minY, maxX, maxY),
                              ((currBarPos.mActiveBgFlag) ? mPanelBg : nonActiveBg), mPanelBgAlpha);
        }
    }
}

} // namespace telemetry
} // namespace mcrt_dataio
