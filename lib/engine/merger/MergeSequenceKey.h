// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace mcrt_dataio {

//
// This is a sequence-action key that uses for Merge Sequence Encode/Decode operation.
//
enum class MergeSequenceKey : unsigned int {
    DECODE_SINGLE = 0, // decode single progressiveFrame message action
    DECODE_RANGE,      // decode multiple progressiveFrame messages action (specified by range)
    MERGE_TILE_SINGLE, // partial merge single tile action
    MERGE_TILE_RANGE,  // partial merge multiple tiles action (specified by range)
    MERGE_ALL_TILES,   // full merge action
    EOD                // end of data
};

} // namespace mcrt_dataio
