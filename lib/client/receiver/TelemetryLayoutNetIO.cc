// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

namespace mcrt_dataio {
namespace telemetry {

void
LayoutNetIO::drawMain(const DisplayInfo& info)
{
    subPanelTitle(info);
    setupPanelPosition(info);
    subPanelGlobalInfo(10, mBBoxTitle.lower.y - 10 - mStepPixY, info, mBBoxGlobalInfo);
    drawGlobalProgressBar(info);
    drawClient(info);
    drawMerge(info);
    drawMCRT(info);
}

//------------------------------------------------------------------------------------------

void
LayoutNetIO::parserConfigure()
{
    mParser.opt("mcrtTotalOW", "<mcrtTotal|show>", "mcrtTotal overwrite value for debug",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else mMcrtTotalOverwrite = (arg++).as<int>(0);
                    return arg.msg(std::to_string(mMcrtTotalOverwrite) + '\n');
                });
}

void
LayoutNetIO::setupPanelPosition(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;

    int mcrtTotal = getMcrtTotal(gNodeInfo);
    if (mcrtTotal <= 4) {
        mPanelCountX = 1;
        mPanelCountY = 4;
    } else if (mcrtTotal <= 8) {
        mPanelCountX = 2;
        mPanelCountY = 4;
    } else if (mcrtTotal <= 10) {
        mPanelCountX = 2;
        mPanelCountY = 5;
    } else if (mcrtTotal <= 15) {
        mPanelCountX = 3;
        mPanelCountY = 5;
    } else if (mcrtTotal <= 18) {
        mPanelCountX = 3;
        mPanelCountY = 6;
    } else if (mcrtTotal <= 24) {
        mPanelCountX = 4;
        mPanelCountY = 6;
    } else if (mcrtTotal <= 30) {
        mPanelCountX = 5;
        mPanelCountY = 6;
    } else { // mcrtTotal > 30
        // More than 36 nodes is not crash but only display machineId 0 ~ 35 in the display area.
        // MachineId 36 or later node display outside of the current display window.
        // We don't have scroll overlay functionality yet.
        mPanelCountX = 6;
        mPanelCountY = 6;
    }
    mPanelCountX += 2;

    unsigned width = mOverlay->getWidth() - mGapX * 2;
    unsigned currY = mBBoxTitle.lower.y;

    unsigned height = currY - mGapY * 2;
    mPanelWidth = (width - mGapX * (mPanelCountX - 1)) / mPanelCountX;
    mPanelHeight = (height - mGapY * (mPanelCountY - 1)) / mPanelCountY;

    mPanelTopY = currY - mGapY;
    mPanelCenterY = currY / 2;
    mPanelMcrtLeftX = mGapX * 3 + mPanelWidth * 2;

    mBpsGraphMax = 118.0 * 1024.0 * 1024.0;
    mBpsRulerYSize = 5 - (mPanelCountY - 4); // heuristic solution
}

int
LayoutNetIO::getMcrtTotal(const GlobalNodeInfo* gNodeInfo) const
{
    int mcrtTotal = gNodeInfo->getMergeMcrtTotal();
    if (mMcrtTotalOverwrite > 0) {
        mcrtTotal = mMcrtTotalOverwrite;
    }
    return std::max(1, mcrtTotal); // just in case
}

void    
LayoutNetIO::drawGlobalProgressBar(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;
    
    if (gNodeInfo->getMergeMcrtTotal() == 0) return; // early exit

    constexpr unsigned gapLeftX = 10;
    constexpr unsigned gapRightX = 5;
    unsigned barLeftBottomX = gapLeftX;
    unsigned barLeftBottomY = mBBoxGlobalInfo.lower.y - 10 - mStepPixY;
    unsigned barWidth = mPanelMcrtLeftX - (gapLeftX + gapRightX);

    subPanelGlobalProgressBar(barLeftBottomX, barLeftBottomY, barWidth, info, mBBoxGlobalProgressBar);
}

void
LayoutNetIO::drawClient(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;

    unsigned minX = mGapX;
    unsigned maxX = minX + mPanelWidth;
    unsigned minY = mPanelCenterY - mPanelHeight / 2;
    unsigned maxY = minY + mPanelHeight;

    std::ostringstream ostr;
    ostr << strSimpleHostName(gNodeInfo->getClientHostName()) << " ==CLIENT==";
    if (info.mClientMessage && !info.mClientMessage->empty()) {
        ostr << ' ' << *info.mClientMessage;
    }

    subPanelNetIOCpuMemAndProgress(minX, minY, maxX, maxY,
                                   mBpsGraphMax,
                                   mBpsRulerYSize,
                                   ostr.str(),
                                   gNodeInfo->getClientCpuTotal(),
                                   gNodeInfo->getClientCpuUsage(),
                                   gNodeInfo->getClientMemTotal(),
                                   gNodeInfo->getClientMemUsage(),
                                   -1.0f,
                                   -1.0f,
                                   -1.0f,
                                   *gNodeInfo->getClientNetSendVtt(),
                                   *gNodeInfo->getClientNetRecvVtt(),
                                   true,
                                   mBBoxClient);
}

void
LayoutNetIO::drawMerge(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;

    if (gNodeInfo->getMergeHostName().empty()) {
        return; // merge information is still empty
    }
    
    unsigned minX = mBBoxClient.upper.x + mGapX;
    unsigned maxX = minX + mPanelWidth;
    unsigned minY = mBBoxClient.lower.y;
    unsigned maxY = mBBoxClient.upper.y;

    std::ostringstream ostr;
    ostr << strSimpleHostName(gNodeInfo->getMergeHostName());
    if (gNodeInfo->getMergeHostName() == gNodeInfo->getDispatchHostName()) {
        ostr << " ==DISPATCH/MERGE==";
    } else {
        ostr << " ==MERGE==";
    }

    subPanelNetIOCpuMemAndProgress(minX, minY, maxX, maxY,
                                   mBpsGraphMax,
                                   mBpsRulerYSize,
                                   ostr.str(),
                                   gNodeInfo->getMergeCpuTotal(),
                                   gNodeInfo->getMergeCpuUsage(),
                                   gNodeInfo->getMergeMemTotal(),
                                   gNodeInfo->getMergeMemUsage(),
                                   -1.0f,
                                   -1.0f,
                                   -1.0f,
                                   *gNodeInfo->getMergeNetSendVtt(),
                                   *gNodeInfo->getMergeNetRecvVtt(),
                                   true,
                                   mBBoxMerge);
}

void
LayoutNetIO::drawMCRT(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;

    size_t mcrtTotal = getMcrtTotal(gNodeInfo);
    mBBoxMcrt.resize(mcrtTotal);

    auto computeMcrtPanelPosition = [&](int machineId,
                                        unsigned& minX, unsigned& minY, unsigned& maxX, unsigned& maxY) {
        unsigned yId = machineId % mPanelCountY;
        unsigned xId = machineId / mPanelCountY;
        minX = xId * (mPanelWidth + mGapX) + mPanelMcrtLeftX;
        maxX = minX + mPanelWidth;
        maxY = mPanelTopY - yId * (mPanelHeight + mGapY);
        minY = maxY - mPanelHeight;
    };

    auto drawNode = [&](std::shared_ptr<McrtNodeInfo> node) {
        int machineId = node->getMachineId();

        unsigned minX, minY, maxX, maxY;
        computeMcrtPanelPosition(machineId, minX, minY, maxX, maxY);

        std::ostringstream ostr;
        ostr << strSimpleHostName(node->getHostName()) << " ==MCRT-" << machineId
        << "== syncId:" << node->getSyncId();

        const scene_rdl2::grid_util::RenderPrepStats& renderPrepStats = node->getRenderPrepStats();
        float renderPrepProgress = (static_cast<float>(renderPrepStats.getCurrSteps()) /
                                    static_cast<float>(renderPrepStats.getTotalSteps()));

        bool activeBgFlag = (node->getSyncId() == info.mFrameId);

        subPanelNetIOCpuMemAndProgress(minX, minY, maxX, maxY,
                                       mBpsGraphMax,
                                       mBpsRulerYSize,
                                       ostr.str(), // title
                                       node->getCpuTotal(),
                                       node->getCpuUsage(),
                                       node->getMemTotal(),
                                       node->getMemUsage(),
                                       renderPrepProgress,
                                       node->getProgress(),
                                       node->getGlobalProgress(),
                                       *node->getNetSendVtt(),
                                       *node->getNetRecvVtt(),
                                       activeBgFlag,
                                       mBBoxMcrt[machineId]);
    };

    gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
            drawNode(node); return true;
        });
}

} // namespace telemetry
} // namespace mcrt_dataio
