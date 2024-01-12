// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "GlobalNodeInfo.h"

#include <mcrt_dataio/engine/mcrt/McrtControl.h>
#include <mcrt_dataio/share/util/MiscUtil.h>
#include <mcrt_dataio/share/util/ValueTimeTracker.h>

#include <scene_rdl2/render/util/StrUtil.h>
#include <scene_rdl2/render/util/TimeUtil.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>

// activate clock delta measurement logic.
#define DO_CLOCK_DELTA_MCRT

// show clock delta measurement related debug message 
#define DEBUG_MSG_CLOCK_DELTA

namespace mcrt_dataio {

GlobalNodeInfo::GlobalNodeInfo(bool decodeOnly,
                               float valueKeepDurationSec,
                               MsgSendHandlerShPtr msgSendHandler)
    : mValueKeepDurationSec(valueKeepDurationSec)
    , mInfoCodec("globalNodeInfo", decodeOnly)
    , mMsgSendHandler(msgSendHandler)
{
    parserConfigure();
    if (mValueKeepDurationSec > 0.0f) setupValueTimeTrackerMemory();
}

void
GlobalNodeInfo::reset()
{
    std::vector<float> coreFractions(mMergeCpuTotal, 0.0f);

    setClientNetRecvBps(0.0f);
    setClientNetSendBps(0.0f);

    setMergeCpuUsage(0.0f);
    setMergeCoreUsage(coreFractions);
    setMergeMemUsage(0.0f);
    setMergeNetRecvBps(0.0f);
    setMergeNetSendBps(0.0f);
    setMergeRecvBps(0.0f);
    setMergeSendBps(0.0f);
    setMergeProgress(0.0f);

    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr nodeInfo) {
            nodeInfo->reset();
            return true;
        });
}

//------------------------------------------------------------------------------------------

void
GlobalNodeInfo::setClientHostName(const std::string& hostName)
{
    mInfoCodec.setString("clientHostName", hostName, &mClientHostName);
}

void
GlobalNodeInfo::setClientClockTimeShift(const float ms) // millisec
{
    mInfoCodec.setFloat("clientClockTimeShift", ms, &mClientClockTimeShift);
}

void
GlobalNodeInfo::setClientRoundTripTime(const float ms)
{
    mInfoCodec.setFloat("clientRoundTripTime", ms, &mClientRoundTripTime);
}

void
GlobalNodeInfo::setClientCpuTotal(const int total)
{
    mInfoCodec.setInt("clientCpuTotal", total, &mClientCpuTotal);
}

void
GlobalNodeInfo::setClientCpuUsage(const float fraction)
{
    mInfoCodec.setFloat("clientCpuUsage", fraction, &mClientCpuUsage);
}

void
GlobalNodeInfo::setClientMemTotal(const size_t size)
{
    mInfoCodec.setSizeT("clientMemTotal", size, &mClientMemTotal);
}

void
GlobalNodeInfo::setClientMemUsage(const float fraction)
{
    mInfoCodec.setFloat("clientMemUsage", fraction, &mClientMemUsage);
}

void
GlobalNodeInfo::setClientNetRecvBps(const float bytesPerSec)
{
    mInfoCodec.setFloat("clientNetRecv", bytesPerSec, &mClientNetRecvBps);
    if (mClientNetRecvVtt) mClientNetRecvVtt->push(bytesPerSec);
}

void
GlobalNodeInfo::setClientNetSendBps(const float bytesPerSec)
{
    mInfoCodec.setFloat("clientNetSend", bytesPerSec, &mClientNetSendBps);
    if (mClientNetSendVtt) mClientNetSendVtt->push(bytesPerSec);
}

//------------------------------------------------------------------------------------------

void
GlobalNodeInfo::setDispatchHostName(const std::string& hostName)
{
    mInfoCodec.setString("dispatchHostName", hostName, &mDispatchHostName);
}

void
GlobalNodeInfo::setDispatchClockTimeShift(const float ms) // millisec
{
    mInfoCodec.setFloat("dispatchClockTimeShift", ms, &mDispatchClockTimeShift);
}

void
GlobalNodeInfo::setDispatchRoundTripTime(const float ms)
{
    mInfoCodec.setFloat("dispatchRoundTripTime", ms, &mDispatchRoundTripTime);
}

//------------------------------------------------------------------------------------------

void
GlobalNodeInfo::setMergeHostName(const std::string& hostName)
{
    mInfoCodec.setString("mergeHostName", hostName, &mMergeHostName);
}

void
GlobalNodeInfo::setMergeClockDeltaSvrPort(const int port)
{
    mInfoCodec.setInt("mergeClockDeltaSvrPort", port, &mMergeClockDeltaSvrPort);
}

void
GlobalNodeInfo::setMergeClockDeltaSvrPath(const std::string& path)
{
    mInfoCodec.setString("mergeClockDeltaSvrPath", path, &mMergeClockDeltaSvrPath);
}

void
GlobalNodeInfo::setMergeMcrtTotal(const int total)
{
    mInfoCodec.setInt("mergeMcrtTotal", total, &mMergeMcrtTotal);
}

void
GlobalNodeInfo::setMergeCpuTotal(const int total)
{
    mInfoCodec.setInt("mergeCpuTotal", total, &mMergeCpuTotal);
}

void
GlobalNodeInfo::setMergeAssignedCpuTotal(const int total)
{
    mInfoCodec.setInt("mergeAssignedCpuTotal", total, &mMergeAssignedCpuTotal);
}

