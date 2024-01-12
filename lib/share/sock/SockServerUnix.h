// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <memory>
#include <string>

namespace mcrt_dataio {

class SockServerConnection;

class SockServerUnix
//
// This class is in charge of establishing incoming UNIX domain connections.
// New SockServerConnection is constructed if a new incoming connection is available
// when calling newClientConnection() API.
// newClientConnection() returns null if there is no incoming connection.
// This newClientConnection() is called from SockServer::mainLoop() periodically.
// You have to set a filename which is used by UNIX domain socket connections as an
// argument of open() API. This class supports abstract namespace mode of UNIX domain
// socket. In order to use abstract namespace mode, you have to set "@" for the argument
// of open().
//
{
public:
    using ConnectionShPtr = std::shared_ptr<SockServerConnection>;

    SockServerUnix() :
        mBaseSock(-1)
    {}
    ~SockServerUnix();

    // You have to set a filename which is used by UNIX domain socket connections
    // as path argument. This class supports abstract namespace mode of UNIX domain
    // socket. 
    // In order to use abstract namespace mode, you have to set a path starting
    // with "@" (like "@abc" or only "@"). Actual path would be created with
    // a port number. ActualPath = path + '.' + portNum. (If you set path as "@abc"
    // and port number is 20001, Actual path which used by unix-domain sock is "@abc.20001")
    // Too long path or empty returns error.
    bool open(const std::string &path, int port);

    // Returns a file path which is used by UNIX domain connections.
    // It returns "@" when under abstract namespace mode.
    const std::string &getPath() const { return mPath; }

    ConnectionShPtr newClientConnection();

private:
    std::string mPath;
    int mBaseSock;

    bool baseSockBindAndListen();
    bool acceptNewSocket(int *sock, std::string &clientPath) const;
    void closeBaseSock();
};

} // namespace mcrt_dataio

