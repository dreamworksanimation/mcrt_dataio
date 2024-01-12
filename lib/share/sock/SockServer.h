// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include "SockServerConnection.h"

#include <functional>
#include <list>
#include <memory>
#include <mutex>

namespace mcrt_dataio {

class SockServerConnectionQueue
//
// This class is keeping multiple SockServerConnection by FIFO queue with MTsafe operation.
//
{
public:
    using ConnectionShPtr = std::shared_ptr<SockServerConnection>;

    void enq(ConnectionShPtr connection); // MTsafe
    ConnectionShPtr deq();                // MTsafe

private:
    using ConnectionList = std::list<ConnectionShPtr>;

    std::mutex mMutex;
    ConnectionList mConnectionList;
};

class SockServer
//
// This is a server side socket main loop API.
// You have to set the shutdownFlag address to the constructor. Only way to finish this
// mainLoop() function is to set true to shutdownFlag. After calling mainLoop(), mainLoop()
// continues to process new connections from outside until shutdownFlag sets true. (This
// is a biggest difference from SockP2p class).
// Internally mainLoop() is watching both of INET domain connection and Unix domain (IPC)
// connection.
// This class is designed under multi thread configurations. New incoming connections are
// stored into connectionQueue. You have to process this connectionQueue by another thread
// which is not calling mainLoop() thread.
// In order to use Unix domain (IPC) abstract namespace mode, you have to set a path starting
// with "@" (like "@abc" or only "@") for path-argument of mainLoop().
// Actual path would be created with a port number. ActualPath = path + '.' + portNum.
// (If you set path as "@abc" and port number is 20001, Actual path which used by
// unix-domain sock is "@abc.20001")
//    
{
public:
    using ConnectionShPtr = std::shared_ptr<SockServerConnection>;
    using ConnectFunc = std::function<void(ConnectionShPtr)>;

    explicit SockServer(bool *shutdownFlag) : // need to set shutdown control flag address
        mShutdown(shutdownFlag)
    {}

    // for multi-thread implementation. The connectionQueue has to be processed by other threads.
    bool mainLoop(int port,                // for INET connection from other hosts
                  const std::string &path, // for IPC (Unix domain) connection from same hosts
                  SockServerConnectionQueue &connectionQueue);

    // for single thread easy API. the connectFunc is processed by the same thread of mainLoop.
    bool mainLoop(int port,                // for INET connection from other hosts
                  const std::string &path, // for IPC (Unix domain) connection from same hosts
                  ConnectFunc connectFunc);

private:
    bool *mShutdown;
};

} // namespace mcrt_dataio