void
GlobalNodeInfo::setMergeCpuUsage(const float fraction)
{
    mInfoCodec.setFloat("mergeCpuUsage", fraction, &mMergeCpuUsage);
}

void
GlobalNodeInfo::setMergeCoreUsage(const std::vector<float>& fractions)
{
    mInfoCodec.setVecFloat("mergeCoreUsage", fractions, &mMergeCoreUsage);
}

void
GlobalNodeInfo::setMergeMemTotal(const size_t size)
{
    mInfoCodec.setSizeT("mergeMemTotal", size, &mMergeMemTotal);
}

void
GlobalNodeInfo::setMergeMemUsage(const float fraction)
{
    mInfoCodec.setFloat("mergeMemUsage", fraction, &mMergeMemUsage);
}

void
GlobalNodeInfo::setMergeNetRecvBps(const float bytesPerSec)
{
    mInfoCodec.setFloat("mergeNetRecv", bytesPerSec, &mMergeNetRecvBps);
    if (mMergeNetRecvVtt) mMergeNetRecvVtt->push(bytesPerSec);
}

void
GlobalNodeInfo::setMergeNetSendBps(const float bytesPerSec)
{
    mInfoCodec.setFloat("mergeNetSend", bytesPerSec, &mMergeNetSendBps);
    if (mMergeNetSendVtt) mMergeNetSendVtt->push(bytesPerSec);
}

void
GlobalNodeInfo::setMergeRecvBps(const float bytesPerSec)
{
    mInfoCodec.setFloat("mergeRecvBps", bytesPerSec, &mMergeRecvBps);
}

void
GlobalNodeInfo::setMergeSendBps(const float bytesPerSec)
{
    mInfoCodec.setFloat("mergeSendBps", bytesPerSec, &mMergeSendBps);
}

void
GlobalNodeInfo::setMergeProgress(const float fraction)
{
    mInfoCodec.setFloat("mergeProgress", fraction, &mMergeProgress);
}

void
GlobalNodeInfo::setMergeFeedbackActive(const bool flag)
{
    mInfoCodec.setBool("mergeFeedbackActive", flag, &mMergeFeedbackActive);
}

void    
GlobalNodeInfo::setMergeFeedbackInterval(const float sec) // sec
{
    mInfoCodec.setFloat("mergeFeedbackInterval", sec, &mMergeFeedbackInterval);
}

void    
GlobalNodeInfo::setMergeEvalFeedbackTime(const float ms) // millisec
{
    mInfoCodec.setFloat("mergeEvalFeedbackTime", ms, &mMergeEvalFeedbackTime);
}

void
GlobalNodeInfo::setMergeSendFeedbackFps(const float fps) // fps
{
    mInfoCodec.setFloat("mergeSendFeedbackFps", fps, &mMergeSendFeedbackFps);
}

void    
GlobalNodeInfo::setMergeSendFeedbackBps(const float bytesPerSec) // Byte/Sec
{
    mInfoCodec.setFloat("mergeSendFeedbackBps", bytesPerSec, &mMergeSendFeedbackBps);
}

//------------------------------------------------------------------------------------------

int
GlobalNodeInfo::getMcrtTotalCpu() const
{
    int totalCpu = 0;
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) {
            totalCpu += mcrtNodeInfo->getCpuTotal();
            return true;
        });
    return totalCpu;
}

bool
GlobalNodeInfo::isMcrtAllStop() const
{
    return crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            return !mcrtNodeInfo->getRenderActive();
        });
}

bool
GlobalNodeInfo::isMcrtAllStart() const
{
    return crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            return mcrtNodeInfo->getRenderActive();
        });
}

bool
GlobalNodeInfo::isMcrtAllRenderPrepCompletedOrCanceled() const
{
    return crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            const scene_rdl2::grid_util::RenderPrepStats &renderPrepStats =
                mcrtNodeInfo->getRenderPrepStats();
            return renderPrepStats.isCompleted() || renderPrepStats.isCanceled();
        });
}

size_t
GlobalNodeInfo::getMaxMcrtHostName() const
{
    size_t size = 0;
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) {
            size_t currSize = mcrtNodeInfo->getHostName().size();
            if (size < currSize) size = currSize;
            return true;
        });
    return size;
}

bool
GlobalNodeInfo::crawlAllMcrtNodeInfo(std::function<bool(McrtNodeInfoShPtr)> func) const
{
    for (auto& itr : mMcrtNodeInfoMap) {
        McrtNodeInfoShPtr currPtr = itr.second;
        if (!func(currPtr)) return false; // early exit
    }
    return true;
}

bool
GlobalNodeInfo::accessMcrtNodeInfo(int mcrtId, std::function<bool(McrtNodeInfoShPtr)> func) const
{
    auto itr = mMcrtNodeInfoMap.find(mcrtId);
    if (itr == mMcrtNodeInfoMap.end()) return false;
    return func(itr->second);
}

//------------------------------------------------------------------------------------------

