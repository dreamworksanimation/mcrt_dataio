// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TimingRecorderHydra.h"
#include "ClientReceiverFb.h"

#include <mcrt_dataio/share/util/MiscUtil.h>

#include <scene_rdl2/render/util/StrUtil.h>

namespace mcrt_dataio {

std::string
ResolveInfo::showSenderMachineId() const
{
    return ClientReceiverFb::showSenderMachineId(mRecvImgSenderMachineId);
}

//------------------------------------------------------------------------------------------

TimingRecorderHydra::TimingRecorderHydra()
    : mInitArras(0.0f)
    , mGlobalBaseTimeFromEpoch(0)
    , mSendMessageSize(0)
    , mRecvImgSenderMachineId(static_cast<int>(ClientReceiverFb::SenderMachineId::UNKNOWN))
    , mResolveStart(0.0f)
{
    mConnectDescription[0] = "connect start";
    mConnectDescription[1] = "before createSession";
    mConnectDescription[2] = "after createSession";
    mConnectDescription[3] = "before fbReceiver construction";
    mConnectDescription[4] = "after fbReceiver construction";
    mConnectDescription[5] = "connect finish";

    mEndUpdateDescription[0] = "endUpdate start";
    mEndUpdateDescription[1] = "after create rdlMessage";
    mEndUpdateDescription[2] = "after send message";
    mEndUpdateDescription[3] = "endUpdate finish";

    mMessageHandlerDescription[0] = "messageHandler start";
    mMessageHandlerDescription[1] = "after decodeProgressiveFrame";
    mMessageHandlerDescription[2] = "before send creditMsg";
    mMessageHandlerDescription[3] = "messageHandler finish";

    parserConfigure();
}

void
TimingRecorderHydra::setInitArrasEnd(float sec)
{
    mInitArras = sec;
}

std::string
TimingRecorderHydra::showInitArras() const
{
    using scene_rdl2::str_util::secStr;

    std::ostringstream ostr;
    ostr << "initArras {\n"
         << "  mInitArras:" << secStr(mInitArras) << '\n'
         << "}";
    return ostr.str();
}

//------------------------------

void
TimingRecorderHydra::initGlobalBaseTime()
{
    mGlobalBaseTimeFromEpoch = MiscUtil::getCurrentMicroSec(); // return microsec from Epoch
    mGlobalBaseTime.start();
}

void
TimingRecorderHydra::setConnect(size_t execPosId)
{
    if (execPosId < mConnect.size()) {
        mConnect[execPosId] = mGlobalBaseTime.end();
    }
}

std::string
TimingRecorderHydra::showConnect() const
{
    using scene_rdl2::str_util::secStr;

    int maxSizeTime =
        maxSecStrLen(mConnect.size(),
                     [&](size_t execPosId) -> float {
                         return mConnect[execPosId];
                     });
    int maxSizeDelta =
        maxSecStrLen(mConnect.size(),
                     [&](size_t execPosId) -> float {
                         return (!execPosId) ? 0.0f : mConnect[execPosId] - mConnect[execPosId - 1];
                     });
    int w = scene_rdl2::str_util::getNumberOfDigits(static_cast<unsigned>(mConnect.size()));

    std::ostringstream ostr;
    ostr << "connect {\n"
         << "  mGlobalBaseTimeFromEpoch:" << mGlobalBaseTimeFromEpoch << " us (" 
         << MiscUtil::timeFromEpochStr(mGlobalBaseTimeFromEpoch) << ")\n";
    for (size_t execPosId = 0; execPosId < mConnect.size(); ++execPosId) {
        float currSec = mConnect[execPosId];
        float deltaSec = (!execPosId) ? 0.0f : currSec - mConnect[execPosId - 1];
        ostr << "  " << std::setw(w) << std::setfill('0') << execPosId << std::setfill(' ') << " :"
             << " time(" << std::setw(maxSizeTime) << secStr(currSec) << ")"
             << " delta(" << std::setw(maxSizeDelta) << secStr(deltaSec) << ")"
             << " : " << mConnectDescription[execPosId] << '\n';
    }
    ostr << "}";
    return ostr.str();
}

float
TimingRecorderHydra::getConnect(size_t execPosId) const
{
    return (execPosId > sConnectTotal) ? 0.0f : mConnect[execPosId];
}

std::string
TimingRecorderHydra::getConnectDescription(size_t execPosId) const
{
    return (execPosId > sConnectTotal) ? "" : mConnectDescription[execPosId];
}

//------------------------------

void
TimingRecorderHydra::setEndUpdate(size_t execPosId)
{
    if (execPosId < mEndUpdate.size()) {
        mEndUpdate[execPosId] = mGlobalBaseTime.end();
    }
}

void
TimingRecorderHydra::afterSendMessage()
{
    m1stResolve.clear();
    mResolve.clear();
}

std::string
TimingRecorderHydra::showEndUpdate() const
{
    using scene_rdl2::str_util::byteStr;
    using scene_rdl2::str_util::secStr;

    int maxSizeTime =
        maxSecStrLen(mEndUpdate.size(),
                     [&](size_t execPosId) -> float {
                         return mEndUpdate[execPosId];
                     });
    float localBase = mEndUpdate[0];
    int maxSizeLocal =
        maxSecStrLen(mEndUpdate.size(),
                     [&](size_t execPosId) -> float {
                         return mEndUpdate[execPosId] - localBase;
                     });
    int maxSizeDelta =
        maxSecStrLen(mEndUpdate.size(),
                     [&](size_t execPosId) -> float {
                         return (!execPosId) ? 0.0f : mEndUpdate[execPosId] - mEndUpdate[execPosId - 1];
                     });
    int w = scene_rdl2::str_util::getNumberOfDigits(static_cast<unsigned>(mEndUpdate.size()));

    std::ostringstream ostr;
    ostr << "endUpdate {\n";
    for (size_t execPosId = 0; execPosId < mEndUpdate.size(); ++execPosId) {
        float currSec = mEndUpdate[execPosId];
        float localSec = currSec - localBase;
        float deltaSec = (!execPosId) ? 0.0f : currSec - mEndUpdate[execPosId - 1];
        ostr << "  " << std::setw(w) << std::setfill('0') << execPosId << std::setfill(' ') << " :"
             << " time(" << std::setw(maxSizeTime) << secStr(currSec) << ")"
             << " local(" << std::setw(maxSizeLocal) << secStr(localSec) << ")"
             << " delta(" << std::setw(maxSizeDelta) << secStr(deltaSec) << ")"
             << " : " << mEndUpdateDescription[execPosId] << '\n';
    }
    ostr << "}";
    return ostr.str();
}

float
TimingRecorderHydra::getEndUpdate(size_t execPosId) const
{
    return (execPosId > sEndUpdateTotal) ? 0.0f : mEndUpdate[execPosId];
}

std::string
TimingRecorderHydra::getEndUpdateDescription(size_t execPosId) const
{
    return (execPosId > sEndUpdateTotal) ? "" : mEndUpdateDescription[execPosId];
}

//------------------------------

void
TimingRecorderHydra::setMessageHandler(size_t execPosId)
{
    if (execPosId < mMessageHandler.size()) {
        mMessageHandler[execPosId] = mGlobalBaseTime.end();
    }
}

void
TimingRecorderHydra::setReceivedImageSenderMachineId(int machineId)
{
    mRecvImgSenderMachineId = machineId;
}

std::string
TimingRecorderHydra::showMessageHandler() const
{
    return showMessageHandler(mMessageHandler, mRecvImgSenderMachineId);
}

float
TimingRecorderHydra::getMessageHandler(size_t execPosId) const
{
    return (execPosId > sMessageHandlerTotal) ? 0.0f : mMessageHandler[execPosId];
}
    
std::string
TimingRecorderHydra::getMessageHandlerDescription(size_t execPosId) const
{
    return (execPosId > sMessageHandlerTotal) ? "" : mMessageHandlerDescription[execPosId];
}

//------------------------------

void
TimingRecorderHydra::setResolveStart()
{
    mResolveStart = mGlobalBaseTime.end();
}

void
TimingRecorderHydra::setResolve(const std::string& name)
{
    ResolveInfoShPtr d = std::make_shared<ResolveInfo>(name,
                                                       mResolveStart,
                                                       mGlobalBaseTime.end(),
                                                       mMessageHandler,
                                                       mRecvImgSenderMachineId);

    if (m1stResolve.find(name) == m1stResolve.end()) {
        // This is 1st resolve action after afterSendMessage()
        m1stResolve[name] = d;
    } else {
        mResolve[name] = d;
    }
}

std::string
TimingRecorderHydra::showResolve() const
{
    using scene_rdl2::str_util::addIndent;
    using scene_rdl2::str_util::secStr;

    auto showResolveInfo = [&](const ResolveInfoShPtr info) -> std::string {
        std::ostringstream ostr;
        ostr
        << "name:" << info->getName() << " {\n"
        << "  getStart:" << secStr(info->getStart()) << '\n'
        << "  getEnd:" << secStr(info->getEnd()) << '\n'
        << addIndent(showMessageHandler(info->getMessageHandler(), info->getRecvImgSenderMachineId())) << '\n'
        << "}";
        return ostr.str();
    };
    auto showResolveSec = [&](const std::string& msg,
                              const std::unordered_map<std::string, ResolveInfoShPtr> &resolve) -> std::string {
        std::ostringstream ostr;
        ostr << msg << " {\n";
        for (auto itr = m1stResolve.begin(); itr != m1stResolve.end(); ++itr) {
            ostr << addIndent(showResolveInfo(itr->second)) << '\n';
        }
        ostr << "}";
        return ostr.str();
        
    };

    std::ostringstream ostr;
    ostr << "resolve {\n"
         << addIndent(showResolveSec("1stResolve", m1stResolve)) << '\n'
         << addIndent(showResolveSec("resolve", mResolve)) << '\n'
         << "}";
    return ostr.str();
}

const TimingRecorderHydra::ResolveInfoShPtr
TimingRecorderHydra::get1stResolveInfo() const
{
    if (m1stResolve.empty()) return nullptr;

    ResolveInfoShPtr oldest = nullptr;
    float time = 0.0f;
    for (auto itr = m1stResolve.begin(); itr != m1stResolve.end(); ++itr) {
        const ResolveInfoShPtr info = itr->second;
        if (itr == m1stResolve.begin()) {
            time = info->getStart();
            oldest = info;
        } else if (info->getStart() < time) {
            oldest = info;
        }
    }
    return oldest;
}

std::string
TimingRecorderHydra::show1stImgSenderMachineId() const
{
    const ResolveInfoShPtr info = get1stResolveInfo();
    if (!info) return "?";
    return info->showSenderMachineId();
}

std::string
TimingRecorderHydra::show() const
{
    using scene_rdl2::str_util::addIndent;
    using scene_rdl2::str_util::secStr;

    std::ostringstream ostr;
    ostr << "TimingRecorderHydra {\n"
         << addIndent(showInitArras()) << '\n'
         << addIndent(showConnect()) << '\n'
         << addIndent(showEndUpdate()) << '\n'
         << addIndent(showMessageHandler()) << '\n'
         << addIndent(showResolve()) << '\n'
         << "}";
    return ostr.str();
}

//------------------------------------------------------------------------------------------

int
TimingRecorderHydra::maxSecStrLen(size_t execPosTotal,
                                  const std::function<float(size_t execPosId)> &calcSecCallBack) const
{
    using scene_rdl2::str_util::secStr;

    size_t maxLen = 0;
    for (size_t execPosId = 0; execPosId < execPosTotal; ++execPosId) {
        maxLen = std::max(secStr(calcSecCallBack(execPosId)).size(), maxLen);
    }
    return static_cast<int>(maxLen);
}

std::string
TimingRecorderHydra::showMessageHandler(const std::array<float, sMessageHandlerTotal> &messageHandler,
                                        int recvImgSenderMachineId ) const
{    
    using scene_rdl2::str_util::secStr;

    int maxSizeTime =
        maxSecStrLen(messageHandler.size(),
                     [&](size_t execPosId) -> float {
                         return messageHandler[execPosId];
                     });
    float localBase = messageHandler[0];
    int maxSizeLocal =
        maxSecStrLen(messageHandler.size(),
                     [&](size_t execPosId) -> float {
                         return messageHandler[execPosId] - localBase;
                     });
    int maxSizeDelta =
        maxSecStrLen(messageHandler.size(),
                     [&](size_t execPosId) -> float {
                         return ((!execPosId) ?
                                 0.0f :
                                 messageHandler[execPosId] - messageHandler[execPosId - 1]);
                     });
    int w = scene_rdl2::str_util::getNumberOfDigits(static_cast<unsigned>(messageHandler.size()));

    std::ostringstream ostr;
    ostr << "messageHandler {\n";
    for (size_t execPosId = 0; execPosId < messageHandler.size(); ++execPosId) {
        float currSec = messageHandler[execPosId];
        float localSec = currSec - localBase;
        float deltaSec = (!execPosId) ? 0.0f : currSec - messageHandler[execPosId - 1];
        ostr << "  " << std::setw(w) << std::setfill('0') << execPosId << std::setfill(' ') << " :"
             << " time(" << std::setw(maxSizeTime) << secStr(currSec) << ")"
             << " local(" << std::setw(maxSizeLocal) << secStr(localSec) << ")"
             << " delta(" << std::setw(maxSizeDelta) << secStr(deltaSec) << ")"
             << " : " << mMessageHandlerDescription[execPosId] << '\n';
    }
    ostr << "  mRecvImgSenderMachineId:"
         << ClientReceiverFb::showSenderMachineId(recvImgSenderMachineId) << '\n'
         << "}";
    return ostr.str();
}

void
TimingRecorderHydra::parserConfigure()
{
    mParser.description("TimingRecorderHydra command");
    mParser.opt("initArras", "", "show initArras info",
                [&](Arg& arg) -> bool { return arg.msg(showInitArras() + '\n'); });
    mParser.opt("connect", "", "show connect info",
                [&](Arg& arg) -> bool { return arg.msg(showConnect() + '\n'); });
    mParser.opt("endUpdate", "", "show endUpdate info",
                [&](Arg& arg) -> bool { return arg.msg(showEndUpdate() + '\n'); });
    mParser.opt("messageHandler", "", "show messageHandler info",
                [&](Arg& arg) -> bool { return arg.msg(showMessageHandler() + '\n'); });
    mParser.opt("resolve", "", "show resolve info",
                [&](Arg& arg) -> bool { return arg.msg(showResolve() + '\n'); });
    mParser.opt("all", "", "show all info",
                [&](Arg& arg) -> bool { return arg.msg(show() + '\n'); });
}

} // namespace mcrt_dataio
