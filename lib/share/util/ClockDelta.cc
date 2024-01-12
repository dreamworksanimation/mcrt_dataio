// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "ClockDelta.h"
#include "MiscUtil.h"

#include <mcrt_dataio/share/sock/SockClient.h>
#include <mcrt_dataio/share/sock/SockServerConnection.h>

#include <scene_rdl2/scene/rdl2/ValueContainerDeq.h>
#include <scene_rdl2/scene/rdl2/ValueContainerEnq.h>

#include <iostream>

namespace mcrt_dataio {

// static function
bool
ClockDelta::serverMain(SockServerInet::ConnectionShPtr connection,
                       const int maxLoop,
                       std::string &hostName,
                       float &clockDelta,   // millisec
                       float &roundTripAve, // millisec
                       NodeType &nodeType)
{
    size_t recvSize;
    if (connection->recv(&recvSize, sizeof(size_t)) != sizeof(size_t)) {
        std::cerr << ">> ClockDelta.cc ERROR : serverMain() recv failed 1\n";
        return false;
    }
    std::string work(recvSize, 0x0);
    if (connection->recv(&work[0], recvSize) != static_cast<int>(recvSize)) {
        std::cerr << ">> ClockDelta.cc ERROR : serverMain() recv failed 2\n";
        return false;
    }
    scene_rdl2::rdl2::ValueContainerDeq vcDeq(work.data(), work.size());
    hostName = vcDeq.deqString();
    nodeType = static_cast<NodeType>(vcDeq.deqInt());
    // std::cerr << "client hostName:" << hostName << '\n'; // for debug

    float deltaSum = 0.0f;
    float roundTripSum = 0.0f;
    int totalTest = 0;
    for (int i = 0; i < maxLoop; ++i) {
        uint64_t sendData = MiscUtil::getCurrentMicroSec();
        if (!connection->send(&sendData, sizeof(uint64_t))) {
            std::cerr << ">> ClockDelta.cc ERROR : serverMain() loop sendData failed\n";
            break;
        }

        uint64_t recvData[3];
        int result = connection->recv(recvData, sizeof(uint64_t) * 2);
        if (result == -1) {
            break;              // EOF
        }
        if (result != sizeof(uint64_t) * 2) {
            std::cerr << ">> ClockDelta.cc ERROR : serverMain() loop recvData failed\n";
            break;
        }
        recvData[2] = MiscUtil::getCurrentMicroSec();

        float currRoundTrip; // ms
        deltaSum += analyzeRoundTripTimeDelta(recvData[0], recvData[1], recvData[2], currRoundTrip);
        roundTripSum += currRoundTrip;
        totalTest++;
    }

    if (totalTest > 0) {
        clockDelta = deltaSum / (float)totalTest;
        roundTripAve = roundTripSum / (float)totalTest;
    }

    return true;
}

// static function
bool
ClockDelta::clientMain(const std::string &serverName, int serverPort, const std::string &path,
                       const NodeType nodeType)
{
    SockClient sockClient;
    if (!sockClient.open(serverName, serverPort, path)) {
        std::cerr << ">> ClockDelta.cc ERROR : clientMain open failed\n";
        return false;
    }

    std::string work;
    scene_rdl2::rdl2::ValueContainerEnq vcEnq(&work);
    vcEnq.enqString(MiscUtil::getHostName());
    vcEnq.enqInt(static_cast<int>(nodeType));
    size_t dataSize = vcEnq.finalize();

    if (!sockClient.send(&dataSize, sizeof(dataSize)) || !sockClient.send(work.data(), dataSize)) {
        std::cerr << ">> ClockDelta.cc ERROR : clientMain send client hostName failed.\n";
        return false;
    }

    while (1) {
        uint64_t recvData[2];
        int flag = sockClient.recv(&recvData, sizeof(recvData[0]));
        if (flag == -1) {
            break;              // EOF
        }
        if (flag != sizeof(uint64_t)) {
            std::cerr << ">> ClockDelta.cc ERROR : clientMain recv failed\n";
            break;
        }

        recvData[1] = MiscUtil::getCurrentMicroSec();
        if (!sockClient.send(recvData, sizeof(uint64_t) * 2)) {
            std::cerr << ">> ClockDelta.cc ERROR : clockMain send clock info failed\n";
            break;
        }
    }

    return true;
}

//------------------------------------------------------------------------------------------

// static function
float
ClockDelta::analyzeRoundTripTimeDelta(uint64_t startTime,
                                      uint64_t halfWayTime,
                                      uint64_t endTime,
                                      float &deltaStartEndMillSec)
{
    /* useful debug message
    std::cout << "start:" << std::hex << std::setw(16) << std::setfill('0') << startTime << '\n'
              << " half:" << std::hex << std::setw(16) << std::setfill('0') << halfWayTime << '\n'
              << "  end:" << std::hex << std::setw(16) << std::setfill('0') << endTime
              << std::dec << std::endl;
    */

    int64_t deltaStartEnd = endTime - startTime;
    int64_t halfTimeStartEnd = deltaStartEnd / 2 + startTime;
    int64_t deltaHalf = halfWayTime - halfTimeStartEnd;

    deltaStartEndMillSec = (float)deltaStartEnd * 0.000001f * 1000.0f;
    float deltaHalfMillSec = (float)deltaHalf * 0.000001f * 1000.0f;

    /* useful debug message
    std::cout << "deltaStartEnd:" << deltaStartEndMillSec << " ms"
              << " half:" << deltaHalfMillSec << " ms"
              << std::endl;
    */

    return deltaHalfMillSec;
}

} // namespace mcrt_dataio

