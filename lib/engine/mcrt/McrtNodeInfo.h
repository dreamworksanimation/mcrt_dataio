// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <mcrt_dataio/share/codec/InfoCodec.h>

#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/grid_util/RenderPrepStats.h>

#include <mutex>
#include <string>

namespace mcrt_dataio {

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

    enum class NodeStat : int { IDLE, RENDER_PREP_RUN, RENDER_PREP_CANCEL, MCRT };

    McrtNodeInfo(bool decodeOnly);

    InfoCodec &getInfoCodec() { return mInfoCodec; }

    void setHostName(const std::string &hostName); // MTsafe
    void setMachineId(const int id); // MTsafe
    void setCpuTotal(const int total); // MTsafe
    void setCpuUsage(const float fraction); // MTsafe
    void setMemTotal(const size_t size); // MTsafe : byte
    void setMemUsage(const float fraction); // MTsafe
    void setSnapshotToSend(const float ms); // MTsafe : millisec
    void setSendBps(const float bps); // MTsafe : Byte/Sec
    void setClockTimeShift(const float ms); // MTsafe : millisec
    void setRoundTripTime(const float ms); // MTsafe : millisec
    void setLastRunClockOffsetTime(const uint64_t us); // MTsafe : microsec
    void setSyncId(const unsigned id); // MTsafe
    void setRenderActive(const bool flag); // MTsafe
    void setRenderPrepCancel(const bool flag); // MTsafe
    void setRenderPrepStatsInit(); // MTsafe
    void setRenderPrepStats(const RenderPrepStats &rPrepStats); // MTsafe : for mcrt_computation
    void setRenderPrepStatsStage(const RenderPrepStats::Stage &stage); // MTsafe
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

    void enqGenericComment(const std::string &comment); // MTsafe

    const std::string &getHostName() const { return mHostName; }
    int getMachineId() const { return mMachineId; }
    int getCpuTotal() const { return mCpuTotal; }
    float getCpuUsage() const { return mCpuUsage; }
    size_t getMemTotal() const { return mMemTotal; }
    float getMemUsage() const { return mMemUsage; }
    float getSnapshotToSend() const { return mSnapshotToSend; }
    float getSendBps() const { return mSendBps; }
    float getClockTimeShift() const { return mClockTimeShift; } // millisec
    uint64_t getLastRunClockOffsetTime() const { return mLastRunClockOffsetTime; }
    unsigned getSyncId() const { return mSyncId; }
    bool getRenderActive() const { return mRenderActive; }
    bool getRenderPrepCancel() const { return mRenderPrepCancel; }
    const RenderPrepStats &getRenderPrepStats() const { return mRenderPrepStats; }
    RenderPrepStats &getRenderPrepStats() { return mRenderPrepStats; }
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

    std::string deqGenericComment(); // MTsafe

    void flushEncodeData(); // MTsafe
    bool decode(const std::string &inputData);

    bool setClockDeltaTimeShift(const std::string &hostName,
                                float clockDeltaTimeShift, // millisec
                                float roundTripTime); // millisec

    NodeStat getNodeStat() const;

    std::string show() const;
    static std::string nodeStatStr(const NodeStat& stat);

    Parser& getParser() { return mParser; }

private:
    void parserConfigure();

    std::string showTimeLog() const;

    //------------------------------

    std::string mHostName;
    int mMachineId;

    int mCpuTotal;              // CPU core total
    float mCpuUsage;            // fraction 0.0~1.0
    size_t mMemTotal;           // memory total byte
    float mMemUsage;            // fraction 0.0~1.0
    float mSnapshotToSend;      // millisec
    float mSendBps;             // outgoing message bandwidth Byte/Sec

    float mClockTimeShift;      // millisec
    float mRoundTripTime;       // millisec

    uint64_t mLastRunClockOffsetTime; // microsec from Epoch

    unsigned mSyncId;           // current processed syncId
    bool mRenderActive;         // true:rendering false:not-rendering
    bool mRenderPrepCancel;     // true:cancelActionOnGoing false:noCancelAction

    RenderPrepStats mRenderPrepStats;

    std::mutex mRenderPrepStatsMutex;
    bool mRenderPrepStatsLoadGeometriesRequestFlush;
    bool mRenderPrepStatsTessellationRequestFlush;
    RenderPrepStats mRenderPrepStatsWork; // work memory
    
    // related to initial image send back timing
    uint64_t mGlobalBaseFromEpoch;
    unsigned mTotalMsg; // total processed rdlMessage for current render frame
    float mOldestMessageRecvTiming; // sec from process start
    float mNewestMessageRecvTiming; // sec from process start
    float mRenderPrepStartTiming; // sec from process start
    float mRenderPrepEndTiming; // sec from process start
    float m1stSnapshotStartTiming; // sec from process start
    float m1stSnapshotEndTiming; // sec from process start
    float m1stSendTiming; // sec from process start

    float mProgress;            // render progress 0.0~1.0

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

