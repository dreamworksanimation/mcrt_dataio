// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "SockServer.h"
#include "SockServerInet.h"
#include "SockServerUnix.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace mcrt_dataio {

void
SockServerConnectionQueue::enq(ConnectionShPtr connection)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mConnectionList.push_front(connection);
}

SockServerConnectionQueue::ConnectionShPtr
SockServerConnectionQueue::deq()
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mConnectionList.size()) {
        return ConnectionShPtr(nullptr);
    }

    ConnectionShPtr connection = mConnectionList.back();
    mConnectionList.pop_back();
    return connection;
}

//------------------------------------------------------------------------------------------

bool
SockServer::mainLoop(int port, const std::string &path, SockServerConnectionQueue &connectionQueue)
{
    return mainLoop(port, path,
                    [&](ConnectionShPtr connection) {
                        connectionQueue.enq(connection);
                    });
}

bool
SockServer::mainLoop(int port, const std::string &path, ConnectFunc connectFunc)
{
    SockServerInet sockServerInet;
    if (!sockServerInet.open(port)) {
        std::cerr << ">> SockServer.cc ERROR : mainLoop sockServerInet open failed\n";
        return false;
    }

    SockServerUnix sockServerUnix;
    if (!sockServerUnix.open(path, port)) {
        std::cerr << ">> SockServer.cc ERROR : mainLoop sockServerUnix open failed\n";
        return false;
    }

    while (1) {
        if (mShutdown && *mShutdown) break;

        SockServerInet::ConnectionShPtr connectionInet = sockServerInet.newClientConnection();
        SockServerInet::ConnectionShPtr connectionUnix = sockServerUnix.newClientConnection();

        if (!connectionInet && !connectionUnix) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } else {
            if (connectionInet) connectFunc(connectionInet);
            if (connectionUnix) connectFunc(connectionUnix);
        }
    }

    return true;
}

} // namespace mcrt_dataio

