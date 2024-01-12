// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

namespace mcrt_dataio {
namespace telemetry {

void
LayoutCorePerf::drawMain(const DisplayInfo& info)
{
    subPanelTitle(info);
    subPanelGlobalInfo(10, mBBoxTitle.lower.y - 10 - mStepPixY, info, mBBoxGlobalInfo);
    drawGlobalProgressBar(info);
    drawMcrtComputation(info);
}

void    
LayoutCorePerf::drawGlobalProgressBar(const DisplayInfo& info)
{
    constexpr unsigned gapWidth = 10;
    constexpr unsigned gapHeight = 10;
    unsigned barLeftBottomX = mBBoxGlobalInfo.upper.x + gapWidth;
    unsigned barLeftBottomY = mBBoxTitle.lower.y - gapHeight - mStepPixY;
    unsigned barWidth = mOverlay->getWidth() - barLeftBottomX - gapWidth;

    subPanelGlobalProgressBar(barLeftBottomX, barLeftBottomY, barWidth, info, mBBoxGlobalProgressBar);
}

void
LayoutCorePerf::drawMcrtComputation(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;

    constexpr unsigned gapX = 10;
    unsigned leftX = gapX;
    unsigned mcrtWidth = mOverlay->getWidth() - leftX - gapX;
    unsigned fontStepX = getFontStepX();

    constexpr unsigned gapY = 10;
    unsigned mcrtHeight = mBBoxGlobalProgressBar.lower.y - gapY * 2;
    unsigned yStep = mStepPixY;
    unsigned yStart = mBBoxGlobalProgressBar.lower.y - gapY - yStep;
    unsigned yMax = mcrtHeight / yStep - 1; // substruct 1 for title row

    if (!setupCorePerfRowInfo(gNodeInfo, yMax)) return;

    mMcrtPosArray.resize(gNodeInfo->getMcrtTotal());

    bool allActiveBgFlag = true;
    int titleWidthChar = 0;
    std::ostringstream ostr;
    {
        auto strNodeTitle = [&](std::shared_ptr<McrtNodeInfo> node, McrtPos& mcrtPos) {
            mcrtPos.mRowCoreNum = mRowCoreNum;
            mcrtPos.mNumOfRows = std::ceil(static_cast<float>(node->getCpuTotal()) /
                                           static_cast<float>(mcrtPos.mRowCoreNum));

            std::string title = drawSingleNodeTitle(gNodeInfo->getMcrtTotal(),
                                                    node,
                                                    mcrtPos.mNumOfRows);
            mcrtPos.mTitleWidthChar = static_cast<unsigned>(Overlay::msgDisplayWidth(title));
            return title;
        };

        ostr << "MCRT Computation (totalMcrt:" << gNodeInfo->getMcrtTotal()
             << " totalCpu:" << gNodeInfo->getMcrtTotalCpu() << ") "
             << " isAllStop:" << strBool(gNodeInfo->isMcrtAllStop())
             << " isAllStart:" << strBool(gNodeInfo->isMcrtAllStart())
             << " isAllFinishRenderPrep:" << strBool(gNodeInfo->isMcrtAllRenderPrepCompletedOrCanceled()) << '\n';
        unsigned yBase = yStart - yStep;
        int id = 0;
        gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
                McrtPos& currMcrtPos = mMcrtPosArray[id++];
                currMcrtPos.mMaxY = yBase;
                currMcrtPos.mYStep = yStep;
                ostr << strNodeTitle(node, currMcrtPos);
                if (id < mMcrtPosArray.size()) ostr << '\n';

                if (currMcrtPos.mTitleWidthChar > titleWidthChar) titleWidthChar = currMcrtPos.mTitleWidthChar;
                yBase -= yStep * currMcrtPos.mNumOfRows;

                currMcrtPos.mActiveBgFlag = (node->getSyncId() == info.mFrameId);
                if (!currMcrtPos.mActiveBgFlag) allActiveBgFlag = false;
                return true;
            });
    }