void
GlobalNodeInfo::enqMergeGenericComment(const std::string& comment) // MTsafe
{
    std::lock_guard<std::mutex> lock(mMergeGenericCommentMutex);
    if (!mMergeGenericComment.empty()) {
        // If genericComment is not flushed yet, we add newline here to easily understand
        // the separation of comments between old and new.
        mMergeGenericComment += '\n';
    }
    mMergeGenericComment += comment;
    if (mMergeGenericComment.back() == '\n') {
        // We have to remove all last '\n' from genericComment buffer
        while (!mMergeGenericComment.empty() && mMergeGenericComment.back() == '\n') {
            mMergeGenericComment.pop_back(); // remove last '\n'
        }
    }
}

std::string
GlobalNodeInfo::deqGenericComment() // MTsafe
//
// This API retrieve all the generic comments from all back-end engines.
//    
{
    std::string comment;
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            std::string genericComment = mcrtNodeInfo->deqGenericComment();
            if (!genericComment.empty()) {
                std::ostringstream ostr;
                ostr << ((!comment.empty()) ? "\n" : "")
                     << "genericComment"
                     << " (machineId:" << mcrtNodeInfo->getMachineId()
                     << " hostName:" << mcrtNodeInfo->getHostName() << ") {\n"
                     << scene_rdl2::str_util::addIndent(genericComment) << '\n'
                     << "}";
                comment += ostr.str();
            }
            return true;
        });

    {
        std::lock_guard<std::mutex> lock(mMergeGenericCommentMutex);

        if (!mMergeGenericComment.empty()) {
            std::ostringstream ostr;
            ostr << ((!comment.empty()) ? "\n" : "")
                 << "genericComment merge (hostName:" << mMergeHostName << ") {\n"
                 << scene_rdl2::str_util::addIndent(mMergeGenericComment) << '\n'
                 << "}";
            comment += ostr.str();

            mMergeGenericComment.clear();
            mMergeGenericComment.shrink_to_fit();
        }
    }
    
    return comment;
}

//------------------------------------------------------------------------------------------

bool
GlobalNodeInfo::encode(std::string& outputData)
{
    for (auto& itr : mMcrtNodeInfoMap) {
        McrtNodeInfoShPtr currPtr = itr.second;
        currPtr->flushEncodeData();
        mInfoCodec.encodeTable("mcrtNodeInfoMap",
                               std::to_string(currPtr->getMachineId()),
                               currPtr->getInfoCodec());
    }

    {
        //
        // flush data for mergeGenericComment
        //
        std::lock_guard<std::mutex> lock(mMergeGenericCommentMutex);
        if (!mMergeGenericComment.empty()) {
            mInfoCodec.setString("mergeGenericComment", mMergeGenericComment);
            mMergeGenericComment.clear();
            mMergeGenericComment.shrink_to_fit();
        }
    }

    return mInfoCodec.encode(outputData);
}

bool
GlobalNodeInfo::decode(const std::string& inputData)
{
    auto decodeMcrtNodeInfoMap = [&](int id, std::string& itemInfoData) -> bool {
        if (mMcrtNodeInfoMap.find(id) == mMcrtNodeInfoMap.end()) {
            mMcrtNodeInfoMap[id].reset(new McrtNodeInfo(mInfoCodec.getDecodeOnly(),
                                                        mValueKeepDurationSec));
#           ifdef DO_CLOCK_DELTA_MCRT
            sendClockDeltaClientMainToMcrt(id);
#           endif // end DO_CLOCK_DELTA_MCRT
        }
        return mMcrtNodeInfoMap[id]->decode(itemInfoData);
    };

    if (mInfoCodec.decode
        (inputData,
         [&]() {
            std::string str, itemKeyStr;
            float f;
            int i;
            size_t t;
            bool b;
            std::vector<float> vecF;
            if (mInfoCodec.getString("clientHostName", str)) {
                setClientHostName(str);
            } else if (mInfoCodec.getFloat("clientClockTimeShift", f)) {
                setClientClockTimeShift(f);
            } else if (mInfoCodec.getFloat("clientRoundTripTime", f)) {
                setClientRoundTripTime(f);
            } else if (mInfoCodec.getInt("clientCpuTotal", i)) {
                setClientCpuTotal(i);
            } else if (mInfoCodec.getFloat("clientCpuUsage", f)) {
                setClientCpuUsage(f);
            } else if (mInfoCodec.getSizeT("clientMemTotal", t)) {
                setClientMemTotal(t);
            } else if (mInfoCodec.getFloat("clientMemUsage", f)) {
                setClientMemUsage(f);
            } else if (mInfoCodec.getFloat("clientNetRecv", f)) {
                setClientNetRecvBps(f);
            } else if (mInfoCodec.getFloat("clientNetSend", f)) {
                setClientNetSendBps(f);

            } else if (mInfoCodec.getString("dispatchHostName", str)) {
                setDispatchHostName(str);
            } else if (mInfoCodec.getFloat("dispatchClockTimeShift", f)) {
                setDispatchClockTimeShift(f);
            } else if (mInfoCodec.getFloat("dispatchRoundTripTime", f)) {
                setDispatchRoundTripTime(f);

            } else if (mInfoCodec.getString("mergeHostName", str)) {
                setMergeHostName(str);
            } else if (mInfoCodec.getInt("mergeClockDeltaSvrPort", i)) {
                setMergeClockDeltaSvrPort(i);
            } else if (mInfoCodec.getString("mergeClockDeltaSvrPath", str)) {
                setMergeClockDeltaSvrPath(str);
            } else if (mInfoCodec.getInt("mergeMcrtTotal", i)) {
                setMergeMcrtTotal(i);
            } else if (mInfoCodec.getInt("mergeCpuTotal", i)) {
                setMergeCpuTotal(i);
            } else if (mInfoCodec.getInt("mergeAssignedCpuTotal", i)) {
                setMergeAssignedCpuTotal(i);
            } else if (mInfoCodec.getFloat("mergeCpuUsage", f)) {
                setMergeCpuUsage(f);
            } else if (mInfoCodec.getVecFloat("mergeCoreUsage", vecF)) {
                setMergeCoreUsage(vecF);
            } else if (mInfoCodec.getSizeT("mergeMemTotal", t)) {
                setMergeMemTotal(t);                
            } else if (mInfoCodec.getFloat("mergeMemUsage", f)) {
                setMergeMemUsage(f);
            } else if (mInfoCodec.getFloat("mergeNetRecv", f)) {
                setMergeNetRecvBps(f);
            } else if (mInfoCodec.getFloat("mergeNetSend", f)) {
                setMergeNetSendBps(f);
            } else if (mInfoCodec.getFloat("mergeRecvBps", f)) {
                setMergeRecvBps(f);
            } else if (mInfoCodec.getFloat("mergeSendBps", f)) {
                setMergeSendBps(f);
            } else if (mInfoCodec.getFloat("mergeProgress", f)) {
                setMergeProgress(f);
                
            } else if (mInfoCodec.getBool("mergeFeedbackActive", b)) {
                setMergeFeedbackActive(b);
            } else if (mInfoCodec.getFloat("mergeFeedbackInterval", f)) {
                setMergeFeedbackInterval(f);
            } else if (mInfoCodec.getFloat("mergeEvalFeedbackTime", f)) {
                setMergeEvalFeedbackTime(f);
            } else if (mInfoCodec.getFloat("mergeSendFeedbackFps", f)) {
                setMergeSendFeedbackFps(f);
            } else if (mInfoCodec.getFloat("mergeSendFeedbackBps", f)) {
                setMergeSendFeedbackBps(f);

            } else if (mInfoCodec.decodeTable("mcrtNodeInfoMap", itemKeyStr, str)) {
                return decodeMcrtNodeInfoMap(std::stoi(itemKeyStr), str);

            } else if (mInfoCodec.getString("mergeGenericComment", str)) {
                enqMergeGenericComment(str);
            }
            return true;
        }) == -1) {
        return false;           // parse error
    }
    return true;
}

