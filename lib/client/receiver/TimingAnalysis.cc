// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TimingAnalysis.h"

#include <mcrt_dataio/share/util/MiscUtil.h>
#include <iostream>

namespace mcrt_dataio {

int
TimingLogEvent::rankIdLen() const
{
    return static_cast<int>((mRankId == -1) ? showClient().size() : showRankId().size());
}

int
TimingLogEvent::secStrLenDeltaTimeStamp(const TimingLogEvent* prev) const
{
    return (!prev) ? 0 : secStrLen(mTimeStamp - prev->mTimeStamp);
}

std::string
TimingLogEvent::show(int rankIdLen,
                     int maxTimeLen,
                     int maxLocalTimeLen,
                     int maxDeltaTimeLen,
                     const TimingLogEvent* prevEvent) const
{
    using scene_rdl2::str_util::secStr;

    std::ostringstream ostr;

    // client/rankId field
    if (rankIdLen) ostr << std::setw(rankIdLen);
    ostr << ((mRankId == -1) ? showClient() : showRankId()) << " :";

    // time field
    ostr << " time(";
    if (maxTimeLen) ostr << std::setw(maxTimeLen);
    ostr << secStr(mTimeStamp) << ")";

    // local time field
    ostr << " local(";
    if (maxLocalTimeLen) ostr << std::setw(maxLocalTimeLen);
    ostr << secStr(mLocalTimeStamp) << ")";

    // delta time field
    if (prevEvent) {
        ostr << " delta(";
        if (maxDeltaTimeLen) ostr << std::setw(maxDeltaTimeLen);
        ostr << secStr(std::max(mTimeStamp - prevEvent->mTimeStamp, 0.0f)) << ")";
    }

    // description field
    ostr << " : " << mDescription;

    return ostr.str();
}

std::string
TimingLogEvent::showClient() const
{
    return std::string("client");
}

std::string
TimingLogEvent::showRankId() const
{
    std::ostringstream ostr;
    ostr << "rank:" << mRankId;
    return ostr.str();
}

// static function
int
TimingLogEvent::secStrLen(float sec)
{
    return static_cast<int>(scene_rdl2::str_util::secStr(sec).size());
}

//------------------------------------------------------------------------------------------

std::string
TimingLog::show() const
{
    using scene_rdl2::str_util::addIndent;
    using scene_rdl2::str_util::secStr;

    int w = scene_rdl2::str_util::getNumberOfDigits(static_cast<unsigned>(mEventTable.size()));

    int lenRankId = 0;
    int lenTime = 0;
    int lenDeltaTime = 0;
    int lenLocalTime = 0;
    for (size_t eventId = 0; eventId < mEventTable.size(); ++eventId) {
        const TimingLogEvent* currEvent = &mEventTable[eventId];
        const TimingLogEvent* prevEvent = (eventId == 0) ? nullptr : &mEventTable[eventId - 1];
        lenRankId = std::max(currEvent->rankIdLen(), lenRankId);
        lenTime = std::max(currEvent->secStrLenTimeStamp(), lenTime);
        lenDeltaTime = std::max(currEvent->secStrLenDeltaTimeStamp(prevEvent), lenDeltaTime);
        lenLocalTime = std::max(currEvent->secStrLenLocalTimeStamp(), lenLocalTime);
    }

    auto showEvent =
        [&](size_t id, const TimingLogEvent* currEvent, const TimingLogEvent* prevEvent) -> std::string {
        std::ostringstream ostr;
        ostr
        << "event-" << std::setw(w) << std::setfill('0') << id << std::setfill(' ') << " : "
        << currEvent->show(lenRankId, lenTime, lenLocalTime, lenDeltaTime, prevEvent);
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "TimingLog {\n"
         << "  baseTime:" << mBaseTime << " us (" << MiscUtil::timeFromEpochStr(mBaseTime) << ")\n"
         << "  1stRecvImgSender: " << mRecvImgSenderMachineId << '\n';
    for (size_t i = 0; i < mEventTable.size(); ++i) {
        const TimingLogEvent* currEvent = &mEventTable[i];
        const TimingLogEvent* prevEvent = (i == 0) ? nullptr : &mEventTable[i - 1];
        ostr << addIndent(showEvent(i, currEvent, prevEvent)) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

//------------------------------------------------------------------------------------------

TimingAnalysis::TimingAnalysis(GlobalNodeInfo& globalNodeInfo)
    : mGlobalNodeInfo(globalNodeInfo)
{
    parserConfigure();
}

void
TimingAnalysis::setTimingRecorderHydra(std::shared_ptr<TimingRecorderHydra> timingRecorderHydra)
{
    mTimingRecorderHydra = timingRecorderHydra;
}
    
TimingAnalysis::TimingLogShPtr
TimingAnalysis::make1stRecvImgTimingLogHydra() const
{
    if (!mTimingRecorderHydra) return nullptr;

    std::shared_ptr<TimingRecorderHydra> tr = mTimingRecorderHydra;

    TimingLogShPtr log = std::make_shared<TimingLog>();
    log->setBaseTime(tr->getGlobalBaseTimeFromEpoch());
    log->setRecvImgSenderMachineId(tr->show1stImgSenderMachineId());

    static constexpr int client = -1; // client rankId = -1

    int mcrtId = -1;
    if (tr->get1stResolveInfo()) {
        mcrtId = tr->get1stResolveInfo()->getRecvImgSenderMachineId();
    }
    if (mcrtId < 0) {
        // received image was not properly rendered by mcrt computation somehow.
        log->enqEvent(client, 0.0f, 0.0f, "ERROR : can not find received image sender machineId");
        return log;
    }

    // init-arras
    log->enqEvent(client, tr->getInitArrasEnd(), tr->getInitArrasEnd(), "initArras end");

    // connect
    float localBase = 0.0f;
    for (size_t execPosId = 0; execPosId < tr->getConnectTotal(); ++execPosId) {
        float currTime = tr->getConnect(execPosId);
        if (!execPosId) localBase = currTime;
        log->enqEvent(client, currTime, currTime - localBase, tr->getConnectDescription(execPosId));
    }
                 
    // endUpdate
    localBase = 0.0f;
    for (size_t execPosId = 0; execPosId < tr->getEndUpdateTotal(); ++execPosId) {
        float currTime = tr->getEndUpdate(execPosId);
        if (!execPosId) localBase = currTime;
        log->enqEvent(client, currTime, currTime - localBase, tr->getEndUpdateDescription(execPosId));
    }

    // mcrt info
    mGlobalNodeInfo.accessMcrtNodeInfo(mcrtId, [&](std::shared_ptr<McrtNodeInfo> nodeInfo) -> bool {
            makeTimingLogMcrt(nodeInfo, log);
            return true;
        });

    // ResolveInfo
    const TimingRecorderHydra::ResolveInfoShPtr resolveInfo = tr->get1stResolveInfo();
    if (!resolveInfo) {
        log->enqEvent(client, 0.0f, 0.0f, "ERROR : ResolveInfo empty");
        return log;
    }

    // messageHandler
    localBase = 0.0f;
    for (size_t execPosId = 0; execPosId < resolveInfo->getMessageHandler().size(); ++execPosId) {
        float currTime = resolveInfo->getMessageHandler()[execPosId];
        if (!execPosId) localBase = currTime;
        log->enqEvent(client, currTime, currTime - localBase, tr->getMessageHandlerDescription(execPosId));
    }

    // resolve
    log->enqEvent(client, resolveInfo->getStart(), 0.0f, "resolve get start");
    log->enqEvent(client, resolveInfo->getEnd(), resolveInfo->getDelta(), "resolve get end");

    return log;
}

void
TimingAnalysis::makeTimingLogMcrt(std::shared_ptr<McrtNodeInfo> nodeInfo, TimingLogShPtr log) const
{
    int rankId = nodeInfo->getMachineId();

    float baseSec = nodeInfo->getOldestMessageRecvTiming(); // mcrt local time

    auto pushEvent = [&](float mcrtLocalSec, float mcrtLocalBaseSec, const std::string& description) {
        log->enqEvent(rankId,
                      deltaSecMcrtToClient(mcrtLocalSec, nodeInfo),
                      mcrtLocalSec - mcrtLocalBaseSec,
                      description);
    };
    
    pushEvent(baseSec, baseSec, "message recv");
    pushEvent(nodeInfo->getRenderPrepStartTiming(), baseSec, "renderPrep start");
    pushEvent(nodeInfo->getRenderPrepEndTiming(), baseSec, "renderPrep end");
    pushEvent(nodeInfo->get1stSnapshotStartTiming(), baseSec, "1st snapshot start");
    pushEvent(nodeInfo->get1stSnapshotEndTiming(), baseSec, "1st snapshot end");
    pushEvent(nodeInfo->get1stSendTiming(), baseSec, "1st send");
}

float
TimingAnalysis::deltaSecMcrtToClient(float mcrtDeltaSec, std::shared_ptr<McrtNodeInfo> nodeInfo) const
//
// This function converts mcrtDeltaSec on the backend computation to the deltaSec of the client.
// This conversion takes into consideration of an internal clock shift between backend hosts and client hosts.
//    
{
    float mcrtClockTimeShift = nodeInfo->getClockTimeShift(); // millisec
    float clientClockTimeShift = mGlobalNodeInfo.getClientClockTimeShift(); // millisec

    uint64_t mcrtGlobalUS = (static_cast<uint64_t>(mcrtDeltaSec * 1000000.0f) +
                             nodeInfo->getGlobalBaseFromEpoch()); // microsec
    uint64_t mergeGlobalUS = mcrtGlobalUS - static_cast<uint64_t>(mcrtClockTimeShift * 1000.0f);
    uint64_t clientGlobalUS = mergeGlobalUS + static_cast<uint64_t>(clientClockTimeShift * 1000.0f);
    uint64_t clientDeltaUS = clientGlobalUS - mTimingRecorderHydra->getGlobalBaseTimeFromEpoch();

    float clientDeltaSec = static_cast<float>(static_cast<double>(clientDeltaUS) / 1000000.0);
    return clientDeltaSec;
}

std::string
TimingAnalysis::show1stRecvImgLogHydra() const
{
    if (!mTimingRecorderHydra) return "timingRecorderHydra is empty";
    TimingLogShPtr log = make1stRecvImgTimingLogHydra();
    return (!log) ? "timing log is empty" : log->show();
}

void
TimingAnalysis::parserConfigure()
{
    mParser.description("timingAnalysis command");
    mParser.opt("globalNode", "...command...", "globalNode command",
                [&](Arg& arg) -> bool { return mGlobalNodeInfo.getParser().main(arg.childArg()); });
    mParser.opt("timingRecorder", "...command...", "timingRecorderHydra command",
                [&](Arg& arg) -> bool {
                    if (!mTimingRecorderHydra) return arg.msg("timingRecorderHydra is empty");
                    return mTimingRecorderHydra->getParser().main(arg.childArg());
                });
    mParser.opt("show1stLogHydra", "", "show 1st received image log",
                [&](Arg& arg) -> bool { return arg.msg(show1stRecvImgLogHydra() + '\n'); });
}

} // namespace mcrt_dataio
