// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "McrtNodeInfo.h"

#include <mcrt_dataio/share/util/MiscUtil.h>

#include <scene_rdl2/render/util/StrUtil.h>

#include <iomanip>
#include <sstream>

namespace mcrt_dataio {

McrtNodeInfo::McrtNodeInfo(bool decodeOnly)
    : mMachineId(0)
    , mCpuTotal(0)
    , mCpuUsage(0.0f)            // fractio 0.0~1.0
    , mMemTotal(0)               // byte
    , mMemUsage(0.0f)            // fraction 0.0~1.0
    , mSnapshotToSend(0.0f)      // millisec
    , mSendBps(0.0f)             // byte/sec
    , mClockTimeShift(0.0f)      // millisec
    , mRoundTripTime(0.0f)       // millisec
    , mLastRunClockOffsetTime(0) // microsec from Epoch
    , mSyncId(0)
    , mRenderActive(false)
    , mRenderPrepCancel(false)
    , mRenderPrepStatsLoadGeometriesRequestFlush(false)
    , mRenderPrepStatsTessellationRequestFlush(false)
    , mGlobalBaseFromEpoch(0)
    , mTotalMsg(0)
    , mOldestMessageRecvTiming(0.0f)
    , mNewestMessageRecvTiming(0.0f)
    , mRenderPrepStartTiming(0.0f)
    , mRenderPrepEndTiming(0.0f)
    , m1stSnapshotStartTiming(0.0f)
    , m1stSnapshotEndTiming(0.0f)
    , m1stSendTiming(0.0f)
    , mProgress(0.0f)            // progress 0.0~1.0
    , mInfoCodec("mcrtNodeInfo", decodeOnly)
{
    parserConfigure();
}

void
McrtNodeInfo::setHostName(const std::string &hostName)
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
}

void
McrtNodeInfo::setCpuUsage(const float fraction)
{
    mInfoCodec.setFloat("cpuUsage", fraction, &mCpuUsage);
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
McrtNodeInfo::setSnapshotToSend(const float ms) // millisec
{
    mInfoCodec.setFloat("snapshotToSend", ms, &mSnapshotToSend);
}

void
McrtNodeInfo::setSendBps(const float bps) // byte/sec
{
    mInfoCodec.setFloat("sendBps", bps, &mSendBps);
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
McrtNodeInfo::setRenderPrepStats(const RenderPrepStats &rPrepStats)
//
// for mcrt_computation
//
{
    using Stage = RenderPrepStats::Stage;

    const Stage &stage = rPrepStats.stage();

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
McrtNodeInfo::setRenderPrepStatsStage(const RenderPrepStats::Stage &stage)
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
McrtNodeInfo::enqGenericComment(const std::string &comment)
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
McrtNodeInfo::decode(const std::string &inputData)
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
            if (mInfoCodec.getString("hostName", str)) {
                setHostName(str);
            } else if (mInfoCodec.getInt("machineId", i)) {
                setMachineId(i);
            } else if (mInfoCodec.getInt("cpuTotal", i)) {
                setCpuTotal(i);
            } else if (mInfoCodec.getFloat("cpuUsage", f)) {
                setCpuUsage(f);
            } else if (mInfoCodec.getSizeT("memTotal", t)) {
                setMemTotal(t);
            } else if (mInfoCodec.getFloat("memUsage", f)) {
                setMemUsage(f);
            } else if (mInfoCodec.getFloat("snapshotToSend", f)) {
                setSnapshotToSend(f);
            } else if (mInfoCodec.getFloat("sendBps", f)) {
                setSendBps(f);
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
McrtNodeInfo::setClockDeltaTimeShift(const std::string &hostName,
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

    auto pctShow = [&](float fraction) -> std::string {
        std::ostringstream ostr;
        ostr << std::setw(6) << std::fixed << std::setprecision(2) << fraction * 100.0f << " %";
        return ostr.str();
    };
    auto msShow = [&](float ms) -> std::string {
        std::ostringstream ostr;
        ostr << std::setw(7) << std::fixed << std::setprecision(2) << ms << " ms";
        return ostr.str();
    };
    auto bpsShow = [&](float bps) -> std::string {
        std::ostringstream ostr;
        ostr << byteStr(static_cast<size_t>(bps)) << "/sec";
        return ostr.str();
    };
    auto boolShow = [&](bool b) -> std::string { return (b)? "true": "false"; };

    size_t memUsed = static_cast<size_t>(static_cast<float>(mMemTotal) * mMemUsage);

    std::ostringstream ostr;
    ostr << "McrtNodeInfo {\n"
         << "  mHostName:" << mHostName << '\n'
         << "  mMachineId:" << mMachineId << '\n'
         << "  mCpuTotal:" << mCpuTotal << '\n'
         << "  mCpuUsage:" << pctShow(mCpuUsage) << '\n'
         << "  mMemTotal:" << byteStr(mMemTotal) << '\n'
         << "  mMemUsage:" << pctShow(mMemUsage) << " (" << byteStr(memUsed) << ")\n"
         << "  mSnapshotToSend:" << msShow(mSnapshotToSend) << '\n'
         << "  mSendBps:" << bpsShow(mSendBps) << '\n'
         << "  mClockTimeShift:" << msShow(mClockTimeShift) << '\n'
         << "  mRoundTripTime:" << msShow(mRoundTripTime) << '\n'
         << "  mLastRunClockOffsetTime:" << mLastRunClockOffsetTime << " us ("
         << MiscUtil::timeFromEpochStr(mLastRunClockOffsetTime) << ")\n"
         << "  mSyncId:" << mSyncId << '\n'
         << "  mRenderActive:" << boolShow(mRenderActive) << '\n'
         << "  mRenderPrepCancel:" << boolShow(mRenderPrepCancel) << '\n'
         << scene_rdl2::str_util::addIndent(mRenderPrepStats.show()) << '\n'
         << "  mRenderPrepStatsLoadGeometriesRequestFlush:"
         << boolStr(mRenderPrepStatsLoadGeometriesRequestFlush) << '\n'
         << "  mRenderPrepStatsTessellationRequestFlush:"
         << boolStr(mRenderPrepStatsTessellationRequestFlush) << '\n'
         << addIndent(showTimeLog()) << '\n'
         << "  mProgress:" << pctShow(mProgress) << '\n'
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

//------------------------------------------------------------------------------------------

void
McrtNodeInfo::parserConfigure()
{
    mParser.description("McrtNodeInfo command");

    mParser.opt("all", "", "show all info",
                [&](Arg& arg) -> bool { return arg.msg(show() + '\n'); });
    mParser.opt("renderPrep", "", "show renderPrep status",
                [&](Arg& arg) -> bool { return arg.msg(mRenderPrepStats.show() + '\n'); });
    mParser.opt("nodeStat", "", "show current node status",
                [&](Arg& arg) -> bool {
                    return arg.msg(nodeStatStr(getNodeStat()) + '\n');
                });
    mParser.opt("timeLog", "", "show timeLog info",
                [&](Arg& arg) -> bool { return arg.msg(showTimeLog() + '\n'); });
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

} // namespace mcrt_dataio