bool
GlobalNodeInfo::decode(const std::vector<std::string>& inputDataArray)
{
    bool returnStatus = true;
    for (size_t i = 0; i < inputDataArray.size(); ++i) {
        // We should try to decode all data. 
        if (!decode(inputDataArray[i])) returnStatus = false;
    }
    return returnStatus;
}

bool
GlobalNodeInfo::clockDeltaClientMainAgainstMerge()
{
#   ifdef DEBUG_MSG_CLOCK_DELTA
    std::cerr << ">> GlobalNodeInfo.cc clockDeltaClientMainAgainstMerge()"
              << " clientHost:" << mClientHostName
              << " mergeHost:" << mMergeHostName << std::endl;
#   endif // end DEBUG_MSG_CLOCK_DELTA

    if (mClientHostName == mMergeHostName) {
        // This case client and merger are running on same host.
        // We don't need to do clockDelta measurement.
        return true;
    }

    return ClockDelta::clientMain(mMergeHostName,
                                  mMergeClockDeltaSvrPort,
                                  mMergeClockDeltaSvrPath,
                                  ClockDelta::NodeType::CLIENT);
}

bool
GlobalNodeInfo::setClockDeltaTimeShift(NodeType nodeType,
                                       const std::string& hostName,
                                       float clockDeltaTimeShift, // millisec
                                       float roundTripTime)       // millisec
{

    bool result = false;
    if (nodeType == ClockDelta::NodeType::CLIENT) {
        setClientHostName(hostName);
        setClientClockTimeShift(clockDeltaTimeShift);
        setClientRoundTripTime(roundTripTime);
        result = true;
#       ifdef DEBUG_MSG_CLOCK_DELTA
        std::cerr << ">> GlobalNodeInfo.cc setClockDeltaTimeShift() >>client<<"
                  << " hostName:" << hostName
                  << " shift:" << clockDeltaTimeShift
                  << " roundTrip:" << roundTripTime << std::endl;
#       endif // end DEBUG_MSG_CLOCK_DELTA

    } else if (nodeType == ClockDelta::NodeType::DISPATCH) {
        // We skip setDispatchHostName() here because we alerady have proper dispatch host name
        setDispatchClockTimeShift(clockDeltaTimeShift);
        setDispatchRoundTripTime(roundTripTime);
        result = true;
#       ifdef DEBUG_MSG_CLOCK_DELTA
        std::cerr << ">> GlobalNodeInfo.cc setClockDeltaTimeShift() >>dispatch<<"
                  << " hostName:" << hostName
                  << " shift:" << clockDeltaTimeShift
                  << " roundTrip:" << roundTripTime << std::endl;
#       endif // end DEBUG_MSG_CLOCK_DELTA

    } else {
        for (auto& itr : mMcrtNodeInfoMap) {
            McrtNodeInfoShPtr currPtr = itr.second;
            if (currPtr->setClockDeltaTimeShift(hostName, clockDeltaTimeShift, roundTripTime)) {
                result = true;
                sendClockOffsetToMcrt(currPtr);
#               ifdef DEBUG_MSG_CLOCK_DELTA
                std::cerr << ">> GlobalNodeInfo.cc setClockDeltaTimeShift() >>mcrt<<"
                          << " hostName:" << hostName
                          << " shift:" << clockDeltaTimeShift
                          << " roundTrip:" << roundTripTime << std::endl;
#               endif // end DEBUG_MSG_CLOCK_DELTA
                break;
            }
        }
    }
    return result;
}

