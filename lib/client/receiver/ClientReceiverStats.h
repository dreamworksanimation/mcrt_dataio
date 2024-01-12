// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
// -- ProgressiveFrame message receiver statistic information --
//
// This file includes ClientReceiverStats class and it is used for performance analyzing about
// ProgressiveFrame receiver by frontend client in order to breakdown latency value, data receive fps,
// and received message size information. Only used for debugging purpose.<br>
//
#pragma once

#include <scene_rdl2/common/platform/Platform.h> // finline
#include <scene_rdl2/common/rec_time/RecTime.h>

namespace mcrt_dataio {

class ClientReceiverStats {
public:
    ClientReceiverStats() :
        mLatencyAll(0.0f),
        mLatencyTotal(0),
        mRecvMsgIntervalAll(0.0f),
        mRecvMsgIntervalTotal(0),
        mRecvMsgSizeAll(0),
        mRecvMsgSizeTotal(0)
    {}

    // Reset all internal information and back to default condition
    finline void reset();

    // Update onMessage() call interval
    // This API is updated internal info to measure onMessage() call interval.
    // Everytime you receive message inside onMessage() you should call this API once.
    void updateMsgInterval();

    // Update snapshot latency timing log
    // latencySec : Seconds since snapshot at backend computation to process at the client.
    // This API is updated latency info from snapshotStartTime to current time.
    // Everytime you receive message inside onMessage() you should call this API once with proper argument
    // for update latency.
    void updateLatency(const float latencySec);

    // Update received message size log
    // byte : Received message size
    // This API is updated received data size.
    // Everytime you receive message inside onMessage() you should call this API once with proper argument
    // for update received message size info.
    void updateRecvMsgSize(const uint64_t byte);
                
    // Show client receiver statistical info
    // elapsedSecFromStart : Elapsed second from start rendering. This is only used for header of output strings
    // return : Formatted strings to dump all statistical information about client receiver
    // Return formatted strings to dump all statistical information about client receiver.
    // This includes latency (= time from MCRT snapshot to current), fps (= frequency of received message) and
    // received message size (byte).
    // All recorded informations are averaged between every show() call and created formatted strings for
    // display.
    // Argument elapsedSecFromStart is just used for elapsed time display purpose and not used for other
    // internal result calculation.
    std::string show(const float elapsedSecFromStart) const;
                                             
protected:
    float mLatencyAll;
    uint64_t mLatencyTotal;;

    scene_rdl2::rec_time::RecTime mRecvMsgIntervalTime;
    float    mRecvMsgIntervalAll;  // interval (sec) of onMessage()
    uint64_t mRecvMsgIntervalTotal;

    uint64_t mRecvMsgSizeAll;
    uint64_t mRecvMsgSizeTotal;

    //------------------------------

    float calcAveLatency() const {
        return (mLatencyTotal)? (mLatencyAll / (float)mLatencyTotal * 1000.0f): 0.0f; // ms
    }
    finline float calcFps() const; // Frame Per Sec
    finline float calcBps() const; // Byte Per Sec
    finline uint64_t calcAveRecvMsgSize() const;
    std::string byteStr(const uint64_t size) const; // convert byte size to string
    std::string bpsStr(const float bps) const; // convert byte/sec info to string
}; // ClientReceiverStats

finline void
ClientReceiverStats::reset()
{
    mLatencyAll = 0.0f;
    mLatencyTotal = 0;
    mRecvMsgIntervalAll = 0.0f;
    mRecvMsgIntervalTotal = 0;
    mRecvMsgSizeAll = 0;
    mRecvMsgSizeTotal = 0;
}

finline float
ClientReceiverStats::calcFps() const
{
    if (mRecvMsgIntervalAll > 0.0f && mRecvMsgIntervalTotal > 0) {
        return (1.0f / (mRecvMsgIntervalAll / (float)mRecvMsgIntervalTotal));
    }
    return 0.0f;
}

finline float
ClientReceiverStats::calcBps() const
{
    if (mRecvMsgIntervalAll > 0.0f) {
        return (float)mRecvMsgSizeAll / mRecvMsgIntervalAll;
    }
    return 0.0f;
}

finline uint64_t
ClientReceiverStats::calcAveRecvMsgSize() const
{
    if (mRecvMsgSizeTotal) {
        return mRecvMsgSizeAll / mRecvMsgSizeTotal;
    }
    return 0;
}

} // namespace mcrt_dataio
