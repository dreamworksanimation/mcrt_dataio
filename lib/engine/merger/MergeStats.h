// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#pragma once

#include <scene_rdl2/common/platform/Platform.h> // finline
#include <scene_rdl2/common/rec_time/RecTime.h>

namespace mcrt_dataio {

class MergeStats {
public:    
    MergeStats() :
        mSendMsgIntervalAll(0.0f),
        mSendMsgIntervalTotal(0),
        mSendMsgSizeAll(0),
        mSendMsgSizeTotal(0)
    {}

    /// @brief Reset all internal information and back to default condition
    finline void reset();

    /// @brief Update onIdle() call interval
    ///
    /// @detail
    /// This API is updated internal info to measure sendMsg() call interval.<br>
    /// Everytime you send message inside onIdle() you should call this API once.
    finline void updateMsgInterval();

    /// @brief Update send message size log
    /// @param byte send message size
    ///
    /// @detail
    /// This API is updated send data size.<br>
    /// Everytime you send message inside onIdle() you should call this API once with proper argument
    /// for update send message size info.
    finline void updateSendMsgSize(const uint64_t byte);

    std::string show(const float elapsedSecFromStart) const;

protected:

    scene_rdl2::rec_time::RecTime mSendMsgIntervalTime;
    float    mSendMsgIntervalAll;  // interval (sec) of sendProgressiveFrame()
    uint64_t mSendMsgIntervalTotal;

    uint64_t mSendMsgSizeAll;
    uint64_t mSendMsgSizeTotal;

    finline uint64_t calcAveSendMsgSize() const;
    finline float calcFps() const; // Frame Per Sec
    finline float calcBps() const; // Byte Per Sec
    std::string byteStr(const uint64_t size) const;
    std::string bpsStr(const float bps) const;
}; // MergeStats

finline void
MergeStats::reset()
{
    mSendMsgIntervalAll = 0.0f;
    mSendMsgIntervalTotal = 0;

    mSendMsgSizeAll = 0;
    mSendMsgSizeTotal = 0;
}

finline void
MergeStats::updateMsgInterval()
{
    if (mSendMsgIntervalTime.isInit()) {
        mSendMsgIntervalAll = 0.0f;
        mSendMsgIntervalTotal = 0;
    } else {
        mSendMsgIntervalAll += mSendMsgIntervalTime.end();
        mSendMsgIntervalTotal++;
    }
    mSendMsgIntervalTime.start();
}

finline void
MergeStats::updateSendMsgSize(const uint64_t byte)
{
    mSendMsgSizeAll += byte;
    mSendMsgSizeTotal++;
}

finline uint64_t
MergeStats::calcAveSendMsgSize() const
{
    if (mSendMsgSizeTotal) {
        return mSendMsgSizeAll / mSendMsgSizeTotal;
    }
    return 0;
}

finline float
MergeStats::calcFps() const
{
    if (mSendMsgIntervalAll > 0.0f && mSendMsgIntervalTotal > 0) {
        return (1.0f / (mSendMsgIntervalAll / (float)mSendMsgIntervalTotal));
    }
    return 0.0f;
}

finline float
MergeStats::calcBps() const
{
    if (mSendMsgIntervalAll > 0.0f) {
        return (float)mSendMsgSizeAll / mSendMsgIntervalAll;
    }
    return 0.0f;
}

} // namespace mcrt_dataio

