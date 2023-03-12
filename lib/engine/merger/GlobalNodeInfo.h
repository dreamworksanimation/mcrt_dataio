// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include "MsgSendHandler.h"

#include <mcrt_dataio/engine/mcrt/McrtNodeInfo.h>
#include <mcrt_dataio/share/codec/InfoCodec.h>
#include <mcrt_dataio/share/util/ClockDelta.h>

#include <scene_rdl2/common/grid_util/Parser.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mcrt_dataio {

class GlobalNodeInfo
//
// This class keeps all nodes (dispatch, mcrt, merge and client) runtime statistical information.
// This information is updated during the whole session.
// All information is encoded by infoCodec and sent to the client by using auxInfo channel of
// progressiveFrame message. Client application decodes data and updates GlobalNodeInfo inside
// ClientReceiverFb. Merge computation send GlobalNodeInfo multiple times during entire session
// to the client and client can update GlobalNodeInfo with close to real time (very short latency)
//
{
public:
    using McrtNodeInfoShPtr = std::shared_ptr<McrtNodeInfo>;
    using MsgSendHandlerShPtr = std::shared_ptr<MsgSendHandler>;
    using NodeType = ClockDelta::NodeType;
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;
    using NodeStat = McrtNodeInfo::NodeStat;

    GlobalNodeInfo(bool decodeOnly, MsgSendHandlerShPtr msgSendHandler);

    //------------------------------
    
    void setClientHostName(const std::string &hostName); // MTsafe
    void setClientClockTimeShift(const float ms); // MTsafe : millisec
    void setClientRoundTripTime(const float ms); // MTsafe : millisec
    float getClientClockTimeShift() const { return mClientClockTimeShift; } // millisec
    
    //------------------------------
    
    void setDispatchHostName(const std::string &hostName); // MTsafe
    void setDispatchClockTimeShift(const float ms); // MTsafe : millisec
    void setDispatchRoundTripTime(const float ms); // MTsafe : millisec
    const std::string &getDispatchHostName() const { return mDispatchHostName; }

    //------------------------------
    
    void setMergeHostName(const std::string &hostName); // MTsafe
    void setMergeClockDeltaSvrPort(const int port); // MTsafe
    void setMergeClockDeltaSvrPath(const std::string &path); // MTsafe : unix-domain ipc path
    void setMergeCpuTotal(const int total); // MTsafe
    void setMergeCpuUsage(const float fraction); // MTsafe fraction 0.0~1.0
    void setMergeMemTotal(const size_t size); // MTsafe byte
    void setMergeMemUsage(const float fraction); // MTsafe fraction 0.0~1.0
    void setMergeRecvBps(const float bps); // MTsafe Byte/Sec
    void setMergeSendBps(const float bps); // MTsafe Byte/Sec
    void setMergeProgress(const float fraction); // MTsafe

    const std::string &getMergeHostName() const { return mMergeHostName; }
    int getMergeClockDeltaSvrPort() const { return mMergeClockDeltaSvrPort; }
    const std::string &getMergeClockDeltaSvrPath() const { return mMergeClockDeltaSvrPath; }
    int getMergeCpuTotal() const { return mMergeCpuTotal; }
    float getMergeCpuUsage() const { return mMergeCpuUsage; }
    size_t getMergeMemTotal() const { return mMergeMemTotal; }
    float getMergeMemUsage() const { return mMergeMemUsage; }
    float getMergeRecvBps() const { return mMergeRecvBps; }
    float getMergeSendBps() const { return mMergeSendBps; }
    float getMergeProgress() const { return mMergeProgress; }

    //------------------------------

    size_t getMcrtTotal() const { return mMcrtNodeInfoMap.size(); }
    bool isMcrtAllStop() const;
    bool isMcrtAllStart() const;
    bool isMcrtAllRenderPrepCompletedOrCanceled() const;
    bool crawlAllMcrtNodeInfo(std::function<bool(McrtNodeInfoShPtr)> func) const;
    bool accessMcrtNodeInfo(int mcrtId, std::function<bool(McrtNodeInfoShPtr)> func) const;

    //------------------------------

    void enqMergeGenericComment(const std::string &comment); // MTsafe
    std::string deqGenericComment(); // MTsafe

    bool encode(std::string &outputData); // MTsafe
    bool decode(const std::string &inputData);
    bool decode(const std::vector<std::string> &inputDataArray);

    bool clockDeltaClientMainAgainstMerge();
    bool setClockDeltaTimeShift(NodeType nodeType,
                                const std::string &hostName,
                                float clockDeltaTimeShift, // millisec
                                float roundTripTime); // millisec

    unsigned getNewestBackEndSyncId() const; // return biggest syncId inside all back-end mcrt computation
    unsigned getOldestBackEndSyncId() const; // return smallest syncId inside all back-end mcrt computation

    float getRenderPrepProgress() const; // return fraction

    NodeStat getNodeStat() const;

    std::string show() const;
    std::string showRenderPrepStatus() const;
    std::string showAllHostsName() const;

    Parser& getParser() { return mParser; }

private:
    //------------------------------
    //
    // client information
    //
    std::string mClientHostName;
    float mClientClockTimeShift; // millisec
    float mClientRoundTripTime;  // millisec

    //------------------------------
    //
    // dispatcher information
    //
    std::string mDispatchHostName;
    float mDispatchClockTimeShift; // millisec
    float mDispatchRoundTripTime;  // millisec

    //------------------------------
    //
    // merge information
    //
    std::string mMergeHostName;
    int mMergeClockDeltaSvrPort;
    std::string mMergeClockDeltaSvrPath; // unix-domain ipc path
    int mMergeCpuTotal;                  // CPU core total
    float mMergeCpuUsage;                // fraction 0.0~1.0
    size_t mMergeMemTotal;               // memory total byte
    float mMergeMemUsage;                // fraction 0.0~1.0
    float mMergeRecvBps;                 // merge incoming message bandwidth Byte/Sec
    float mMergeSendBps;                 // merge outgoing message bandwidth Byte/Sec
    float mMergeProgress;                // progress 0.0~1.0 range might be more than 1.0

    std::mutex mMergeGenericCommentMutex;
    std::string mMergeGenericComment; // merge computation's generic comment data for any purpose

    //------------------------------
    //
    // mcrt computation information
    //
    std::unordered_map<int, McrtNodeInfoShPtr> mMcrtNodeInfoMap; // int = machineId

    //------------------------------

    InfoCodec mInfoCodec;
    MsgSendHandlerShPtr mMsgSendHandler; // generic message send handler

    Parser mParser;

    //------------------------------

    void sendClockDeltaClientMainToMcrt(const int machineId);
    void sendClockOffsetToMcrt(McrtNodeInfoShPtr nodeInfo);

    void parserConfigure();
};

} // namespace mcrt_dataio

