// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include "SockCoreSimple.h"

#include <string>

namespace mcrt_dataio {

class SockServerConnection
//
// This class is in charge of single socket connection send and receive operation at
// server side program. All the process regarding the established socket connection
// (for both of INET/UNIX domain) is completed by arras::grid_util::SockServer and
// this SockServerConnection is internally created by SockServer mainLoop() API.
// You can call close() when you want to disconnect connection.
//
{
public:
    static const int RECV_STATUS_EOF   = SockCoreSimple::RECV_STATUS_EOF;
    static const int RECV_STATUS_ERROR = SockCoreSimple::RECV_STATUS_ERROR;

    enum class DomainType : int {
        UNDEF = 0,
        INETDOMAIN,
        UNIXDOMAIN
    };

    SockServerConnection() :
        mDomainType(DomainType::UNDEF),
        mClientPort(0)
    {}

    bool setInetSock(int sock, const std::string &clientHost, const int clientPort);
    bool setUnixSock(int sock, const std::string &clientPath);

    // busy send
    bool send(const void *buff, const size_t size) { return mCore.busySend(buff, size); }

    // busy receive
    // return received data size (positive or 0) or error code (negative)
    //                 +n : receive data size byte
    //                  0 : skip receive operation
    //    RECV_STATUS_EOF : EOF (negative value)
    //  RECV_STATUS_ERROR : error (negative value)
    int recv(void *buff, const size_t size) { return mCore.busyRecv(buff, size); }

    void close() { mCore.close(); }

private:

    DomainType mDomainType;

    std::string mClientHost;    // connected client host name (inet-domain)
    int mClientPort;            // connected client port number (inet-domain)

    std::string mClientPath;    // connected client path (unix-domain)

    SockCoreSimple mCore;

    //------------------------------

    bool setupSendRecvBuffer(int sock);
};

} // namespace mcrt_dataio

