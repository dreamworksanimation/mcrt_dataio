// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mcrt_dataio/share/codec/InfoCodec.h>

#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/grid_util/RenderPrepStats.h>

#include <mutex>
#include <string>

namespace mcrt_dataio {

class ValueTimeTracker;

class McrtNodeInfo
//
// This class is used to keep backend progmcrt engine's node information.
// The encode to and decode from messages of this class is done by InfoCodec
// This object is located 2 places in the system. One is progmcrt and the other is
// a client.
// First of all, this object is updated inside the backend progmcrt engine and
// information is encoded to the message.
// This message is sent to the client via merge computation.
// On the client process, the messages are decoded and all the information is stored
// inside this object. Subsequent message decode operations overwrite previous
// information.
//
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;
    using RenderPrepStats = scene_rdl2::grid_util::RenderPrepStats;
    using ValueTimeTrackerShPtr = std::shared_ptr<ValueTimeTracker>;

    enum class NodeStat : int { IDLE, RENDER_PREP_RUN, RENDER_PREP_CANCEL, MCRT };
    enum class ExecMode : int { SCALAR, VECTOR, XPU, AUTO, UNKNOWN };

    McrtNodeInfo(bool decodeOnly, float valueKeepDurationSec);

    InfoCodec& getInfoCodec() { return mInfoCodec; }

    void reset();

    void setHostName(const std::string& hostName); // MTsafe
    void setMachineId(const int id); // MTsafe
    void setCpuTotal(const int total); // MTsafe
    void setAssignedCpuTotal(const int total); // MTsafe
    void setCpuUsage(const float fraction); // MTsafe
    void setCoreUsage(const std::vector<float>& fractions); // MTsafe
    void setMemTotal(const size_t size); // MTsafe : byte
    void setMemUsage(const float fraction); // MTsafe
    void setExecMode(const ExecMode& mode); // MTsafe
    void setSnapshotToSend(const float ms); // MTsafe : millisec
    void setNetRecvBps(const float bytesPerSec); // MTsafe : Byte/Sec (entire host)
    void setNetSendBps(const float bytesPerSec); // MTsafe : Byte/Sec (entire host)
    void setSendBps(const float bytesPerSec); // MTsafe : Byte/Sec (only from mcrt_computation)
    void setFeedbackActive(const bool flag); // MTsafe
    void setFeedbackInterval(const float sec); // MTsafe : sec
    void setRecvFeedbackFps(const float fps); // MTsafe : fps
    void setRecvFeedbackBps(const float bytesPerSec); // MTsafe : Byte/Sec
    void setEvalFeedbackTime(const float ms); // MTsafe : millisec
    void setFeedbackLatency(const float ms); // MTsafe : millisec
    void setClockTimeShift(const float ms); // MTsafe : millisec
    void setRoundTripTime(const float ms); // MTsafe : millisec
    void setLastRunClockOffsetTime(const uint64_t us); // MTsafe : microsec
    void setSyncId(const unsigned id); // MTsafe
    void setRenderActive(const bool flag); // MTsafe
    void setRenderPrepCancel(const bool flag); // MTsafe
    void setRenderPrepStatsInit(); // MTsafe
    void setRenderPrepStats(const RenderPrepStats& rPrepStats); // MTsafe : for mcrt_computation
    void setRenderPrepStatsStage(const RenderPrepStats::Stage& stage); // MTsafe
    void setRenderPrepStatsLoadGeometriesTotal(const int stageId, const int total); // MTsafe
    void setRenderPrepStatsLoadGeometriesProcessed(const int stageId, const int processed); // MTsafe
    void setRenderPrepStatsTessellationTotal(const int stageId, const int total); // MTsafe
    void setRenderPrepStatsTessellationProcessed(const int stageId, const int processed); // MTsafe
    void setGlobalBaseFromEpoch(const uint64_t ms); // MTsafe
    void setMsgRecvTotal(const unsigned total); // MTsafe
    void setOldestMsgRecvTiming(const float sec); // MTsafe
    void setNewestMsgRecvTiming(const float sec); // MTsafe
    void setRenderPrepStartTiming(const float sec); // MTsafe
    void setRenderPrepEndTiming(const float sec); // MTsafe
    void set1stSnapshotStartTiming(const float sec); // MTsafe
    void set1stSnapshotEndTiming(const float sec); // MTsafe
    void set1stSendTiming(const float sec); // MTsafe
    void setProgress(const float fraction); // MTsafe
    void setGlobalProgress(const float fraction); // MTsafe

