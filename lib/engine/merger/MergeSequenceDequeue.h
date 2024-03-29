// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "MergeSequenceKey.h"
#include <scene_rdl2/render/cache/CacheDequeue.h>

#include <cstddef>              // size_t
#include <string>

namespace mcrt_dataio {

class MergeSequenceDequeue
//
// MergeSequenceDequeue is a decoder of merge sequence action binary data generated by MergeSequenceEnqueue.
//
{
public:
    MergeSequenceDequeue(const void* addr, const size_t dataSize)
        : mDequeue(addr, dataSize)
    {}

    template <typename DecodeSingleFunc,
              typename DecodeRangeFunc,
              typename MergeTileSingleFunc,
              typename MergeTileRangeFunc,
              typename MergeAllTilesFunc,
              typename EODFunc>
    bool decodeLoop(std::string& error,
                    DecodeSingleFunc decodeSingleFunc,
                    DecodeRangeFunc decodeRangeFunc,
                    MergeTileSingleFunc mergeTileSingleFunc,
                    MergeTileRangeFunc mergeTileRangeFunc,
                    MergeAllTilesFunc mergeAllTilesFunc,
                    EODFunc eodFunc)
    {
        auto pushError = [&](const std::string& msg) {
            if (!error.empty()) error += '\n';
            error += msg;
        };

        float endFlag = false;
        while (!endFlag) {
            MergeSequenceKey key = static_cast<MergeSequenceKey>(mDequeue.deqVLUInt());

            switch (key) {
            case MergeSequenceKey::DECODE_SINGLE : {
                unsigned int sendImageActionId = mDequeue.deqVLUInt();
                if (!decodeSingleFunc(sendImageActionId)) {
                    pushError("ERROR : MergeSequenceDequeue() decodeSingleFunc() failed");
                    return false;
                }
            } break;
            case MergeSequenceKey::DECODE_RANGE : {
                unsigned int startSendImageActionId = mDequeue.deqVLUInt();
                unsigned int endSendImageActionId = mDequeue.deqVLUInt();
                if (!decodeRangeFunc(startSendImageActionId, endSendImageActionId)) {
                    pushError("ERROR : MergeSequenceDequeue() decodeRangeFunc() failed");
                    return false;
                }
            } break;
            case MergeSequenceKey::MERGE_TILE_SINGLE : {
                unsigned int tileId = mDequeue.deqVLUInt();
                if (!mergeTileSingleFunc(tileId)) {
                    pushError("ERROR : MergeSequenceDequeue() mergeTileSingleFunc() failed");
                    return false;
                }
            } break;
            case MergeSequenceKey::MERGE_TILE_RANGE : {
                unsigned int startTileId = mDequeue.deqVLUInt();
                unsigned int endTileId = mDequeue.deqVLUInt();
                if (!mergeTileRangeFunc(startTileId, endTileId)) {
                    pushError("ERROR : MergeSequenceDequeue() mergeTileRangeFunc() failed");
                    return false;
                }
            } break;
            case MergeSequenceKey::MERGE_ALL_TILES : {
                if (!mergeAllTilesFunc()) {
                    pushError("ERROR : MergeSequenceDequeue() mergeFullFunc() failed");
                    return false;
                }
            } break;
            case MergeSequenceKey::EOD : {
                if (!eodFunc()) {
                    pushError("ERROR : MergeSequenceDequeue() eodFunc() failed");
                    return false;
                }
                endFlag = true;
            } break;
            default : {
                std::ostringstream ostr;
                ostr << "ERROR : MergeSequenceDequeue() unknown MergeSequenceKey"
                     << " key:0x" << std::hex << static_cast<unsigned int>(key);
                pushError(ostr.str());
                return false;
            }
            }
        }
        return true;
    }

private:
    scene_rdl2::cache::CacheDequeue mDequeue;
};

} // namespace mcrt_dataio
