// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include "SockCoreSimple.h"

#include <string>

namespace mcrt_dataio {

class SockClient
//
// This class is used by the client side of the process. Server process should use
// arras::grid_util::SockServer class to communicate with this class.
// This class uses an INET domain socket between different hosts and a UNIX domain IPC
// between the same hosts (when you specify server hostname as "localhost").
// Under the Unix-domain IPC, this class supports abctract namespace mode.
// In order to use abstract namespace mode, you have to set a path starting
// with "@" (like "@abc" or only "@"). Actual path would be created with
// a port number. ActualPath = path + '.' + portNum. (If you set path as "@abc"
// and port number is 20001, Actual path which used by unix-domain sock is "@abc.20001")
//
{
public:
    static const int RECV_STATUS_EOF   = SockCoreSimple::RECV_STATUS_EOF;
    static const int RECV_STATUS_ERROR = SockCoreSimple::RECV_STATUS_ERROR;

    SockClient() :
        mPort(0)
    {}
    ~SockClient() { close(); }
    
    bool open(const std::string &hostName, // server hostname
              const int port,              // server port number
              const std::string &unixDomainSockPath); // used when hostName == "localhost"

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
    std::string mHostName; // server hostname
    int mPort;             // server port number
    std::string mUnixDomainSockPath;

    SockCoreSimple mCore;

    bool openSockMain(const bool errorDisplayST);
    bool openInetSockMain(const bool errorDisplayST);
    bool openUnixSockMain(const bool errorDisplayST);
};

} // namespace mcrt_dataio

