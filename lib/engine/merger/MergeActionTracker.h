// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mcrt_dataio/engine/merger/MergeSequenceEnqueue.h>
#include <scene_rdl2/render/cache/CacheDequeue.h>

// This enables keeping human-readable merge action information by string data for debugging purposes.
// This is pretty useful for manually tracing what operation has been executed regarding the merge operation.
//#define MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO

namespace mcrt_dataio {

class MergeActionTracker
//
// Regarding image synchronization feedback logic for adaptive sampling under multi-machine configuration,
// a complex merge operation is happening at Merge computation and MCRT computation needs to simulate the
// same merge operation. In order to do this action, it is pretty important to bookkeep what merge operation
// was executed internally. 
// MergeActionTracker records what kind of procedure was executed at Merge computation and encoded to binary
// data by special variable length coding format. This encoded data would send to the MCRT computation by
// progressiveFeedback messages. On MCRT computation, encoded data is properly decoded by MergeActionTracker
// as well and will use for simulating exactly the same merge operation at merge computation.
//
{
public:
    MergeActionTracker()
        : mMachineId(0) // dummy machineId
        , mLastSendActionId(0)
        , mLastPartialMergeTileId(0)
        , mEnq(&mData)
    {};

    void setMachineId(unsigned machineId) { mMachineId = machineId; }

    void resetEncode();

    void decodeAll(const std::vector<unsigned>& sendActionIdData);
    void mergeFull();
    void mergePartial(const std::vector<char>& partialMergeTilesTbl);

    void encodeData(scene_rdl2::cache::CacheEnqueue& enqueue);

    static void decodeDataSkipOnMCRTComputation(scene_rdl2::cache::CacheDequeue& dequeue);
    void decodeDataOnMCRTComputation(scene_rdl2::cache::CacheDequeue& dequeue);

    const std::string& getData() const { return mData; }

    //------------------------------

    std::string dumpData() const;
    unsigned getLastSendActionId() const { return mLastSendActionId; }
    unsigned getLastPartialMergeTileId() const { return mLastPartialMergeTileId; }

private:

    static std::string dumpDataAsAscii(const std::string& data);

#   ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
    void pushBackDebugEncodeSequence(const std::string& str);
#   endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO

    //------------------------------

    unsigned mMachineId; // for debug

    // for merge computation. keep last operated sendActionId and partialMergeTileId
    // partialMergeTileId is 0 if it is non partial merge mode
    // These counters are never reset during sessions.
    // These counters are always 0 under mcrt computation
    unsigned mLastSendActionId; // for debug
    unsigned mLastPartialMergeTileId; // for debug

    std::string mData;
    MergeSequenceEnqueue mEnq;

#   ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
    std::string mDebugEncodeSequence;
#   endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
};

} // namespace mcrt_dataio