unsigned
GlobalNodeInfo::getNewestBackEndSyncId() const
// This API should call by same thread of caller of decode()
// Return biggest syncId inside all back-end mcrt computation
{
    unsigned syncId = 0;
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            syncId = std::max(mcrtNodeInfo->getSyncId(), syncId);
            return true;
        });
    return syncId;
}

unsigned
GlobalNodeInfo::getOldestBackEndSyncId() const
// This API should call by same thread of caller of decode()
// Return smallest syncId inside all back-end mcrt computation
{
    unsigned syncId = std::numeric_limits<unsigned>::max();
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            syncId = std::min(mcrtNodeInfo->getSyncId(), syncId);
            return true;
        });
    return syncId;
}

float
GlobalNodeInfo::getRenderPrepProgress() const
// This API should call by same thread of caller of decode()
{
    unsigned latestSyncId = getNewestBackEndSyncId();

    unsigned maxTotalSteps = 0;
    unsigned currStepsAll = 0;
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            if (mcrtNodeInfo->getSyncId() == latestSyncId) { // we have to pick up latestSyncId data only
                const scene_rdl2::grid_util::RenderPrepStats &currRenderPrepStats =
                    mcrtNodeInfo->getRenderPrepStats();
                maxTotalSteps = std::max(maxTotalSteps, currRenderPrepStats.getTotalSteps());
                currStepsAll += currRenderPrepStats.getCurrSteps();
            }
            return true;
        });
    unsigned totalStepsAll = maxTotalSteps * static_cast<unsigned>(mMcrtNodeInfoMap.size());
    if (totalStepsAll == 0) return 0.0f; // special case
    return static_cast<float>(currStepsAll) / static_cast<float>(totalStepsAll);
}

GlobalNodeInfo::NodeStat
GlobalNodeInfo::getNodeStat() const
{
    bool renderPrepRun = false;
    bool renderPrepCancel = false;
    bool mcrt = false;

    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr nodeInfo) {
            switch (nodeInfo->getNodeStat()) {
            case NodeStat::IDLE : break;
            case NodeStat::RENDER_PREP_RUN : renderPrepRun = true; break;
            case NodeStat::RENDER_PREP_CANCEL : renderPrepCancel = true; break;
            case NodeStat::MCRT : mcrt = true; break;
            default : break;
            }
            return true;
        });

    if (renderPrepCancel) return NodeStat::RENDER_PREP_CANCEL;
    if (renderPrepRun) return NodeStat::RENDER_PREP_RUN;
    if (mcrt) return NodeStat::MCRT;
    return NodeStat::IDLE;
}

std::string
GlobalNodeInfo::show() const
{
    using scene_rdl2::str_util::addIndent;

    std::ostringstream ostr;
    ostr << "GlobalNodeInfo {\n"
         << addIndent(showClientInfo()) << '\n'
         << addIndent(showDispatchInfo()) << '\n'
         << addIndent(showMergeInfo()) << '\n'
         << addIndent(showAllNodeInfo()) << '\n'
         << "  getNodeStat():" << McrtNodeInfo::nodeStatStr(getNodeStat()) << '\n'
         << "}";
    return ostr.str();
}

std::string
GlobalNodeInfo::showRenderPrepStatus() const
{
    std::ostringstream ostr;
    ostr << "GlobalNodeInfo (total mcrt:" << mMcrtNodeInfoMap.size() << ' '
         << scene_rdl2::time_util::currentTimeStr() << ") {\n";
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            auto showInfo = [&]() -> std::string {
                std::ostringstream ostr;
                ostr
                << "mcrtNodeInfo ("
                << "machineId:" << mcrtNodeInfo->getMachineId() << ' '
                << "hostName:" << mcrtNodeInfo->getHostName() << ") {\n";
                ostr << "  syncId:" << mcrtNodeInfo->getSyncId() << '\n';
                ostr << scene_rdl2::str_util::addIndent(mcrtNodeInfo->getRenderPrepStats().show()) << '\n';
                ostr << "}";
                return ostr.str();
            };
            ostr << scene_rdl2::str_util::addIndent(showInfo()) << '\n';
            return true;
        });
    ostr << "}";
    return ostr.str();
}

