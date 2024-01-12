// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ClientReceiverFb.h"

#include <scene_rdl2/common/grid_util/DebugConsoleDriver.h>

namespace mcrt_dataio {

//
// This class provides debug console features to the clientReceiverFb and we can connect console
// via telnet connection and execute several different command line commands by hand. 
// This functionality is a big help to test the back-end engine via clientReceiverFb.
//
// This class boots an independent thread in order to run a debug console operation inside
// the initialize() function (See scene_rdl2/lib/common/grid_util/DebugConsoleDriver.h).
// If we don't have any incoming socket connection, this debug console threads
// almost always asleep and minimizes the CPU overhead. This debug console thread is automatically
// shut down inside the destructor.
//
class ClientReceiverConsoleDriver : public scene_rdl2::grid_util::DebugConsoleDriver
{
public:
    using MessageContentConstPtr = arras4::api::MessageContentConstPtr;
    using MessageSendFunc = std::function<bool(const MessageContentConstPtr msg)>;
    using MessageGenFunc = std::function<const MessageContentConstPtr()>;

    ClientReceiverConsoleDriver() :
        DebugConsoleDriver(),
        mParserMcrtRankId(0),    // only send mcrt command to rankId=0
        mFbReceiver(nullptr)
    {}
    ~ClientReceiverConsoleDriver() {}

    void set(const MessageSendFunc &messageSendCallBack,
             ClientReceiverFb *fbReceiver)
    {
        mMessageSend = messageSendCallBack;
        mFbReceiver = fbReceiver;
    }

    bool sendMessage(const MessageContentConstPtr msg) const
    {
        return (mMessageSend) ? (mMessageSend)(msg) : false;
    }
    bool sendMessage(const MessageGenFunc &func) const { return sendMessage(func()); }

private:
    using Parser = scene_rdl2::grid_util::Parser;
    using Arg = scene_rdl2::grid_util::Arg;

    void parserConfigure(Parser &parser) override;

    bool cmdAovLs(Arg &arg);
    bool cmdAovPix(Arg &arg);
    bool cmdPick(Arg &arg, int mode) const;
    bool cmdFeedback(Arg& arg);
    bool cmdFeedbackInterval(Arg& arg);
    void sendCommandToAllMcrtAndMerge(const std::string& cmd);

    std::string showRankInfo() const;

    //------------------------------

    Parser mParserAov;
    Parser mParserInvalidate;
    Parser mParserDispatch;
    Parser mParserMcrt;
    int mParserMcrtRankId; // mcrt debug command destination rankId. -1 indicates all mcrt
    Parser mParserMerge;
    Parser mParserPick;

    MessageSendFunc mMessageSend;
    ClientReceiverFb *mFbReceiver;
};

} // namespace mcrt_dataio
