// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "MergeSequenceKey.h"

#include <scene_rdl2/render/cache/CacheEnqueue.h>

namespace mcrt_dataio {

class MergeSequenceEnqueue
//
// MergeSequenceEnqueue is an encoder of merge sequence action. Internally all merge sequence action
// is converted to variable length items and would convert to single binary data by CacheEnqueue.
// In order to decode this data, Please use MergeSequenceDequeue object.
//
{
public:
    MergeSequenceEnqueue(std::string* bytes)
        : mEnqueue(bytes)
    {}

    inline void decodeSingle(unsigned int sendImageActionId);
    inline void decodeRange(unsigned int startSendImageActionId, unsigned int endSendImageActionId);
    inline void mergeTileSingle(unsigned int tileId);
    inline void mergeTileRange(unsigned int startTileId, unsigned int endTileId);
    inline void mergeAllTiles();
    inline void endOfData();

    std::string showDebug() const;

private:
    scene_rdl2::cache::CacheEnqueue mEnqueue;
};

inline void
MergeSequenceEnqueue::decodeSingle(unsigned int sendImageActionId)
{
    mEnqueue.enqVLUInt(static_cast<unsigned int>(MergeSequenceKey::DECODE_SINGLE));
    mEnqueue.enqVLUInt(sendImageActionId);
}

inline void
MergeSequenceEnqueue::decodeRange(unsigned int startSendImageActionId,
                                  unsigned int endSendImageActionId)
{
    mEnqueue.enqVLUInt(static_cast<unsigned int>(MergeSequenceKey::DECODE_RANGE));
    mEnqueue.enqVLUInt(startSendImageActionId);
    mEnqueue.enqVLUInt(endSendImageActionId);
}

inline void
MergeSequenceEnqueue:: mergeTileSingle(unsigned int tileId)
{
    mEnqueue.enqVLUInt(static_cast<unsigned int>(MergeSequenceKey::MERGE_TILE_SINGLE));
    mEnqueue.enqVLUInt(tileId);
}

inline void
MergeSequenceEnqueue::mergeTileRange(unsigned int startTileId, unsigned int endTileId)
{
    mEnqueue.enqVLUInt(static_cast<unsigned int>(MergeSequenceKey::MERGE_TILE_RANGE));
    mEnqueue.enqVLUInt(startTileId);
    mEnqueue.enqVLUInt(endTileId);
}

inline void
MergeSequenceEnqueue::mergeAllTiles()
{
    mEnqueue.enqVLUInt(static_cast<unsigned int>(MergeSequenceKey::MERGE_ALL_TILES));
}

inline void
MergeSequenceEnqueue::endOfData()
{
    mEnqueue.enqVLUInt(static_cast<unsigned int>(MergeSequenceKey::EOD));
    mEnqueue.finalize();
}

} // namespace mcrt_dataio
