// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <functional>
#include <string>

namespace mcrt_dataio {

class MsgSendHandler
//
// This class keeps the message send procedure internally and is used for
// sending infoCodec based information to the downstream computation.
//
{
public:
    using MsgSendFunc = std::function<void(const std::string &)>;
    
    void set(MsgSendFunc sendFunc) { mSendFunc = sendFunc; }

    void sendMessage(const std::string &msg) { mSendFunc(msg); }

private:
    MsgSendFunc mSendFunc;
};

} // namespace mcrt_dataio