std::string
GlobalNodeInfo::showAllHostsName() const
{
    auto getCpuTotal = [&]() -> size_t {
        size_t totalCpu = 0;
        crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
                totalCpu += mcrtNodeInfo->getCpuTotal();
                return true;
            });
        return totalCpu;
    };
    auto getMaxCpu = [&]() -> size_t {
        size_t maxCpu = 0;
        crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
                if (maxCpu < static_cast<size_t>(mcrtNodeInfo->getCpuTotal())) {
                    maxCpu = static_cast<size_t>(mcrtNodeInfo->getCpuTotal());
                }
                return true;
            });
        return maxCpu;
    };

    std::ostringstream ostr;
    ostr << "GlobalNodeInfo HostName {\n"
         << "  mClientHostName:" << mClientHostName << '\n'
         << "  mDispatchHostName:" << mDispatchHostName << '\n'
         << "  mMergeHostName:" << mMergeHostName << " mMergeCpuTotal:" << mMergeCpuTotal << '\n'
         << "  mcrt (totalMcrt:" << mMcrtNodeInfoMap.size() << " totalCpu:" << getCpuTotal() << ") {\n";
    if (mMcrtNodeInfoMap.size()) {
        int w0 = scene_rdl2::str_util::getNumberOfDigits(static_cast<unsigned>(mMcrtNodeInfoMap.size() - 1));
        int w1 = scene_rdl2::str_util::getNumberOfDigits(static_cast<unsigned>(getMaxCpu()));
        crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
                ostr << "    mMachineId:" << std::setw(w0) << mcrtNodeInfo->getMachineId()
                     << " mCpuTotal:" << std::setw(w1) << mcrtNodeInfo->getCpuTotal()
                     << " mHostName:" << mcrtNodeInfo->getHostName() << '\n';
                return true;
            });
    }
    ostr << "  }\n"
         << "}";
    return ostr.str();
}

//------------------------------------------------------------------------------------------

void
GlobalNodeInfo::setupValueTimeTrackerMemory()
{
    mClientNetRecvVtt = std::make_shared<ValueTimeTracker>(mValueKeepDurationSec);
    mClientNetSendVtt = std::make_shared<ValueTimeTracker>(mValueKeepDurationSec);

    mMergeNetRecvVtt = std::make_shared<ValueTimeTracker>(mValueKeepDurationSec);
    mMergeNetSendVtt = std::make_shared<ValueTimeTracker>(mValueKeepDurationSec);
}

void
GlobalNodeInfo::sendClockDeltaClientMainToMcrt(const int machineId)
//
// This function sends clockDeltaClient command to the mcrt computation
//
{
    if (!mMsgSendHandler) return;

    mMsgSendHandler->
        sendMessage(McrtControl::
                    msgGen_clockDeltaClient(machineId,
                                            mMergeHostName,
                                            mMergeClockDeltaSvrPort,
                                            mMergeClockDeltaSvrPath));
#   ifdef DEBUG_MSG_CLOCK_DELTA
    std::cerr << ">> GlobalNodeInfo.cc sendMessage "
              << McrtControl::msgGen_clockDeltaClient(machineId,
                                                      mMergeHostName,
                                                      mMergeClockDeltaSvrPort,
                                                      mMergeClockDeltaSvrPath)
              << '\n';
#   endif // end DEBUG_MSG_CLOCK_DELTA
}

void
GlobalNodeInfo::sendClockOffsetToMcrt(McrtNodeInfoShPtr mcrtNodeInfo)
//
// This function sends clockOffset command to the mcrt computation
//
{
    float offsetMs = -1.0f * mcrtNodeInfo->getClockTimeShift(); // millisec
    mMsgSendHandler->
        sendMessage(McrtControl::
                    msgGen_clockOffset(mcrtNodeInfo->getHostName(), offsetMs));
#   ifdef DEBUG_MSG_CLOCK_DELTA
    std::cerr << ">> GlobalNodeInfo.cc sendClockOffsetToMcrt() >"
              << McrtControl::msgGen_clockOffset(mcrtNodeInfo->getHostName(), offsetMs);
#   endif // end DEBUG_MSG_CLOCK_DELTA

    uint64_t currTimeUs = MiscUtil::getCurrentMicroSec();
    mcrtNodeInfo->setLastRunClockOffsetTime(currTimeUs); // update lastClockOffsetTime
#   ifdef DEBUG_MSG_CLOCK_DELTA
    std::cerr << ">> GlobalNodeInfo.cc sendClockOffsetToMcrt() currTimeUs:" << currTimeUs << std::endl;
#   endif // end DEBUG_MSG_CLOCK_DELTA
}

