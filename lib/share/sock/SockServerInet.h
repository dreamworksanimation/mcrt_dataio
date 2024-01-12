// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <memory>
#include <string>

namespace mcrt_dataio {

class SockServerConnection;

class SockServerInet
//
// This class is in charge of establishing incoming INET domain connections.
// New SockServerConnection is constructed if a new incoming connection is available
// when calling newClientConnection() API.
// newClientConnection() returns null if there is no incoming connection.
// This newClientConnection() is called from SockServer::mainLoop() periodically.
// If you set serverPortNumber as 0, this class automatically tries to find an
// non-used empty port number for you and opens the socket by that port.
// getPortNum() returns a port number which was used for open.
//
{
public:
    using ConnectionShPtr = std::shared_ptr<SockServerConnection>;

    SockServerInet() :
        mPort(-1),
        mBaseSock(-1)
    {}

    // If you set serverPortNumber as 0, this open() API automatically tries
    // to find an non-used empty port number for you and opens the socket by
    // that port.
    bool open(const int serverPortNumber);

    int getPortNum() const { return mPort; }

    ConnectionShPtr newClientConnection();

private:
    int mPort;                  // server port number
    int mBaseSock;              // for incoming socket

    bool baseSockBindAndListen();    
    bool acceptNewSocket(int *sock, std::string &clientHost, int *clientPort) const;
    void closeBaseSock();
};

} // namespace mcrt_dataio

