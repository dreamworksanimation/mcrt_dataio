// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <mcrt_dataio/share/sock/SockServerInet.h>

#include <string>

namespace mcrt_dataio {

class ClockDelta
//
// This class is used to measure how big the internal clock difference is between 2 hosts.
// Basic idea is very simple and follows. 
// First of all, host A sends packet to host B. Host B sends back packet to host A.
// By this procedure, host A measures round-trip timing cost.
// During this procedure, host B sends back host B's internal machine time to host A
// After receiving a round-trip packet from host B, host A can measure the round-trip
// timing cost. Considering these message passing costs, host A can calculate how big
// the internal clock shifts between host A and B.
// The fundamental premisses is that the timing cost from host A to B and host B to A is
// identical. This class assumes that a one way message send timing cost is half of
// roundtrip timing cost. In order to improve accuracy this class execute multiple times
// roundtrip test and averages result.
//
// Each host's internal clock syncs very well at Glendale farm but sometimes it is very
// different on the cloud (like Azure and/or AWS). This internal clock shift measurement
// is very important and powerfull to measure actual latency information under
// multi-machine arras environments.
//
// Basic usage is that we measure clock delta time between server and client. 
// You have to select one of the hosts as a server and you can use multiple client hosts.
// All result clock delta is calculated as relative timing with server hosts.
// You should use merge computation as server hosts and others (dispatch, mcrt and
// client-frontend) as clients.
// If hostA result is +3.5ms, this means hostA's internal clock is 3.5ms ahead of serve
// host.
//
{
public:
    enum class NodeType : int {
        CLIENT = 0,
        DISPATCH,
        MCRT
    };

    static bool serverMain(SockServerInet::ConnectionShPtr connection,
                           const int maxLoop,
                           std::string &hostName,
                           float &clockDelta,   // millisec
                           float &roundTripAve, // millisec
                           NodeType &nodeType);
    static bool clientMain(const std::string &serverName, int serverPort, const std::string &path,
                           const NodeType nodeType);

private:
    static float analyzeRoundTripTimeDelta(uint64_t startTime,
                                           uint64_t halfWayTime,
                                           uint64_t endTime,
                                           float &deltaStartEndMillSec);
};

} // namespace mcrt_dataio