    void enqGenericComment(const std::string& comment); // MTsafe

    const std::string& getHostName() const { return mHostName; }
    int getMachineId() const { return mMachineId; }
    int getCpuTotal() const { return mCpuTotal; }
    int getAssignedCpuTotal() const { return mAssignedCpuTotal; }
    float getCpuUsage() const { return mCpuUsage; } // fraction
    std::vector<float> getCoreUsage() const { return mCoreUsage; } // fraction
    size_t getMemTotal() const { return mMemTotal; }
    float getMemUsage() const { return mMemUsage; } // fraction
    ExecMode getExecMode() const { return static_cast<ExecMode>(mExecMode); }
    float getSnapshotToSend() const { return mSnapshotToSend; } // millisec
    float getNetRecvBps() const { return mNetRecvBps; } // byte/sec
    float getNetSendBps() const { return mNetSendBps; } // byte/sec
    float getSendBps() const { return mSendBps; } // byte/sec
    bool getFeedbackActive() const { return mFeedbackActive; }
    float getFeedbackInterval() const { return mFeedbackInterval; } // sec
    float getRecvFeedbackFps() const { return mRecvFeedbackFps; }
    float getRecvFeedbackBps() const { return mRecvFeedbackBps; } // Byte/sec
    float getEvalFeedbackTime() const { return mEvalFeedbackTime; } // millisec
    float getFeedbackLatency() const { return mFeedbackLatency; } // millisec
    float getClockTimeShift() const { return mClockTimeShift; } // millisec
    uint64_t getLastRunClockOffsetTime() const { return mLastRunClockOffsetTime; }
    unsigned getSyncId() const { return mSyncId; }
    bool getRenderActive() const { return mRenderActive; }
    bool getRenderPrepCancel() const { return mRenderPrepCancel; }
    const RenderPrepStats& getRenderPrepStats() const { return mRenderPrepStats; }
    RenderPrepStats& getRenderPrepStats() { return mRenderPrepStats; }
    uint64_t getGlobalBaseFromEpoch() const { return mGlobalBaseFromEpoch; }
    unsigned getTotalMsg() const { return mTotalMsg; }
    float getOldestMessageRecvTiming() const { return mOldestMessageRecvTiming; }
    float getNewestMessageRecvTiming() const { return  mNewestMessageRecvTiming; }
    float getRenderPrepStartTiming() const { return mRenderPrepStartTiming; }
    float getRenderPrepEndTiming() const { return mRenderPrepEndTiming; }
    float get1stSnapshotStartTiming() const { return m1stSnapshotStartTiming; }
    float get1stSnapshotEndTiming() const { return m1stSnapshotEndTiming; }
    float get1stSendTiming() const { return m1stSendTiming; }
    float getProgress() const { return mProgress; }
    float getGlobalProgress() const { return mGlobalProgress; }

    ValueTimeTrackerShPtr getNetRecvVtt() const { return mNetRecvVtt; }
    ValueTimeTrackerShPtr getNetSendVtt() const { return mNetSendVtt; }

    std::string deqGenericComment(); // MTsafe

    void flushEncodeData(); // MTsafe
    bool decode(const std::string& inputData);

    bool setClockDeltaTimeShift(const std::string& hostName,
                                float clockDeltaTimeShift, // millisec
                                float roundTripTime); // millisec

    NodeStat getNodeStat() const;

    std::string show() const;
    static std::string nodeStatStr(const NodeStat& stat);
    static std::string execModeStr(const ExecMode& mode);