    unsigned x = leftX;
    unsigned y = yStart;

    if (!mOverlay->drawStr(*mFont, x, y, ostr.str(), {255,255,255}, mError)) {
        std::cerr << ">> TelemetryLayoutCorePerf.cc drawMcrtComputation drawStr failed. "
                  << mError << '\n';
    }
    unsigned strItemId = mOverlay->getDrawStrItemTotal() - 1;
    mBBoxMcrtComputation = mOverlay->calcDrawBbox(strItemId, strItemId);

    int id = 0;
    gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
            McrtPos& currMcrtPos = mMcrtPosArray[id];
            
            currMcrtPos.mCoreWinXMin = leftX + (titleWidthChar + 1) * fontStepX;
            currMcrtPos.mCoreWinXMax = leftX + mcrtWidth - gapX - 1;
            currMcrtPos.mCoreWinYMax = currMcrtPos.mMaxY + currMcrtPos.mYStep;
            currMcrtPos.mCoreWinYMin = currMcrtPos.mCoreWinYMax - currMcrtPos.mYStep * currMcrtPos.mNumOfRows;
            mBBoxMcrtComputation.
                extend({scene_rdl2::math::Vec2i{currMcrtPos.mCoreWinXMin, currMcrtPos.mCoreWinYMin},
                        scene_rdl2::math::Vec2i{currMcrtPos.mCoreWinXMax + gapX, currMcrtPos.mCoreWinYMax}});

            currMcrtPos.mSingleCoreGapX = 6;
            unsigned coreWinWidth = currMcrtPos.mCoreWinXMax - currMcrtPos.mCoreWinXMin + 1;
            currMcrtPos.mSingleCoreWidth =
                (coreWinWidth + currMcrtPos.mSingleCoreGapX) / currMcrtPos.mRowCoreNum -
                currMcrtPos.mSingleCoreGapX;

            drawCorePerfSingleNode(node, currMcrtPos);

            id++;
            return true;
        });

    if (allActiveBgFlag) {
        mOverlay->drawBox(mBBoxMcrtComputation, mPanelBg, mPanelBgAlpha);
    } else {
        int minX = mBBoxMcrtComputation.lower.x;
        int maxX = mBBoxMcrtComputation.upper.x;
        int maxY = mBBoxMcrtComputation.upper.y;
        int minY = maxY - yStep;
        mOverlay->drawBox(setBBox(minX, minY, maxX, maxY), mPanelBg, mPanelBgAlpha);

        C3 nonActiveBg {96,96,96};
        for (size_t id = 0; id < mMcrtPosArray.size(); ++id) {
            McrtPos& currMcrtPos = mMcrtPosArray[id];

            minY = currMcrtPos.mCoreWinYMin;
            maxY = currMcrtPos.mCoreWinYMax;
            mOverlay->drawBox(setBBox(minX, minY, maxX, maxY),
                              ((currMcrtPos.mActiveBgFlag) ? mPanelBg : nonActiveBg), mPanelBgAlpha);
        }
    }
}

bool
LayoutCorePerf::setupCorePerfRowInfo(const GlobalNodeInfo* gNodeInfo, unsigned yMax)
{
    if (gNodeInfo->getMcrtTotal() == mComputeRowInfoMcrtTotal) {
        return true; // early exit. We don't need recalculate row info
    }
    
    //
    // We only update corePerf row info when mcrtTotal number is updated
    //
    mRowCoreNum = calcMinRowCoreNum(gNodeInfo, yMax);
    if (mRowCoreNum == 0) {
        std::cerr << "mRowCoreNum is zero. Skip drawMcrtComputation()\n";
        return false;
    }
    mMinRowMcrtComputation = calcMinRowMcrtComputation(gNodeInfo);
    if (mMinRowMcrtComputation == 0) {
        std::cerr << "mMinRowMcrtComputation is zero. Skip drawMcrtComputation()\n";
        return false;
    }
    mComputeRowInfoMcrtTotal = gNodeInfo->getMcrtTotal();

    return true;
}