void
GlobalNodeInfo::parserConfigure()
{
    mParser.description("GlobalNodeInfo command");

    mParser.opt("mcrt", "<rankId> ...command...", "mcrt node info command",
                [&](Arg& arg) {
                    int rankId = (arg++).as<int>(0);
                    if (mMcrtNodeInfoMap.find(rankId) == mMcrtNodeInfoMap.end()) {
                        return arg.msg("rankId:" + std::to_string(rankId) + " is out of range\n");
                    } else {
                        arg.msg("rankId:" + std::to_string(rankId) + '\n');
                        return mMcrtNodeInfoMap[rankId]->getParser().main(arg.childArg());
                    }
                });
    mParser.opt("renderPrepStat", "", "show all node's renderPrep stat",
                [&](Arg& arg) { return arg.msg(showRenderPrepStatus() + '\n'); });
    mParser.opt("hostsName", "", "show all hostname info",
                [&](Arg& arg) { return arg.msg(showAllHostsName() + '\n'); });
    mParser.opt("clientInfo", "", "show client info",
                [&](Arg& arg) { return arg.msg(showClientInfo() + '\n'); });
    mParser.opt("clientNetRecvVtt", "...command...", "clientNetRecv valueTimeTracker command",
                [&](Arg& arg) {
                    if (!mClientNetRecvVtt) return arg.msg("mClientNetRecvVtt is empty\n");
                    else return mClientNetRecvVtt->getParser().main(arg.childArg());
                });
    mParser.opt("clientNetSendVtt", "...command...", "clientNetSend valueTimeTracker command",
                [&](Arg& arg) {
                    if (!mClientNetSendVtt) return arg.msg("mClientNetSendVtt is empty\n");
                    else return mClientNetSendVtt->getParser().main(arg.childArg());
                });
    mParser.opt("dispatchInfo", "", "show dispatch info",
                [&](Arg& arg) { return arg.msg(showDispatchInfo() + '\n'); });
    mParser.opt("mergeInfo", "", "show merge info",
                [&](Arg& arg) { return arg.msg(showMergeInfo() + '\n'); });
    mParser.opt("mergeNetRecvVtt", "...command...", "mergeNetRecv valueTimeTracker command",
                [&](Arg& arg) {
                    if (!mMergeNetRecvVtt) return arg.msg("mMergeNetRecvVtt is empty\n");
                    else return mMergeNetRecvVtt->getParser().main(arg.childArg());
                });
    mParser.opt("mergeNetSendVtt", "...command...", "mergeNetSend valueTimeTracker command",
                [&](Arg& arg) {
                    if (!mMergeNetSendVtt) return arg.msg("mMergeNetSendVtt is empty\n");
                    else return mMergeNetSendVtt->getParser().main(arg.childArg());
                });
    mParser.opt("mergeFeedbackInfo", "", "show merge feedback info",
                [&](Arg& arg) { return arg.msg(showMergeFeedbackInfo() + '\n'); });
    mParser.opt("allNodeInfo", "", "show all node info",
                [&](Arg& arg) { return arg.msg(showAllNodeInfo() + '\n'); });
    mParser.opt("nodeStat", "", "show current node status",
                [&](Arg& arg) { return arg.msg(McrtNodeInfo::nodeStatStr(getNodeStat()) + '\n'); });
    mParser.opt("feedbackAvg", "", "show feedback info of averaged about all mcrt computations",
                [&](Arg& arg) { return arg.msg(showFeedbackAvg() + '\n'); });
    mParser.opt("reset", "", "reset internal dynamic data",
                [&](Arg& arg) { reset(); return arg.msg("reset\n"); });
}

// static function
std::string
GlobalNodeInfo::msShow(float ms)
{
    std::ostringstream ostr;
    ostr << std::setw(7) << std::fixed << std::setprecision(2) << ms << " ms";
    return ostr.str();
}

// static function
std::string
GlobalNodeInfo::pctShow(float fraction)
{
    std::ostringstream ostr;
    ostr << std::setw(6) << std::fixed << std::setprecision(2) << fraction * 100.0f << " %";
    return ostr.str();
}

// static function
std::string
GlobalNodeInfo::bytesPerSecShow(float bytesPerSec)
{
    std::ostringstream ostr;
    ostr << scene_rdl2::str_util::byteStr((size_t)bytesPerSec) << "/sec";
    return ostr.str();
}

std::string
GlobalNodeInfo::showClientInfo() const
{
    std::ostringstream ostr;
    ostr << "client {\n"
         << "  mClientHostName:" << mClientHostName << '\n'
         << "  mClientClockTimeShift:" << msShow(mClientClockTimeShift) << '\n'
         << "  mClientRoundTripTime:" << msShow(mClientRoundTripTime) << '\n'
         << "  mCLientCpuTotal:" << mClientCpuTotal << '\n'
         << "  mClientCpuUsage:" << pctShow(mClientCpuUsage) << '\n'
         << "  mClientMemTotal:" << scene_rdl2::str_util::byteStr(mClientMemTotal) << '\n'
         << "  mClientMemUsage:" << pctShow(mClientMemUsage) << '\n'
         << "  mClientNetRecvBps:" << bytesPerSecShow(mClientNetRecvBps) << '\n'
         << "  mClientNetSendBps:" << bytesPerSecShow(mClientNetSendBps) << '\n'
         << "}";
    return ostr.str();
}

std::string
GlobalNodeInfo::showDispatchInfo() const
{
    std::ostringstream ostr;    
    ostr << "displatch {\n"
         << "  mDispatchHostName:" << mDispatchHostName << '\n'
         << "  mDispatchClockTimeShift:" << msShow(mDispatchClockTimeShift) << '\n'
         << "  mDispatchRoundTripTime:" << msShow(mDispatchRoundTripTime) << '\n'
         << "}";
    return ostr.str();
}
    
std::string
GlobalNodeInfo::showMergeInfo() const
{
    using scene_rdl2::str_util::addIndent;

    std::ostringstream ostr;
    ostr << "merge {\n"
         << "  mMergeHostName:" << mMergeHostName << '\n'
         << "  mMergeClockDeltaSvrPort:" << mMergeClockDeltaSvrPort << '\n'
         << "  mMergeClockDeltaSvrPath:" << mMergeClockDeltaSvrPath << '\n'
         << "  mMergeMcrtTotal:" << mMergeMcrtTotal << '\n'
         << "  mMergeCpuTotal:" << mMergeCpuTotal << '\n'
         << "  mMergeAssignedCpuTotal:" << mMergeAssignedCpuTotal << '\n'
         << "  mMergeCpuUsage:" << pctShow(mMergeCpuUsage) << '\n'
         << addIndent(showMergeCoreUsage()) << '\n'
         << "  mMergeMemTotal:" << scene_rdl2::str_util::byteStr(mMergeMemTotal) << '\n'
         << "  mMergeMemUsage:" << pctShow(mMergeMemUsage) << '\n'
         << "  mMergeNetRecvBps:" << bytesPerSecShow(mMergeNetRecvBps) << '\n'
         << "  mMergeNetSendBps:" << bytesPerSecShow(mMergeNetSendBps) << '\n'
         << "  mMergeRecvBps:" << bytesPerSecShow(mMergeRecvBps) << '\n'
         << "  mMergeSendBps:" << bytesPerSecShow(mMergeSendBps) << '\n'
         << "  mMergeProgress:" << pctShow(mMergeProgress) << '\n'
         << addIndent(showMergeFeedbackInfo()) << '\n'
         << "}";
    return ostr.str();
}
    