    Parser& getParser() { return mParser; }

private:
    void setupValueTimeTrackerMemory();

    void parserConfigure();
    void resetCoreUsage();

    std::string showTimeLog() const;
    std::string showFeedback() const;
    std::string showCpuUsage() const;
    std::string showCoreUsage() const;
    std::string showDataIO() const;
    std::string showProgress() const;

    static std::string pctShow(float fraction);
    static std::string msShow(float ms);
    static std::string bytesPerSecShow(float bytesPerSec);

    //------------------------------

    float mValueKeepDurationSec {0.0f};

    std::string mHostName;
    int mMachineId {0};

    int mCpuTotal {0};             // CPU core total
    int mAssignedCpuTotal {0};     // assigned CPU core total
    float mCpuUsage {0.0f};        // fraction 0.0~1.0
    std::vector<float> mCoreUsage; // fraction 0.0~1.0
    size_t mMemTotal {0};          // memory total byte
    float mMemUsage {0.0f};        // fraction 0.0~1.0
    int mExecMode {static_cast<int>(ExecMode::SCALAR)};
    float mSnapshotToSend {0.0f};  // millisec
    float mNetRecvBps {0.0f};      // recv network bandwidth byte/sec (regarding entire host)
    float mNetSendBps {0.0f};      // send network bandwidth byte/sec (regarding entire host)
    float mSendBps {0.0f};         // outgoing message bandwidth Byte/Sec (only from mcrt_computation)

    ValueTimeTrackerShPtr mNetRecvVtt;
    ValueTimeTrackerShPtr mNetSendVtt;

    bool mFeedbackActive {false};   // feedback condition
    float mFeedbackInterval {0.0f}; // feedback interval : sec
    float mRecvFeedbackFps {0.0f};  // incoming feedback message receive fps
    float mRecvFeedbackBps {0.0f};  // incoming feedback message bandwidth : Byte/Sec
    float mEvalFeedbackTime {0.0f}; // feedback evaluation cost : millisec
    float mFeedbackLatency {0.0f};  // feedback latency : millisec

    float mClockTimeShift {0.0f}; // millisec
    float mRoundTripTime {0.0f};  // millisec

    uint64_t mLastRunClockOffsetTime {0}; // microsec from Epoch

    unsigned mSyncId {0};           // current processed syncId
    bool mRenderActive {false};     // true:rendering false:not-rendering
    bool mRenderPrepCancel {false}; // true:cancelActionOnGoing false:noCancelAction

    RenderPrepStats mRenderPrepStats;

    std::mutex mRenderPrepStatsMutex;
    bool mRenderPrepStatsLoadGeometriesRequestFlush {false};
    bool mRenderPrepStatsTessellationRequestFlush {false};
    RenderPrepStats mRenderPrepStatsWork; // work memory
    
    // related to initial image send back timing
    uint64_t mGlobalBaseFromEpoch {0};
    unsigned mTotalMsg {0};                // total processed rdlMessage for current render frame
    float mOldestMessageRecvTiming {0.0f}; // sec from process start
    float mNewestMessageRecvTiming {0.0f}; // sec from process start
    float mRenderPrepStartTiming {0.0f};   // sec from process start
    float mRenderPrepEndTiming {0.0f};     // sec from process start
    float m1stSnapshotStartTiming {0.0f};  // sec from process start
    float m1stSnapshotEndTiming {0.0f};    // sec from process start
    float m1stSendTiming {0.0f};           // sec from process start

    float mProgress {0.0f}; // render progress 0.0~1.0
    float mGlobalProgress {0.0f}; // global render progress 0.0~1.0

    // For the genericComment, we use different logic from another regular member for infoCodec operation.
    // The biggest difference between genericComment and other items in this object is that genericComment
    // fields might be added information multiple times instead of overwriting data.
    // This is a reason for the flush operation required to properly encode genericComment data.
    std::mutex mGenericCommentMutex;
    std::string mGenericComment; // generic comment data for any purpose

    InfoCodec mInfoCodec;

    Parser mParser;
};

} // namespace mcrt_dataio
