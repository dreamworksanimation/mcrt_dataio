// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "McrtNodeInfo.h"

#include <mcrt_dataio/share/util/MiscUtil.h>
#include <mcrt_dataio/share/util/ValueTimeTracker.h>
#include <scene_rdl2/render/util/StrUtil.h>

#include <iomanip>
#include <sstream>

namespace mcrt_dataio {

McrtNodeInfo::McrtNodeInfo(bool decodeOnly, float valueKeepDurationSec)
    : mValueKeepDurationSec(valueKeepDurationSec)
    , mInfoCodec("mcrtNodeInfo", decodeOnly)
{
    parserConfigure();
    if (mValueKeepDurationSec > 0.0f) setupValueTimeTrackerMemory();
}

void
McrtNodeInfo::reset()
{
    std::vector<float> coreFractions(mCpuTotal, 0.0f);

    setCpuUsage(0.0f);
    setCoreUsage(coreFractions);
    setMemUsage(0.0f);
    setSnapshotToSend(0.0f);
    setNetRecvBps(0.0f);
    setNetSendBps(0.0f);
    setSendBps(0.0f);
    setFeedbackActive(false);
    setFeedbackInterval(0.0f);
    setRecvFeedbackFps(0.0f);
    setRecvFeedbackBps(0.0f);
    setEvalFeedbackTime(0.0f);
    setFeedbackLatency(0.0f);
    setRenderActive(false);
    setRenderPrepCancel(false);
    setRenderPrepStatsInit();
    setProgress(0.0f);
    setGlobalProgress(0.0f);
}
    
void
McrtNodeInfo::setHostName(const std::string& hostName)
{
    mInfoCodec.setString("hostName", hostName, &mHostName);
}

void    
McrtNodeInfo::setMachineId(const int id)
{
    mInfoCodec.setInt("machineId", id, &mMachineId);
}

void
McrtNodeInfo::setCpuTotal(const int total)
{
    mInfoCodec.setInt("cpuTotal", total, &mCpuTotal);

    mCoreUsage.resize(mCpuTotal);
    for (size_t i = 0; i < mCpuTotal; ++i) mCoreUsage[i] = 0.0f;
}

void
McrtNodeInfo::setAssignedCpuTotal(const int total)
{
    mInfoCodec.setInt("assignedCpuTotal", total, &mAssignedCpuTotal);
}

void
McrtNodeInfo::setCpuUsage(const float fraction)
{
    mInfoCodec.setFloat("cpuUsage", fraction, &mCpuUsage);
}

void
McrtNodeInfo::setCoreUsage(const std::vector<float>& fractions)
{
    mInfoCodec.setVecFloat("coreUsage", fractions, &mCoreUsage);
}

void
McrtNodeInfo::setMemTotal(const size_t size) // byte
{
    mInfoCodec.setSizeT("memTotal", size, &mMemTotal);
}

void
McrtNodeInfo::setMemUsage(const float fraction)
{
    mInfoCodec.setFloat("memUsage", fraction, &mMemUsage);
}

void
McrtNodeInfo::setExecMode(const ExecMode& mode)
{
    mInfoCodec.setInt("execMode", static_cast<int>(mode), &mExecMode);
}

void
McrtNodeInfo::setSnapshotToSend(const float ms) // millisec
{
    mInfoCodec.setFloat("snapshotToSend", ms, &mSnapshotToSend);
}

void
McrtNodeInfo::setNetRecvBps(const float bytesPerSec) // byte/sec
{
    mInfoCodec.setFloat("netRecv", bytesPerSec, &mNetRecvBps);
    if (mNetRecvVtt) mNetRecvVtt->push(bytesPerSec);
}

void
McrtNodeInfo::setNetSendBps(const float bytesPerSec) // byte/sec
{
    mInfoCodec.setFloat("netSend", bytesPerSec, &mNetSendBps);
    if (mNetSendVtt) mNetSendVtt->push(bytesPerSec);
}

void
McrtNodeInfo::setSendBps(const float bytesPerSec) // byte/sec
{
    mInfoCodec.setFloat("sendBps", bytesPerSec, &mSendBps);
}

void
McrtNodeInfo::setFeedbackActive(const bool flag)
{
    mInfoCodec.setBool("feedbackActive", flag, &mFeedbackActive);
}

void
McrtNodeInfo::setFeedbackInterval(const float sec) // sec
{
    mInfoCodec.setFloat("feedbackInterval", sec, &mFeedbackInterval);
}

void
McrtNodeInfo::setRecvFeedbackFps(const float fps) // fps
{
    mInfoCodec.setFloat("recvFeedbackFps", fps, &mRecvFeedbackFps);
}

void
McrtNodeInfo::setRecvFeedbackBps(const float bytesPerSec) // byte/sec
{
    mInfoCodec.setFloat("recvFeedbackBps", bytesPerSec, &mRecvFeedbackBps);
}

void
McrtNodeInfo::setEvalFeedbackTime(const float ms) // millisec
{
    mInfoCodec.setFloat("evalFeedbackTime", ms, &mEvalFeedbackTime);
}

void
McrtNodeInfo::setFeedbackLatency(const float ms) // millisec
{
    mInfoCodec.setFloat("feedbackLatency", ms, &mFeedbackLatency);
}

void
McrtNodeInfo::setClockTimeShift(const float ms) // millisec
{
    mInfoCodec.setFloat("clockTimeShift", ms, &mClockTimeShift);
}

void
McrtNodeInfo::setRoundTripTime(const float ms) // millisec
{
    mInfoCodec.setFloat("roundTripTime", ms, &mRoundTripTime);
}

void
McrtNodeInfo::setLastRunClockOffsetTime(const uint64_t us) // microsec
{
    mInfoCodec.setUInt64("lastRunClockOffsetTime", us, &mLastRunClockOffsetTime);
}

void
McrtNodeInfo::setSyncId(const unsigned id)
{
    mInfoCodec.setUInt("syncId", id, &mSyncId);
}

void
McrtNodeInfo::setRenderActive(const bool flag)
{
    mInfoCodec.setBool("renderActive", flag, &mRenderActive);
}

void
McrtNodeInfo::setRenderPrepCancel(const bool flag)
{
    mInfoCodec.setBool("renderPrepCancel", flag, &mRenderPrepCancel);
}

void
McrtNodeInfo::setRenderPrepStatsInit()
{
    setRenderPrepStatsStage(RenderPrepStats::Stage::NOT_ACTIVE);
}

void
McrtNodeInfo::setRenderPrepStats(const RenderPrepStats& rPrepStats)
//
// for mcrt_computation
//
{
    using Stage = RenderPrepStats::Stage;

    const Stage& stage = rPrepStats.stage();

    if (Stage::GM_LOADGEO0_START <= stage && stage <= Stage::GM_LOADGEO0_DONE_CANCELED) {
        // Geometry Manager loadGeometry phase : we have to update geometry manager's loadGeometry detail info
        std::lock_guard<std::mutex> lock(mRenderPrepStatsMutex);

        if (stage == Stage::GM_LOADGEO0_START || stage == Stage::GM_LOADGEO0_START_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsLoadGeometriesTotal(0, rPrepStats.loadGeometriesTotal(0));
            mRenderPrepStatsLoadGeometriesRequestFlush = false;
        } else if (stage == Stage::GM_LOADGEO0_PROCESS) {
            // Following values will be flushed later
            mRenderPrepStatsWork.stage() = stage;
            mRenderPrepStatsWork.loadGeometriesProcessed(0) = rPrepStats.loadGeometriesProcessed(0);
            mRenderPrepStatsLoadGeometriesRequestFlush = true;
        } else if (stage == Stage::GM_LOADGEO0_DONE || stage == Stage::GM_LOADGEO0_DONE_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsLoadGeometriesProcessed(0, rPrepStats.loadGeometriesProcessed(0));
            mRenderPrepStatsLoadGeometriesRequestFlush = false;
        }

    } else if (Stage::GM_LOADGEO1_START <= stage && stage <= Stage::GM_LOADGEO1_DONE_CANCELED) {
        // Geometry Manager loadGeometry phase : we have to update geometry manager's loadGeometry detail info
        std::lock_guard<std::mutex> lock(mRenderPrepStatsMutex);

        if (stage == Stage::GM_LOADGEO1_START || stage == Stage::GM_LOADGEO1_START_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsLoadGeometriesTotal(1, rPrepStats.loadGeometriesTotal(1));
            mRenderPrepStatsLoadGeometriesRequestFlush = false;
        } else if (stage == Stage::GM_LOADGEO1_PROCESS) {
            // Following values will be flushed later
            mRenderPrepStatsWork.stage() = stage;
            mRenderPrepStatsWork.loadGeometriesProcessed(1) = rPrepStats.loadGeometriesProcessed(1);
            mRenderPrepStatsLoadGeometriesRequestFlush = true;
        } else if (stage == Stage::GM_LOADGEO1_DONE || stage == Stage::GM_LOADGEO1_DONE_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsLoadGeometriesProcessed(1, rPrepStats.loadGeometriesProcessed(1));
            mRenderPrepStatsLoadGeometriesRequestFlush = false;
        }

    } else if (Stage::GM_FINALIZE0_TESSELLATION <= stage &&
               stage <= Stage::GM_FINALIZE0_TESSELLATION_DONE_CANCELED) {
        // Geometry Manager tessellation phase : we have to update geometry manager's tessellation detail info
        std::lock_guard<std::mutex> lock(mRenderPrepStatsMutex);
        
        if (stage == Stage::GM_FINALIZE0_TESSELLATION || stage == Stage::GM_FINALIZE0_TESSELLATION_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsTessellationTotal(0, rPrepStats.tessellationTotal(0));
            mRenderPrepStatsTessellationRequestFlush = false;
        } else if (stage == Stage::GM_FINALIZE0_TESSELLATION_PROCESS) {
            // Following values will be flushed later
            mRenderPrepStatsWork.stage() = stage;
            mRenderPrepStatsWork.tessellationProcessed(0) = rPrepStats.tessellationProcessed(0);
            mRenderPrepStatsTessellationRequestFlush = true;
        } else if (stage == Stage::GM_FINALIZE0_TESSELLATION_DONE ||
                   stage == Stage::GM_FINALIZE0_TESSELLATION_DONE_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsTessellationProcessed(0, rPrepStats.tessellationProcessed(0));
            mRenderPrepStatsTessellationRequestFlush = false;
        }

    } else if (Stage::GM_FINALIZE1_TESSELLATION <= stage &&
               stage <= Stage::GM_FINALIZE1_TESSELLATION_DONE_CANCELED) {
        // Geometry Manager tessellation phase : we have to update geometry manager's tessellation detail info
        std::lock_guard<std::mutex> lock(mRenderPrepStatsMutex);
        
        if (stage == Stage::GM_FINALIZE1_TESSELLATION || stage == Stage::GM_FINALIZE1_TESSELLATION_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsTessellationTotal(1, rPrepStats.tessellationTotal(1));
            mRenderPrepStatsTessellationRequestFlush = false;
        } else if (stage == Stage::GM_FINALIZE1_TESSELLATION_PROCESS) {
            // Following values will be flushed later
            mRenderPrepStatsWork.stage() = stage;
            mRenderPrepStatsWork.tessellationProcessed(1) = rPrepStats.tessellationProcessed(1);
            mRenderPrepStatsTessellationRequestFlush = true;
        } else if (stage == Stage::GM_FINALIZE1_TESSELLATION_DONE ||
                   stage == Stage::GM_FINALIZE1_TESSELLATION_DONE_CANCELED) {
            // We do flush now
            setRenderPrepStatsStage(stage);
            setRenderPrepStatsTessellationProcessed(1, rPrepStats.tessellationProcessed(1));
            mRenderPrepStatsTessellationRequestFlush = false;
        }

    } else {
        setRenderPrepStatsStage(stage);
    }
}

void
McrtNodeInfo::setRenderPrepStatsStage(const RenderPrepStats::Stage& stage)
{
    mInfoCodec.setUInt("renderPrepStatsStage",
                       static_cast<unsigned int>(stage),
                       reinterpret_cast<unsigned int *>(&mRenderPrepStats.stage()));
}

void
McrtNodeInfo::setRenderPrepStatsLoadGeometriesTotal(const int stageId, const int total)
{
    if (stageId == 0) {
        mInfoCodec.setInt("renderPrepStatsLoadGeoTotal0",
                          total,
                          &mRenderPrepStats.loadGeometriesTotal(stageId));
    } else {
        mInfoCodec.setInt("renderPrepStatsLoadGeoTotal1",
                          total,
                          &mRenderPrepStats.loadGeometriesTotal(stageId));
    }
}

void
McrtNodeInfo::setRenderPrepStatsLoadGeometriesProcessed(const int stageId, const int processed)
{
    if (stageId == 0) {
        mInfoCodec.setInt("renderPrepStatsLoadGeoProcessed0",
                          processed,
                          &mRenderPrepStats.loadGeometriesProcessed(stageId));
    } else {
        mInfoCodec.setInt("renderPrepStatsLoadGeoProcessed1",
                          processed,
                          &mRenderPrepStats.loadGeometriesProcessed(stageId));
    }
}

void
McrtNodeInfo::setRenderPrepStatsTessellationTotal(const int stageId, const int total)
{
    if (stageId == 0) {
        mInfoCodec.setInt("renderPrepStatsTessellationTotal0",
                          total,
                          &mRenderPrepStats.tessellationTotal(stageId));
    } else {
        mInfoCodec.setInt("renderPrepStatsTessellationTotal1",
                          total,
                          &mRenderPrepStats.tessellationTotal(stageId));
    }
}

void
McrtNodeInfo::setRenderPrepStatsTessellationProcessed(const int stageId, const int processed)
{
    if (stageId == 0) {
        mInfoCodec.setInt("renderPrepStatsTessellationProcessed0",
                          processed,
                          &mRenderPrepStats.tessellationProcessed(stageId));
    } else {
        mInfoCodec.setInt("renderPrepStatsTessellationProcessed1",
                          processed,
                          &mRenderPrepStats.tessellationProcessed(stageId));
    }
}

void
McrtNodeInfo::setGlobalBaseFromEpoch(const uint64_t us)
{
    mInfoCodec.setUInt64("globalBaseFromEpoch", us, &mGlobalBaseFromEpoch);
}

void
McrtNodeInfo::setMsgRecvTotal(const unsigned total)
{
    mInfoCodec.setUInt("totalMsg", total, &mTotalMsg);
}

void
McrtNodeInfo::setOldestMsgRecvTiming(const float sec)
{    
    mInfoCodec.setFloat("oldestMsg", sec, &mOldestMessageRecvTiming);
}
    
void
McrtNodeInfo::setNewestMsgRecvTiming(const float sec)
{    
    mInfoCodec.setFloat("newestMsg", sec, &mNewestMessageRecvTiming);
}

void
McrtNodeInfo::setRenderPrepStartTiming(const float sec)
{
    mInfoCodec.setFloat("renderPrepStart", sec, &mRenderPrepStartTiming);
}

void
McrtNodeInfo::setRenderPrepEndTiming(const float sec)
{
    mInfoCodec.setFloat("renderPrepEnd", sec, &mRenderPrepEndTiming);
}

void
McrtNodeInfo::set1stSnapshotStartTiming(const float sec)
{
    mInfoCodec.setFloat("snapshot1stStart", sec, &m1stSnapshotStartTiming);
}

void
McrtNodeInfo::set1stSnapshotEndTiming(const float sec)
{
    mInfoCodec.setFloat("snapshot1stEnd", sec, &m1stSnapshotEndTiming);
}

void
McrtNodeInfo::set1stSendTiming(const float sec)
{
    mInfoCodec.setFloat("send1st", sec, &m1stSendTiming);
}

void
McrtNodeInfo::setProgress(const float fraction)
{
    mInfoCodec.setFloat("progress", fraction, &mProgress);
}

void
McrtNodeInfo::setGlobalProgress(const float fraction)
{
    mInfoCodec.setFloat("globalProgress", fraction, &mGlobalProgress);
}

void
McrtNodeInfo::enqGenericComment(const std::string& comment)
{
    std::lock_guard<std::mutex> lock(mGenericCommentMutex);
    if (!mGenericComment.empty()) {
        // If genericComment is not flushed yet, we add newline here to easily understand
        // the separation of comments between old and new.
        mGenericComment += '\n';
    }
    mGenericComment += comment;
    if (mGenericComment.back() == '\n') {
        // We have to remove all last '\n' from genericComment buffer
        while (!mGenericComment.empty() && mGenericComment.back() == '\n') {
            mGenericComment.pop_back(); // remove last '\n'
        }
    }
}

std::string
McrtNodeInfo::deqGenericComment()
{
    std::lock_guard<std::mutex> lock(mGenericCommentMutex);

    std::string str = mGenericComment;
    mGenericComment.clear();
    mGenericComment.shrink_to_fit();

    return str;
}

void
McrtNodeInfo::flushEncodeData()
{
    {
        using Stage = RenderPrepStats::Stage;
        std::lock_guard<std::mutex> lock(mRenderPrepStatsMutex);

        if (mRenderPrepStatsLoadGeometriesRequestFlush) {
            //
            // flush data for renderPrepStats loadGeometry
            //
            RenderPrepStats::Stage flushStage = mRenderPrepStatsWork.stage();
            setRenderPrepStatsStage(flushStage);

            if (flushStage == Stage::GM_LOADGEO0_PROCESS) {
                setRenderPrepStatsLoadGeometriesProcessed
                    (0,
                     mRenderPrepStatsWork.loadGeometriesProcessed(0));
            } else if (flushStage == Stage::GM_LOADGEO1_PROCESS) {
                setRenderPrepStatsLoadGeometriesProcessed
                    (1,
                     mRenderPrepStatsWork.loadGeometriesProcessed(1));
            }
            mRenderPrepStatsLoadGeometriesRequestFlush = false;

        } else if (mRenderPrepStatsTessellationRequestFlush) {
            //
            // flush data for renderPrepStats tessellation
            //
            RenderPrepStats::Stage flushStage = mRenderPrepStatsWork.stage();
            setRenderPrepStatsStage(flushStage);

            if (flushStage == Stage::GM_FINALIZE0_TESSELLATION_PROCESS) {
                setRenderPrepStatsTessellationProcessed
                    (0,
                     mRenderPrepStatsWork.tessellationProcessed(0));
            } else if (flushStage == Stage::GM_FINALIZE1_TESSELLATION_PROCESS) {
                setRenderPrepStatsTessellationProcessed
                    (1,
                     mRenderPrepStatsWork.tessellationProcessed(1));
            }
            mRenderPrepStatsTessellationRequestFlush = false;
        }
    }

    {
        //
        // flush data for genericComment
        //
        std::lock_guard<std::mutex> lock(mGenericCommentMutex);
        if (!mGenericComment.empty()) {
            mInfoCodec.setString("genericComment", mGenericComment);
            mGenericComment.clear();
            mGenericComment.shrink_to_fit();
        }
    }
}

bool    
McrtNodeInfo::decode(const std::string& inputData)
{
    if (mInfoCodec.decode
        (inputData,
         [&]() {
            std::string str;
            int i;
            unsigned int ui;
            uint64_t ull;
            float f;
            size_t t;
            bool b;
            std::vector<float> vecF;
            if (mInfoCodec.getString("hostName", str)) {
                setHostName(str);
            } else if (mInfoCodec.getInt("machineId", i)) {
                setMachineId(i);
            } else if (mInfoCodec.getInt("cpuTotal", i)) {
                setCpuTotal(i);
            } else if (mInfoCodec.getInt("assignedCpuTotal", i)) {
                setAssignedCpuTotal(i);
            } else if (mInfoCodec.getFloat("cpuUsage", f)) {
                setCpuUsage(f);
            } else if (mInfoCodec.getVecFloat("coreUsage", vecF)) {
                setCoreUsage(vecF);
            } else if (mInfoCodec.getSizeT("memTotal", t)) {
                setMemTotal(t);
            } else if (mInfoCodec.getFloat("memUsage", f)) {
                setMemUsage(f);
            } else if (mInfoCodec.getInt("execMode", i)) {
                setExecMode(static_cast<ExecMode>(i));
            } else if (mInfoCodec.getFloat("snapshotToSend", f)) {
                setSnapshotToSend(f);
            } else if (mInfoCodec.getFloat("netRecv", f)) {
                setNetRecvBps(f);
            } else if (mInfoCodec.getFloat("netSend", f)) {
                setNetSendBps(f);
            } else if (mInfoCodec.getFloat("sendBps", f)) {
                setSendBps(f);
            } else if (mInfoCodec.getBool("feedbackActive", b)) {
                setFeedbackActive(b);
            } else if (mInfoCodec.getFloat("feedbackInterval", f)) {
                setFeedbackInterval(f);
            } else if (mInfoCodec.getFloat("recvFeedbackFps", f)) {
                setRecvFeedbackFps(f);
            } else if (mInfoCodec.getFloat("recvFeedbackBps", f)) {
                setRecvFeedbackBps(f);
            } else if (mInfoCodec.getFloat("evalFeedbackTime", f)) {
                setEvalFeedbackTime(f);
            } else if (mInfoCodec.getFloat("feedbackLatency", f)) {
                setFeedbackLatency(f);
            } else if (mInfoCodec.getFloat("clockTimeShift", f)) {
                setClockTimeShift(f);
            } else if (mInfoCodec.getFloat("roundTripTime", f)) {
                setRoundTripTime(f);
            } else if (mInfoCodec.getUInt64("lastRunClockOffsetTime", ull)) {
                setLastRunClockOffsetTime(ull);
            } else if (mInfoCodec.getBool("renderActive", b)) {
                setRenderActive(b);
            } else if (mInfoCodec.getBool("renderPrepCancel", b)) {
                setRenderPrepCancel(b);
            } else if (mInfoCodec.getUInt("syncId", ui)) {
                setSyncId(ui);
            } else if (mInfoCodec.getUInt("renderPrepStatsStage", ui)) {
                setRenderPrepStatsStage((RenderPrepStats::Stage)ui);
            } else if (mInfoCodec.getInt("renderPrepStatsLoadGeoTotal0", i)) {
                setRenderPrepStatsLoadGeometriesTotal(0, i);
            } else if (mInfoCodec.getInt("renderPrepStatsLoadGeoTotal1", i)) {
                setRenderPrepStatsLoadGeometriesTotal(1, i);
            } else if (mInfoCodec.getInt("renderPrepStatsLoadGeoProcessed0", i)) {
                setRenderPrepStatsLoadGeometriesProcessed(0, i);
            } else if (mInfoCodec.getInt("renderPrepStatsLoadGeoProcessed1", i)) {
                setRenderPrepStatsLoadGeometriesProcessed(1, i);
            } else if (mInfoCodec.getInt("renderPrepStatsTessellationTotal0", i)) {
                setRenderPrepStatsTessellationTotal(0, i);
            } else if (mInfoCodec.getInt("renderPrepStatsTessellationTotal1", i)) {
                setRenderPrepStatsTessellationTotal(1, i);
            } else if (mInfoCodec.getInt("renderPrepStatsTessellationProcessed0", i)) {
                setRenderPrepStatsTessellationProcessed(0, i);
            } else if (mInfoCodec.getInt("renderPrepStatsTessellationProcessed1", i)) {
                setRenderPrepStatsTessellationProcessed(1, i);
            } else if (mInfoCodec.getUInt64("globalBaseFromEpoch", ull)) {
                setGlobalBaseFromEpoch(ull);
            } else if (mInfoCodec.getUInt("totalMsg", ui)) {
                setMsgRecvTotal(ui);
            } else if (mInfoCodec.getFloat("oldestMsg", f)) {
                setOldestMsgRecvTiming(f);
            } else if (mInfoCodec.getFloat("newestMsg", f)) {
                setNewestMsgRecvTiming(f);
            } else if (mInfoCodec.getFloat("renderPrepStart", f)) {
                setRenderPrepStartTiming(f);
            } else if (mInfoCodec.getFloat("renderPrepEnd", f)) {
                setRenderPrepEndTiming(f);
            } else if (mInfoCodec.getFloat("snapshot1stStart", f)) {
                set1stSnapshotStartTiming(f);
            } else if (mInfoCodec.getFloat("snapshot1stEnd", f)) {
                set1stSnapshotEndTiming(f);
            } else if (mInfoCodec.getFloat("send1st", f)) {
                set1stSendTiming(f);
            } else if (mInfoCodec.getFloat("progress", f)) {
                setProgress(f);
            } else if (mInfoCodec.getFloat("globalProgress", f)) {
                setGlobalProgress(f);
            } else if (mInfoCodec.getString("genericComment", str)) {
                enqGenericComment(str);
            }
            return true;
        }) == -1) {
        return false;           // parse error
    }
    return true;
}

bool
McrtNodeInfo::setClockDeltaTimeShift(const std::string& hostName,
                                     float clockDeltaTimeShift, // millisec
                                     float roundTripTime)       // millisec
{
    if (hostName != mHostName) {
        return false;
    }
    setClockTimeShift(clockDeltaTimeShift);
    setRoundTripTime(roundTripTime);
    return true;
}

McrtNodeInfo::NodeStat
McrtNodeInfo::getNodeStat() const
{
    if (!mRenderActive) return NodeStat::IDLE;

    if (mRenderPrepStats.isCanceled()) return NodeStat::IDLE;
    if (mRenderPrepCancel) return NodeStat::RENDER_PREP_CANCEL;

    if (!mRenderPrepStats.isCompleted()) return NodeStat::RENDER_PREP_RUN;
    return NodeStat::MCRT;
}

std::string
McrtNodeInfo::show() const
{
    using scene_rdl2::str_util::addIndent;
    using scene_rdl2::str_util::boolStr;
    using scene_rdl2::str_util::byteStr;
    using scene_rdl2::str_util::secStr;

    size_t memUsed = static_cast<size_t>(static_cast<float>(mMemTotal) * mMemUsage);

    std::ostringstream ostr;
    ostr << "McrtNodeInfo {\n"
         << "  mHostName:" << mHostName << '\n'
         << "  mMachineId:" << mMachineId << '\n'
         << "  mCpuTotal:" << mCpuTotal << '\n'
         << "  mAssignedCpuTotal:" << mAssignedCpuTotal << '\n'
         << "  mCpuUsage:" << pctShow(mCpuUsage) << '\n'
         << addIndent(showCoreUsage()) << '\n'
         << "  mMemTotal:" << byteStr(mMemTotal) << '\n'
         << "  mMemUsage:" << pctShow(mMemUsage) << " (" << byteStr(memUsed) << ")\n"
         << "  mExecMode:" << execModeStr(getExecMode()) << '\n'
         << "  mSnapshotToSend:" << msShow(mSnapshotToSend) << '\n'
         << "  mNetRecvBps:" << byteStr(mNetRecvBps) << "/sec\n"
         << "  mNetSendBps:" << byteStr(mNetSendBps) << "/sec\n"
         << "  mSendBps:" << bytesPerSecShow(mSendBps) << '\n';
    ostr << "  mFeedbackActive:" << boolStr(mFeedbackActive) << '\n';
    if (mFeedbackActive) {
        ostr << "  mFeedbackInterval:" << mFeedbackInterval << '\n'
             << "  mRecvFeedbackFps:" << mRecvFeedbackFps << '\n'
             << "  mRecvFeedbackBps:" << bytesPerSecShow(mRecvFeedbackBps) << '\n'
             << "  mEvalFeedbackTime:" << msShow(mEvalFeedbackTime) << '\n'
             << "  mFeedbackLatency:" << msShow(mFeedbackLatency) << '\n';
    }
    ostr << "  mClockTimeShift:" << msShow(mClockTimeShift) << '\n'
         << "  mRoundTripTime:" << msShow(mRoundTripTime) << '\n'
         << "  mLastRunClockOffsetTime:" << mLastRunClockOffsetTime << " us ("
         << MiscUtil::timeFromEpochStr(mLastRunClockOffsetTime) << ")\n"
         << "  mSyncId:" << mSyncId << '\n'
         << "  mRenderActive:" << boolStr(mRenderActive) << '\n'
         << "  mRenderPrepCancel:" << boolStr(mRenderPrepCancel) << '\n'
         << scene_rdl2::str_util::addIndent(mRenderPrepStats.show()) << '\n'
         << "  mRenderPrepStatsLoadGeometriesRequestFlush:"
         << boolStr(mRenderPrepStatsLoadGeometriesRequestFlush) << '\n'
         << "  mRenderPrepStatsTessellationRequestFlush:"
         << boolStr(mRenderPrepStatsTessellationRequestFlush) << '\n'
         << addIndent(showTimeLog()) << '\n'
         << addIndent(showProgress()) << '\n'
         << "  mGenericComment:" << mGenericComment << '\n'
         << "  getNodeStat():" << nodeStatStr(getNodeStat()) << '\n'
         << "}";
    return ostr.str();
}

// static function
std::string
McrtNodeInfo::nodeStatStr(const NodeStat& stat)
{
    switch (stat) {
    case NodeStat::IDLE : return "IDLE";
    case NodeStat::RENDER_PREP_RUN : return "RENDER_PREP_RUN";
    case NodeStat::RENDER_PREP_CANCEL : return "RENDER_PREP_CANCEL";
    case NodeStat::MCRT : return "MCRT";
    default : return "?";
    }
}

// static function
std::string
McrtNodeInfo::execModeStr(const ExecMode& mode)
{
    switch (mode) {
    case ExecMode::SCALAR : return "SCALAR";
    case ExecMode::VECTOR : return "VECTOR";
    case ExecMode::XPU : return "XPU";
    case ExecMode::AUTO : return "AUTO";
    case ExecMode::UNKNOWN : return "UNKNOWN";
    default : return "?";
    }
}

//------------------------------------------------------------------------------------------

void
McrtNodeInfo::setupValueTimeTrackerMemory()
{
    mNetRecvVtt = std::make_shared<ValueTimeTracker>(mValueKeepDurationSec);
    mNetSendVtt = std::make_shared<ValueTimeTracker>(mValueKeepDurationSec);
}

void
McrtNodeInfo::parserConfigure()
{
    mParser.description("McrtNodeInfo command");

    mParser.opt("all", "", "show all info",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
    mParser.opt("renderPrep", "", "show renderPrep status",
                [&](Arg& arg) { return arg.msg(mRenderPrepStats.show() + '\n'); });
    mParser.opt("nodeStat", "", "show current node status",
                [&](Arg& arg) { return arg.msg(nodeStatStr(getNodeStat()) + '\n'); });
    mParser.opt("timeLog", "", "show timeLog info",
                [&](Arg& arg) { return arg.msg(showTimeLog() + '\n'); });
    mParser.opt("feedback", "", "show feedback related status",
                [&](Arg& arg) { return arg.msg(showFeedback() + '\n'); });
    mParser.opt("cpuUsage", "", "show cpu usage",
                [&](Arg& arg) { return arg.msg(showCpuUsage() + '\n'); });
    mParser.opt("coreUsage", "", "show core usage",
                [&](Arg& arg) { return arg.msg(showCoreUsage() + '\n'); });
    mParser.opt("execMode", "", "show execMode",
                [&](Arg& arg) { return arg.msg(execModeStr(getExecMode()) + '\n'); });
    mParser.opt("dataIO", "", "show dataIO usage",
                [&](Arg& arg) { return arg.msg(showDataIO() + '\n'); });
    mParser.opt("progress", "", "show progress info",
                [&](Arg& arg) { return arg.msg(showProgress() + '\n'); });
    mParser.opt("netRecvVtt", "...command...", "netRecv valueTimeTracker command",
                [&](Arg& arg) {
                    if (!mNetRecvVtt) return arg.msg("mNetRecvVtt is empty\n");
                    else return mNetRecvVtt->getParser().main(arg.childArg());
                });
    mParser.opt("netSendVtt", "...command...", "netSend valueTimeTracker command",
                [&](Arg& arg) {
                    if (!mNetSendVtt) return arg.msg("mNetSendVtt is empty\n");
                    else return mNetSendVtt->getParser().main(arg.childArg());
                });
}

std::string
McrtNodeInfo::showTimeLog() const
{
    using scene_rdl2::str_util::secStr;

    std::ostringstream ostr;
    ostr << "timeLog {\n"
         << "  mGlobalBaseFromEpoch:" << mGlobalBaseFromEpoch << " us ("
         << MiscUtil::timeFromEpochStr(mGlobalBaseFromEpoch) << ")\n"
         << "  mTotalMsg:" << mTotalMsg << '\n'
         << "  mOldestMessageRecvTiming:" << secStr(mOldestMessageRecvTiming) << '\n'
         << "  mNewestMessageRecvTiming:" << secStr(mNewestMessageRecvTiming) << '\n'
         << "  mRenderPrepStartTiming:" << secStr(mRenderPrepStartTiming) << '\n'
         << "  mRenderPrepEndTiming:" << secStr(mRenderPrepEndTiming) << '\n'
         << "  m1stSnapshotStartTiming:" << secStr(m1stSnapshotStartTiming) << '\n'
         << "  m1stSnapshotEndTiming:" << secStr(m1stSnapshotEndTiming) << '\n'
         << "  m1stSendTiming:" << secStr(m1stSendTiming) << '\n'
         << "}";
    return ostr.str();
}

std::string
McrtNodeInfo::showFeedback() const
{
    using scene_rdl2::str_util::boolStr;

    std::ostringstream ostr;
    ostr << "feedback status {\n"
         << "  mSendBps:" << bytesPerSecShow(mSendBps) << '\n'
         << "  mFeedbackActive:" << boolStr(mFeedbackActive) << '\n';
    if (mFeedbackActive) {
        ostr << "  mFeedbackInterval:" << mFeedbackInterval << '\n'
             << "  mRecvFeedbackFps:" << mRecvFeedbackFps << " fps\n"
             << "  mRecvFeedbackBps:" << bytesPerSecShow(mRecvFeedbackBps) << '\n'
             << "  mEvalFeedbackTime:" << msShow(mEvalFeedbackTime) << '\n'
             << "  mFeedbackLatency:" << msShow(mFeedbackLatency) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
McrtNodeInfo::showCpuUsage() const
{
    std::ostringstream ostr;
    ostr << "cpuTotal:" << mCpuTotal << '\n'
         << "assignedCpuTotal:" << mAssignedCpuTotal << '\n'
         << "cpuUsage:" << pctShow(mCpuUsage);
    return ostr.str();
}

std::string
McrtNodeInfo::showCoreUsage() const
{
    std::ostringstream ostr;
    ostr << "coreUsage (coreTotal:" << mCoreUsage.size() << ") {\n";
    int w = scene_rdl2::str_util::getNumberOfDigits(mCoreUsage.size());
    for (size_t i = 0; i < mCoreUsage.size(); ++i) {
        ostr << "  i:" << std::setw(w) << i << ' ' << pctShow(mCoreUsage[i]) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
McrtNodeInfo::showDataIO() const
{
    using scene_rdl2::str_util::byteStr;

    std::ostringstream ostr;
    ostr << "dataIO {\n"
         << "  netRecvBps:" << bytesPerSecShow(mNetRecvBps) << '\n'
         << "  netSendBps:" << bytesPerSecShow(mNetSendBps) << '\n'
         << "     sendBps:" << bytesPerSecShow(mSendBps) << '\n'
         << "}";
    return ostr.str();
}

std::string
McrtNodeInfo::showProgress() const
{
    std::ostringstream ostr;
    ostr << "progress {\n"
         << "  progress:" << pctShow(mProgress) << '\n'
         << "  globalProgress:" << pctShow(mGlobalProgress) << '\n'
         << "}";
    return ostr.str();
}

// static function
std::string
McrtNodeInfo::pctShow(float fraction)
{
    std::ostringstream ostr;
    ostr << std::setw(6) << std::fixed << std::setprecision(2) << (fraction * 100.0f) << " %";
    return ostr.str();
}

// static function
std::string
McrtNodeInfo::msShow(float ms)
{
    std::ostringstream ostr;
    ostr << std::setw(7) << std::fixed << std::setprecision(2) << ms << " ms";
    return ostr.str();
}

// static function
std::string
McrtNodeInfo::bytesPerSecShow(float bytesPerSec)
{
    std::ostringstream ostr;
    ostr << scene_rdl2::str_util::byteStr(static_cast<size_t>(bytesPerSec)) << "/sec";
    return ostr.str();
}

} // namespace mcrt_dataio
