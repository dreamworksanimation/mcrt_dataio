// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "SockServerConnection.h"

#include <scene_rdl2/common/grid_util/LiteralUtil.h>
#include <scene_rdl2/common/grid_util/SockUtil.h>

#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>        // TCP_NODELAY

using namespace scene_rdl2::grid_util; // for user defined literals of LiteralUtil.h

namespace mcrt_dataio {

bool
SockServerConnection::setInetSock(int sock,
                                  const std::string &clientHost,
                                  const int clientPort)
{
    mDomainType = DomainType::INETDOMAIN;
    
    mClientHost = clientHost;
    mClientPort = clientPort;
    mClientPath.clear();

    //
    // set socket option
    //
    int optV = 1;               // true
    if (::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&optV, sizeof(int)) < 0) {
        std::cerr << ">> SockServerConnection.cc ERROR : setsockopt() failed. TCP_NODELAY\n";
        return false;
    }

    /* debug info dump
    {
        socklen_t len;
        int cOptV;
        if (getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&cOptV, &len) < 0) {
            std::cerr << ">> SockServerConnection.cc ERROR : getsockopt() failed\n";
            return false;
        }
        std::cerr << ">> SockServerConnection.cc TCP_NODELAY:" <<+ std::to_string(cOptV) << '\n';
    }
    */

    if (!setupSendRecvBuffer(sock)) return false;

    mCore.setSock(sock);

    return true;
}

bool
SockServerConnection::setUnixSock(int sock, const std::string &clientPath)
{
    mDomainType = DomainType::UNIXDOMAIN;

    mClientHost.clear();
    mClientPort = 0;
    mClientPath = clientPath;

    if (!setupSendRecvBuffer(sock)) return false;

    mCore.setSock(sock);

    return true;
}

bool
SockServerConnection::setupSendRecvBuffer(int sock)
{
    //
    // send/recv internal buffer size setup
    // We can not set more than /proc/sys/net/core/rmem_max value
    // Default value is set at /proc/sys/net/core/rmem_default
    // I'll try to set 32MByte but probably this value is more than rmem_max
    //
    if (!setSockBufferSize(sock, SOL_SOCKET, static_cast<int>(32_MiB))) {
        std::cerr << ">> SockServerConnection.cc ERROR : setSockBufferSize() failed.\n";        
        return false;
    }

    /* debug info dump
    {
        socklen_t len;
        int cSendBuffSize, cRecvBuffSize;
        if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&cSendBuffSize, &len) < 0 ||
            getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&cRecvBuffSize, &len) < 0) {
            std::cerr << ">> SockServerConnection.cc ERROR : getsockopt() failed\n";
            return false;
        }
        std::cerr << ">> SockServerConnection.cc"
                  << " sendBuffsize:" << std::to_string(cSendBuffSize)
                  << " recvBuffSize:" << std::to_string(cRecvBuffSize) << '\n';
    }
    */

    return true;
}

} // namespace mcrt_dataio

