// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

namespace mcrt_dataio {
namespace telemetry {

void
LayoutFeedback::drawMain(const DisplayInfo& info)
{
    subPanelTitle(info);
    subPanelGlobalInfo(10, mBBoxTitle.lower.y - 10 - mStepPixY, info, mBBoxGlobalInfo);
    drawGlobalProgressBar(info);
    drawMergeComputation(info);
    drawMcrtComputation(info);
}

void    
LayoutFeedback::drawGlobalProgressBar(const DisplayInfo& info)
{
    unsigned gapWidth = 10;
    unsigned barLeftBottomX = mBBoxGlobalInfo.upper.x + gapWidth;
    unsigned barLeftBottomY = mBBoxTitle.lower.y - 10 - mStepPixY;
    unsigned barWidth = mOverlay->getWidth() - barLeftBottomX - gapWidth;
    subPanelGlobalProgressBar(barLeftBottomX, barLeftBottomY, barWidth, info, mBBoxGlobalProgressBar);
}

void
LayoutFeedback::drawMergeComputation(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;
    if (gNodeInfo->getMcrtTotal() == 1) return;

    std::ostringstream ostr;
    ostr << colReset() << colFg({255,255,0}) << "MERGE: " << colReset()
         << gNodeInfo->getMergeHostName()
         << " Progress:" << strPct(gNodeInfo->getMergeProgress())
         << " Cpu:" << gNodeInfo->getMergeAssignedCpuTotal() << '/' << gNodeInfo->getMergeCpuTotal()
         << " (" << strPct(gNodeInfo->getMergeCpuUsage()) << ")"
         << " Mem:" << strByte(gNodeInfo->getMergeMemTotal()) << " (" << strPct(gNodeInfo->getMergeMemUsage()) << ")"
         << " NetRecv:" << strBps(gNodeInfo->getMergeNetRecvBps())
         << " Recv:" << strBps(gNodeInfo->getMergeRecvBps())
         << " NetSend:" << strBps(gNodeInfo->getMergeNetSendBps())
         << " Send:" << strBps(gNodeInfo->getMergeSendBps()) << '\n';
    ostr << "       "
         << "Feedback:" << strBool(gNodeInfo->getMergeFeedbackActive())
         << " Intvl:" << strSec(gNodeInfo->getMergeFeedbackInterval())
         << " Eval:" << strMillisec(gNodeInfo->getMergeEvalFeedbackTime())
         << " SendFps:" << strFps(gNodeInfo->getMergeSendFeedbackFps())
         << " SendBps:" << strBps(gNodeInfo->getMergeSendFeedbackBps());
    auto calcTopY = [&]() {
        return std::min(mBBoxGlobalInfo.lower.y, mBBoxGlobalProgressBar.lower.y);
    };

    subPanelMessage(10, // x
                    calcTopY() - 10 - mStepPixY, // y
                    ostr.str(),
                    mBBoxMergeComputation);
}

void
LayoutFeedback::drawMcrtComputation(const DisplayInfo& info)
{
    const GlobalNodeInfo* gNodeInfo = info.mGlobalNodeInfo;
    if (!gNodeInfo) return;
    if (gNodeInfo->getMcrtTotal() == 1) return;

    int hostNameW = gNodeInfo->getMaxMcrtHostName();
    auto drawNode = [&](std::shared_ptr<McrtNodeInfo> node) {
        int w = scene_rdl2::str_util::getNumberOfDigits(gNodeInfo->getMcrtTotal());

        const scene_rdl2::grid_util::RenderPrepStats& renderPrepStats = node->getRenderPrepStats();
        float renderPrepProgress = (static_cast<float>(renderPrepStats.getCurrSteps()) /
                                    static_cast<float>(renderPrepStats.getTotalSteps()));

        std::ostringstream ostr;
        ostr
        << colReset()
        << std::setw(hostNameW) << std::setfill(' ') << node->getHostName()
        << " Syc:" << node->getSyncId()
        << " Id:" << std::setw(w) << std::setfill('0') << node->getMachineId() << ' '
        << " Cpu:" << node->getAssignedCpuTotal() << '/' << node->getCpuTotal()
        << "(" << strPct(node->getCpuUsage()) << ")"
        << " Mem:" << strByte(node->getMemTotal()) << "(" << strPct(node->getMemUsage()) << ")"
        << " Act:" << strBool(node->getRenderActive())
        << " Exc:" << strExecMode(node->getExecMode())
        << " Prep:" << strPct(renderPrepProgress)
        << " Prg:" << strPct(node->getProgress()) << '/' << strPct(node->getGlobalProgress())
        << " Snp:" << strMillisec(node->getSnapshotToSend())
        << " NetRcv:" << strBps(node->getNetRecvBps())
        << " NetSnd:" << strBps(node->getNetSendBps())
        << " Send:" << strBps(node->getSendBps()) << '\n';
        ostr
        << std::setw(hostNameW) << std::setfill(' ') << ' '
        << " Feedback:" << strBool(node->getFeedbackActive())
        << " Intvl:" << strSec(node->getFeedbackInterval())
        << " RcvFps:" << strFps(node->getRecvFeedbackFps())
        << " RcvBps:" << strBps(node->getRecvFeedbackBps())
        << " Eval:" << strMillisec(node->getEvalFeedbackTime())
        << " Latcy:" << strMillisec(node->getFeedbackLatency());
        
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << colReset() << colFg({255,255,0}) << "MCRT " << colReset()
         << " (totalMcrt:" << gNodeInfo->getMcrtTotal()
         << " totalCpu:" << gNodeInfo->getMcrtTotalCpu()
         << " isAllStop:" << strBool(gNodeInfo->isMcrtAllStop())
         << " isAllStart:" << strBool(gNodeInfo->isMcrtAllStart())
         << " isAllFinishRenderPrep:" << strBool(gNodeInfo->isMcrtAllRenderPrepCompletedOrCanceled())
         << ") {\n";
    gNodeInfo->crawlAllMcrtNodeInfo([&](std::shared_ptr<McrtNodeInfo> node) {
            ostr << scene_rdl2::str_util::addIndent(drawNode(node)) << '\n';
            return true;
        });
    ostr << "}";

    subPanelMessage(10, // x
                    mBBoxMergeComputation.lower.y - 10 - mStepPixY, // y
                    ostr.str(),
                    mBBoxMcrtComputation);
}

} // namespace telemetry
} // namespace mcrt_dataio