std::string
GlobalNodeInfo::showMergeCoreUsage() const
{
    int w = scene_rdl2::str_util::getNumberOfDigits(mMergeCoreUsage.size());

    std::ostringstream ostr;
    ostr << "mergeCoreUsage (coreTotal:" << mMergeCoreUsage.size() << ") {\n";
    for (size_t i = 0 ; i < mMergeCoreUsage.size(); ++i) {
        ostr << "  i:" << std::setw(w) << i << ' ' << pctShow(mMergeCoreUsage[i]) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
GlobalNodeInfo::showMergeFeedbackInfo() const
{
    using scene_rdl2::str_util::boolStr;

    std::ostringstream ostr;
    ostr << "mergeFeedback {\n"
         << "  mMergeFeedbackActive:" << boolStr(mMergeFeedbackActive) << '\n';
    if (mMergeFeedbackActive) {
        ostr << "  mMergeFeedbackInterval:" << mMergeFeedbackInterval << " sec\n"
             << "  mMergeEvalFeedbackTime:" << msShow(mMergeEvalFeedbackTime) << '\n'
             << "  mMergeSendFeedbackFps:" << mMergeSendFeedbackFps << '\n'
             << "  mMergeSendFeedbackBps:" << bytesPerSecShow(mMergeSendFeedbackBps) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
GlobalNodeInfo::showAllNodeInfo() const
{
    std::ostringstream ostr;    
    ostr << "mMcrtNodeInfoMap (total:" << mMcrtNodeInfoMap.size() << ") {\n";
    for (auto& itr : mMcrtNodeInfoMap) {
        const McrtNodeInfoShPtr currPtr = itr.second;
        ostr << scene_rdl2::str_util::addIndent(currPtr->show()) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
GlobalNodeInfo::showFeedbackAvg() const
{
    float sendBpsAll = 0.0f;
    float feedbackIntervalAll = 0.0f;
    float recvFeedbackFpsAll = 0.0f;
    float recvFeedbackBpsAll = 0.0f;
    float evalFeedbackTimeAll = 0.0f;
    float feedbackLatencyAll = 0.0f;
    
    unsigned totalNode = 0;
    unsigned activeNode = 0;
    crawlAllMcrtNodeInfo([&](McrtNodeInfoShPtr mcrtNodeInfo) -> bool {
            sendBpsAll += mcrtNodeInfo->getSendBps();
            if (mcrtNodeInfo->getFeedbackActive()) {
                feedbackIntervalAll += mcrtNodeInfo->getFeedbackInterval();
                recvFeedbackFpsAll += mcrtNodeInfo->getRecvFeedbackFps();
                recvFeedbackBpsAll += mcrtNodeInfo->getRecvFeedbackBps();
                evalFeedbackTimeAll += mcrtNodeInfo->getEvalFeedbackTime();
                feedbackLatencyAll += mcrtNodeInfo->getFeedbackLatency();
                activeNode++;
            }
            totalNode++;
            return true;
        });

    if (!totalNode) {
        return "empty";
    }
    if (!activeNode) {
        return "no active feedback node";
    }

    float sendBpsAvg = sendBpsAll / static_cast<float>(activeNode);
    float feedbackIntervalAvg = feedbackIntervalAll / static_cast<float>(activeNode);
    float recvFeedbackFpsAvg = recvFeedbackFpsAll / static_cast<float>(activeNode);
    float recvFeedbackBpsAvg = recvFeedbackBpsAll / static_cast<float>(activeNode);
    float evalFeedbackTimeAvg = evalFeedbackTimeAll / static_cast<float>(activeNode);
    float feedbackLatencyAvg = feedbackLatencyAll / static_cast<float>(activeNode);
    
    auto msShow = [](float ms) -> std::string {
        std::ostringstream ostr;
        ostr << std::setw(7) << std::fixed << std::setprecision(2) << ms << " ms";
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "feedback status average (feedackActiveNode:" << activeNode << " totalNode:" << totalNode << ") {\n"
         << "  sendBpsAvg:" << bytesPerSecShow(sendBpsAvg) << '\n'
         << "  feedbackIntervalAvg:" << scene_rdl2::str_util::secStr(feedbackIntervalAvg) << " sec\n"
         << "  recvFeedbackFpsAvg:" << recvFeedbackFpsAvg << '\n'
         << "  recvFeedbackBpsAvg:" << bytesPerSecShow(recvFeedbackBpsAvg) << '\n'
         << "  evalFeedbackTimeAvg:" << msShow(evalFeedbackTimeAvg) << '\n'
         << "  feedbackLatencyAvg:" << msShow(feedbackLatencyAvg) << '\n'
         << "}";
    return ostr.str();
}

} // namespace mcrt_dataio
