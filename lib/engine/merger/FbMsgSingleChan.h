// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

//
// -- Message data for Single buffer --
//
// Typical ProgressiveFrame message consistes of multiple buffer data and each buffer data is
// independent PackTile (= compressed buffer data). This FbMsgSingleChan can keep single buffer
// multiple PackTiles (Why multiple ? is because receiver can receive multiple messages and need
// to keep all in one place by buffer name.
//
// This FbMsgSingleChan is only used if buffer is "LatencyLog", otherwise we processed received
// ProgressiveFrame immediately decode into fb and not saved into memory as binary stream.
// 

// This directive is used for debugging purpose only.
// Enable runtime verify operation for push/pop message by SHA1 hash data.
// We should commented out for promote version.
//#define RUNTIME_VERIFY_HASH_FBMSG_SINGLE_CHAN

#ifdef RUNTIME_VERIFY_HASH_FBMSG_SINGLE_CHAN
#include <scene_rdl2/common/grid_util/PackTiles.h> // for verify
#endif // end RUNTIME_VERIFY_HASH_FBMSG_SINGLE_CHAN

#include <scene_rdl2/common/platform/Platform.h> // finline

#include <string>
#include <vector>

namespace scene_rdl2 {
    namespace rdl2 { class ValueContainerEnq; }
} // namespace scene_rdl2

namespace mcrt_dataio {

class FbMsgSingleChan
{
public:
    using DataPtr = std::shared_ptr<const uint8_t>;

    finline void reset();
    finline bool push(DataPtr dataPtr, const size_t dataLength);

    // encode data and store into vContainerPush : used by latencyLog info
    void encode(scene_rdl2::rdl2::ValueContainerEnq &vContainerEnq) const;

    const std::vector<DataPtr> &dataArray() const { return mDataArray; }
    const std::vector<size_t> &dataSize() const { return mDataSize; }

    // This is special API just for debug purpose. Only works if this message is "latencyLog" 
    std::string showLatencyLog(const std::string &hd) const;

    std::string show(const std::string &hd) const;
    std::string show() const;

protected:
    //
    // We are keeping shared_ptr instead data itself here.
    // This reduce the cost of progmcrt_merge onMessage() 
    //
    std::vector<DataPtr> mDataArray; // mData[messageId] : vector of shared_ptr
    std::vector<size_t> mDataSize; // mDataSize[messageId]
}; // FbMsgSingleChan

finline void
FbMsgSingleChan::reset()
{
    mDataArray.clear();
    mDataArray.shrink_to_fit();
    mDataSize.clear();
    mDataSize.shrink_to_fit();
}

finline bool
FbMsgSingleChan::push(DataPtr dataPtr, const size_t dataLength)
{
    try {
        mDataArray.emplace_back(dataPtr);
    }
    catch (...) {
        return false;
    }
    mDataSize.push_back(dataLength);
    return true;
}

} // namespace mcrt_dataio