unsigned
LayoutCorePerf::calcMinRowCoreNum(const GlobalNodeInfo* gNodeInfo, unsigned yMax) const
{
    if (gNodeInfo->getMcrtTotal() > yMax) {
        return 0; // This is error. yMax is too small
    }

    auto calcMaxCoreNode = [&]() {
        unsigned maxCore = 0;
        gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
                if (node->getCpuTotal() > maxCore) maxCore = node->getCpuTotal();
                return true;
            });
        return maxCore;
    };
    auto calcTotalRow = [&](unsigned singleRowCoreNum) {
        unsigned totalRow = 0;
        gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
                totalRow += std::ceil(static_cast<float>(node->getCpuTotal()) /
                                      static_cast<float>(singleRowCoreNum));
                return true;
            });
        return totalRow;
    };

    unsigned minRowCoreNum = 0;
    for (unsigned currRowCoreNum = calcMaxCoreNode(); currRowCoreNum != 0; --currRowCoreNum) {
        if (calcTotalRow(currRowCoreNum) <= yMax) {
            minRowCoreNum = currRowCoreNum;
        } else {
            break;
        }
    }

    return minRowCoreNum;
}

unsigned
LayoutCorePerf::calcMinRowMcrtComputation(const GlobalNodeInfo* gNodeInfo) const
{
    if (mRowCoreNum == 0) {
        return 0; // This is error then skip. wrong mRowCoreNum value
    }

    unsigned minRow = ~0;
    gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
            unsigned totalRow = std::ceil(static_cast<float>(node->getCpuTotal()) /
                                          static_cast<float>(mRowCoreNum));
            if (totalRow < minRow) minRow = totalRow;
            return true;
        });
    return minRow;
}

std::string
LayoutCorePerf::drawSingleNodeTitle(size_t mcrtTotal,
                                    std::shared_ptr<McrtNodeInfo> node,
                                    unsigned numOfRow) const
