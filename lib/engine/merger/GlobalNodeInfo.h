// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
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

    void reset();

    //------------------------------
    
    void setClientHostName(const std::string& hostName); // MTsafe
    void setClientClockTimeShift(const float ms); // MTsafe : millisec
    void setClientRoundTripTime(const float ms); // MTsafe : millisec
    float getClientClockTimeShift() const { return mClientClockTimeShift; } // millisec
    
    //------------------------------
    
    void setDispatchHostName(const std::string& hostName); // MTsafe
    void setDispatchClockTimeShift(const float ms); // MTsafe : millisec
    void setDispatchRoundTripTime(const float ms); // MTsafe : millisec
    const std::string& getDispatchHostName() const { return mDispatchHostName; }
    float getDispatchClockTimeShift() const { return mDispatchClockTimeShift; } // millisec

    //------------------------------
    
    void setMergeHostName(const std::string& hostName); // MTsafe
    void setMergeClockDeltaSvrPort(const int port); // MTsafe
    void setMergeClockDeltaSvrPath(const std::string& path); // MTsafe : unix-domain ipc path
    void setMergeCpuTotal(const int total); // MTsafe
    void setMergeCpuUsage(const float fraction); // MTsafe fraction 0.0~1.0
    void setMergeCoreUsage(const std::vector<float>& fractions); // MTsafe fraction 0.0~1.0
    void setMergeMemTotal(const size_t size); // MTsafe byte
    void setMergeMemUsage(const float fraction); // MTsafe fraction 0.0~1.0
    void setMergeNetRecvBps(const float bytesPerSec); // MTsafe Byte/Sec
    void setMergeNetSendBps(const float bytesPerSec); // MTsafe Byte/Sec
    void setMergeRecvBps(const float bytesPerSec); // MTsafe Byte/Sec
    void setMergeSendBps(const float bytesPerSec); // MTsafe Byte/Sec
    void setMergeProgress(const float fraction); // MTsafe

    void setMergeFeedbackActive(const bool flag); // MTsafe
    void setMergeFeedbackInterval(const float sec); // MTsafe sec
    void setMergeEvalFeedbackTime(const float ms); // MTsafe millisec
    void setMergeSendFeedbackFps(const float fps); // MTsafe fps
    void setMergeSendFeedbackBps(const float bytesPerSec); // MTsafe Byte/Sec

    const std::string& getMergeHostName() const { return mMergeHostName; }
    int getMergeClockDeltaSvrPort() const { return mMergeClockDeltaSvrPort; }
    const std::string& getMergeClockDeltaSvrPath() const { return mMergeClockDeltaSvrPath; }
    int getMergeCpuTotal() const { return mMergeCpuTotal; }
    float getMergeCpuUsage() const { return mMergeCpuUsage; } // fraction
    std::vector<float> getMergeCoreUsage() const { return mMergeCoreUsage; } // fraction    
    size_t getMergeMemTotal() const { return mMergeMemTotal; }
    float getMergeMemUsage() const { return mMergeMemUsage; }
    float getMergeNetRecvBps() const { return mMergeNetRecvBps; } // byte/sec
    float getMergeNetSendBps() const { return mMergeNetSendBps; } // byte/sec
    float getMergeRecvBps() const { return mMergeRecvBps; }
    float getMergeSendBps() const { return mMergeSendBps; }
    float getMergeProgress() const { return mMergeProgress; }

    bool getMergeFeedbackActive() const { return mMergeFeedbackActive; }
    float getMergeFeedbackInterval() const { return mMergeFeedbackInterval; } // sec
    float getMergeEvalFeedbackTime() const { return mMergeEvalFeedbackTime; } // millisec
    float getMergeSendFeedbackFps() const { return mMergeSendFeedbackFps; } // fps
    float getMergeSendFeedbackBps() const { return mMergeSendFeedbackBps; } // Byte/Sec

    //------------------------------

    size_t getMcrtTotal() const { return mMcrtNodeInfoMap.size(); }
    int getMcrtTotalCpu() const;
    bool isMcrtAllStop() const;
    bool isMcrtAllStart() const;
    bool isMcrtAllRenderPrepCompletedOrCanceled() const;
    bool crawlAllMcrtNodeInfo(std::function<bool(McrtNodeInfoShPtr)> func) const;
    bool accessMcrtNodeInfo(int mcrtId, std::function<bool(McrtNodeInfoShPtr)> func) const;

    //------------------------------

    void enqMergeGenericComment(const std::string& comment); // MTsafe
    std::string deqGenericComment(); // MTsafe

    bool encode(std::string& outputData); // MTsafe
    bool decode(const std::string& inputData);
    bool decode(const std::vector<std::string>& inputDataArray);

    bool clockDeltaClientMainAgainstMerge();
    bool setClockDeltaTimeShift(NodeType nodeType,
                                const std::string& hostName,
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
    float mClientClockTimeShift {0.0f}; // millisec
    float mClientRoundTripTime {0.0f};  // millisec

    //------------------------------
    //
    // dispatcher information
    //
    std::string mDispatchHostName;
    float mDispatchClockTimeShift {0.0f}; // millisec
    float mDispatchRoundTripTime {0.0f};  // millisec

    //------------------------------
    //
    // merge information
    //
    std::string mMergeHostName;
    int mMergeClockDeltaSvrPort {0};
    std::string mMergeClockDeltaSvrPath; // unix-domain ipc path
    int mMergeCpuTotal {0};              // CPU core total
    float mMergeCpuUsage {0.0f};         // fraction 0.0~1.0
    std::vector<float> mMergeCoreUsage;  // fraction 0.0~1.0    
    size_t mMergeMemTotal {0};           // memory total byte
    float mMergeMemUsage {0.0f};         // fraction 0.0~1.0
    float mMergeNetRecvBps {0.0f};       // recv network bandwidth byte/sec (regarding entire host)
    float mMergeNetSendBps {0.0f};       // send network bandwidth byte/sec (regarding entire host)
    float mMergeRecvBps {0.0f};          // merge incoming message bandwidth Byte/Sec
    float mMergeSendBps {0.0f};          // merge outgoing message bandwidth Byte/Sec
    float mMergeProgress {0.0f};         // progress 0.0~1.0 range might be more than 1.0

    bool mMergeFeedbackActive {false};   // merge computation feedback condition : bool
    float mMergeFeedbackInterval {0.0f}; // merge computation feedback interval : sec
    float mMergeEvalFeedbackTime {0.0f}; // merge computation feedback evaluation cost : millisec
    float mMergeSendFeedbackFps {0.0f};  // merge computation outgoing feedback message send fps
    float mMergeSendFeedbackBps {0.0f};  // merge computation outgoing feedback message bandwidth : Byte/Sec

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

    static std::string msShow(float ms);
    static std::string pctShow(float fraction);
    static std::string bytesPerSecShow(float bytesPerSec);

    std::string showClientInfo() const;
    std::string showDispatchInfo() const;
    std::string showMergeInfo() const;
    std::string showMergeCoreUsage() const;
    std::string showMergeFeedbackInfo() const;
    std::string showAllNodeInfo() const;
    std::string showFeedbackAvg() const;
};

} // namespace mcrt_dataio
