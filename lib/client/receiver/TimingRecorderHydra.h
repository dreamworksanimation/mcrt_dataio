// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/common/rec_time/RecTime.h>
#include <scene_rdl2/common/grid_util/Parser.h>

#include <array>
#include <unordered_map>

namespace mcrt_dataio {

class ResolveInfo
//
// The resolve action timing info with related messageHandler timing
//
{
public:
    static constexpr size_t sMessageHandlerTotal = 4;

    ResolveInfo()
        : mGetStart(0.0f)
        , mGetEnd(0.0f)
        , mRecvImgSenderMachineId(0)
    {}
    ResolveInfo(const std::string& name,
                float start,
                float end,
                const std::array<float, sMessageHandlerTotal>& messageHandler,
                int recvImgSenderMachineId)
        : mName(name)
        , mGetStart(start)
        , mGetEnd(end)
        , mMessageHandler{messageHandler}
        , mRecvImgSenderMachineId(recvImgSenderMachineId)
    {}

    const std::string& getName() const { return mName; }

    float getStart() const { return mGetStart; }
    float getEnd() const { return mGetEnd; }

    const std::array<float, sMessageHandlerTotal>& getMessageHandler() const { return mMessageHandler; }
    int getRecvImgSenderMachineId() const { return mRecvImgSenderMachineId; }

    float getDelta() const { return mGetEnd - mGetStart; }
    std::string showSenderMachineId() const;

private:    
    std::string mName;

    float mGetStart; // pixel data get action start timing
    float mGetEnd; // pixel data get action finish timing

    std::array<float, sMessageHandlerTotal> mMessageHandler;
    int mRecvImgSenderMachineId;
};

class TimingRecorderHydra
//
// Timing recording operation designed for hydra deligate client
//
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;
    using ResolveInfoShPtr = std::shared_ptr<ResolveInfo>;

    TimingRecorderHydra();

    //------------------------------

    void setInitArrasEnd(float sec);
    std::string showInitArras() const;

    float getInitArrasEnd() const { return mInitArras; }

    //------------------------------

    void initGlobalBaseTime();
    uint64_t getGlobalBaseTimeFromEpoch() const { return mGlobalBaseTimeFromEpoch; }

    void setConnect(size_t execPosId);
    std::string showConnect() const;

    size_t getConnectTotal() const { return sConnectTotal; }
    float getConnect(size_t execPosId) const;
    std::string getConnectDescription(size_t execPosId) const;

    //------------------------------

    void setEndUpdate(size_t execPosId);
    void setSendMessageSize(size_t size) { mSendMessageSize = size; }
    void afterSendMessage();
    std::string showEndUpdate() const;

    size_t getEndUpdateTotal() const { return sEndUpdateTotal; }
    float getEndUpdate(size_t execPosId) const;
    std::string getEndUpdateDescription(size_t execPosId) const;

    //------------------------------

    void setMessageHandler(size_t execPosId);
    void setReceivedImageSenderMachineId(int machineId);
    std::string showMessageHandler() const;

    size_t getMessageHandlerTotal() const { return sMessageHandlerTotal; }
    float getMessageHandler(size_t execPosId) const;
    std::string getMessageHandlerDescription(size_t execPosId) const;

    //------------------------------

    void setResolveStart();
    void setResolve(const std::string& name);
    std::string showResolve() const;

    const ResolveInfoShPtr get1stResolveInfo() const;
    std::string show1stImgSenderMachineId() const;

    //------------------------------

    std::string show() const;

    //------------------------------

    Parser& getParser() { return mParser; }

private:

    float mInitArras; // sec from initArras start

    uint64_t mGlobalBaseTimeFromEpoch; // initArras start time
    scene_rdl2::rec_time::RecTime mGlobalBaseTime;

    static constexpr size_t sConnectTotal = 6;
    std::array<std::string, sConnectTotal> mConnectDescription;
    std::array<float, sConnectTotal> mConnect; // sec from initArras start

    static constexpr size_t sEndUpdateTotal = 4;
    std::array<std::string, sEndUpdateTotal> mEndUpdateDescription;
    std::array<float, sEndUpdateTotal> mEndUpdate; // sec from initArras start
    size_t mSendMessageSize;

    static constexpr size_t sMessageHandlerTotal = ResolveInfo::sMessageHandlerTotal;
    std::array<std::string, sMessageHandlerTotal> mMessageHandlerDescription;
    std::array<float, sMessageHandlerTotal> mMessageHandler; // sec from initArras start
    int mRecvImgSenderMachineId;

    float mResolveStart;
    std::unordered_map<std::string, ResolveInfoShPtr> m1stResolve; // very 1st resolve action
    std::unordered_map<std::string, ResolveInfoShPtr> mResolve; // non-1st resolve action (overwrite)

    Parser mParser;

    //------------------------------

    int maxSecStrLen(size_t execPosTotal,
                     const std::function<float(size_t execPosId)> &calcSecCallBack) const;

    std::string showMessageHandler(const std::array<float, sMessageHandlerTotal> &messageHandler,
                                   int recvImgSenderMachineId) const;

    void parserConfigure();
};

} // namespace mcrt_dataio