//
// single mcrt node left side title
//
{
    auto drawId = [&](size_t totalWidth, const std::string& additionalMsg) {
        auto idStr = [&]() {
            int w = scene_rdl2::str_util::getNumberOfDigits(mcrtTotal);
            std::ostringstream ostr;
            ostr << "Id:" << std::setw(w) << std::setfill('0') << node->getMachineId() << ' '
            << additionalMsg;
            return ostr.str();
        };
        std::string buff = idStr();

        std::ostringstream ostr;
        ostr << colFg({255,255,0}) << buff;
        for (size_t i = buff.size(); i < totalWidth; ++i) ostr << '-';
        ostr << colReset();
        return ostr.str();
    };
    auto addExtraLine = [&](int titleLines) {
        int extraLines = numOfRow - titleLines;
        std::ostringstream ostr;
        for (int i = 0; i < extraLines; ++i) ostr << "\n ";
        return ostr.str();
    };

    if (mMinRowMcrtComputation == 1) {
        // Special case : We only have single line space for title
        std::ostringstream ostr;
        ostr << drawId(0, "")
             << strSimpleHostName(node->getHostName())
             << " Cpu:" << node->getAssignedCpuTotal() << '/' << node->getCpuTotal()
             << ' ' << strPct(node->getCpuUsage())
             << addExtraLine(1);
        return ostr.str();

    } else if (mMinRowMcrtComputation == 2) {
        // Special case : We have 2 lines space for title
        std::ostringstream ostr;
        ostr << drawId(0, "")
             << "Cpu:" << node->getAssignedCpuTotal() << '/' << node->getCpuTotal()
             << " (" << strPct(node->getCpuUsage()) << ")\n"
             << strSimpleHostName(node->getHostName())
             << addExtraLine(2);
        return ostr.str();
    }
    
    //
    // We have 3 or more lines space for title
    //
    std::ostringstream ostr;
    ostr << strSimpleHostName(node->getHostName()) << '\n'
         << "Cpu:" << node->getAssignedCpuTotal() << '/' << node->getCpuTotal()
         << " (" << strPct(node->getCpuUsage()) << ')';
    int titleLines = 3; // id + hostName + Cpu
    int counter = 4;
    if (mMinRowMcrtComputation >= counter++) {
        ostr << "\nMem:" << strByte(node->getMemTotal()) << " (" << strPct(node->getMemUsage()) << ')';
        titleLines++;
    }
    if (mMinRowMcrtComputation >= counter++) {
        ostr << "\nActive:" << strBool(node->getRenderActive())
             << " Exec:" << strExecMode(node->getExecMode());
        titleLines++;
    }
    if (mMinRowMcrtComputation >= counter++) {
        ostr << "\nNET Recv:" << strBps(node->getNetRecvBps());
        titleLines++;
    }
    if (mMinRowMcrtComputation >= counter++) {
        ostr << "\nNET Send:" << strBps(node->getNetSendBps());
        titleLines++;
    }
    if (mMinRowMcrtComputation >= counter++) {
        ostr << "\nSend:" << strBps(node->getSendBps());
        titleLines++;
    }
    if (mMinRowMcrtComputation >= counter++) {
        ostr << "\nSnapshot:" << strMillisec(node->getSnapshotToSend());
        titleLines++;
    }
    if (mMinRowMcrtComputation >= counter++) {
        ostr << "\nProgress:" << strPct(node->getProgress()) << " /"
             << strPct(node->getGlobalProgress());
        titleLines++;
    }
    std::string partialTitle = ostr.str();

    ostr.str("");
    ostr << "SyncId:" << node->getSyncId() << ' ';

    std::ostringstream ostr2;
    ostr2 << drawId(Overlay::msgDisplayWidth(partialTitle), ostr.str()) << '\n'
          << partialTitle
          << addExtraLine(titleLines);
    return ostr2.str();
}

void
LayoutCorePerf::drawCorePerfSingleNode(std::shared_ptr<McrtNodeInfo> node,
                                       const McrtPos& mcrtPos)
{
    std::vector<float> coreUsage = node->getCoreUsage();
    std::sort(coreUsage.begin(), coreUsage.end(), std::greater<float>()); // reverse sort coreUsage

    constexpr int yHalfGap = 3;

    C3 c0 {255,255,0};
    C3 c1 {255,80,0};

    int coreId = 0;
    int yTop = mcrtPos.mMaxY + mcrtPos.mYStep;
    for (int yId = 0; yId < mcrtPos.mNumOfRows; ++yId) {
        int yBase = yTop - mcrtPos.mYStep * (yId + 1);
        int yMax = yBase + mcrtPos.mYStep - yHalfGap;
        int yMin = yBase + yHalfGap;
        int xLeft = mcrtPos.mCoreWinXMin;

        for (int xId = 0; xId < mcrtPos.mRowCoreNum; ++xId) {
            if (coreId >= coreUsage.size()) return; // finish display all cores

            int xMin = xLeft + (mcrtPos.mSingleCoreWidth + mcrtPos.mSingleCoreGapX) * xId;
            int xMax = xMin + mcrtPos.mSingleCoreWidth - 1;
            float fraction = coreUsage[coreId];
            if (fraction > 1.0f) fraction = 1.0f;
            int xOffset = static_cast<int>(static_cast<float>(xMax - xMin + 1) * fraction);

            mOverlay->drawBoxBar({scene_rdl2::math::Vec2i{xMin, yMin}, scene_rdl2::math::Vec2i{xMin + xOffset, yMax}},
                                 (fraction < 0.9f ? c0 : c1), 160);
            coreId++;
        }
    }
}

} // namespace telemetry
} // namespace mcrt_dataio
