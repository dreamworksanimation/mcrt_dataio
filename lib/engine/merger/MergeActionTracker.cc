// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "MergeActionTracker.h"
#include "MergeSequenceDequeue.h"

#include <scene_rdl2/render/util/StrUtil.h>

//#define DEBUG_MSG_RESET_ENCODE  // debug message for MergeActionTracker::resetEncode()
//#define DEBUG_MSG_DECODEALL     // debug message for MergeActionTracker::decodeAll()
//#define DEBUG_MSG_MERGE_FULL    // debug message for MergeActionTracker::mergeFull()
//#define DEBUG_MSG_MERGE_PARTIAL // debug message for MergeActionTracker::mergePartial()
//#define DEBUG_MSG_ENCODE_DATA   // debug message for MergeActionTracker::encodeData()
//#define DEBUG_MSG_DECODE_DATA_SKIP // debug message for MergeActionTracker::decodeDataSkip()
//#define DEBUG_MSG_DECODE_DATA      // debug message for MergeActionTracker::decodeDataOnMCRTComputation()

namespace mcrt_dataio {

void
MergeActionTracker::resetEncode()
{
#   ifdef DEBUG_MSG_RESET_ENCODE
    std::cerr << ">> MergeActionTracker.cc resetEncode()\n";
#   endif // end DEBUG_MSG_RESET_ENCODE

    mData.clear();
    new(&mEnq) MergeSequenceEnqueue(&mData);

#   ifdef DEBUG_MSG_RESET_ENCODE
    std::cerr << ">> MergeActionTracker.cc resetEncode() done\n";
    // std::cerr << mEnq.showDebug() << " mMachineId:" << mMachineId << '\n';
#   endif // end DEBUG_MSG_RESET_ENCODE

#   ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
    mDebugEncodeSequence.clear();
#   endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
}

void    
MergeActionTracker::decodeAll(const std::vector<unsigned>& sendActionIdData)
{
    if (sendActionIdData.size() == 1) {
        mEnq.decodeSingle(sendActionIdData[0]);
#       ifdef DEBUG_MSG_DECODEALL
        std::cerr << ">> MergeActionTracker.cc decodeAll()"
                  << " decodeSingle sendActionId:" << sendActionIdData[0]
                  << " mMachineId:" << mMachineId << '\n';
        // std::cerr << mEnq.showDebug() << " mMachineId:" << mMachineId << '\n';
#       endif // end DEBUG_MSG_DECODEALL

#       ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
        {
            std::ostringstream ostr;
            ostr << "decodeAll(single:" << sendActionIdData[0] << ')';
            pushBackDebugEncodeSequence(ostr.str());
        }
#       endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
        return;
    }

#   ifdef DEBUG_MSG_DECODEALL
    {
        std::ostringstream ostr;
        ostr << "sendActionIdData size:" << sendActionIdData.size() << " {\n";
        for (size_t i = 0; i < sendActionIdData.size(); ++i) {
            if (i == 0) ostr << "  ";
            ostr << sendActionIdData[i]
                 << ((i != sendActionIdData.size() - 1) ? ' ' : '\n');
        }
        ostr << "}";
        std::cerr << ">> MergeActionTracker.cc decodeAll()"
                  << " decodeMulti start size:" << sendActionIdData.size() << " {\n"
                  << scene_rdl2::str_util::addIndent(ostr.str()) << '\n'
                  << "} mMachineId:" << mMachineId << '\n';
    }
#   endif // end DEBUG_MSG_DECODEALL

    unsigned activeStartId, activeEndId;

    auto resetActiveRange = [&]() { activeStartId = ~0; activeEndId = ~0; };
    auto startActiveRange = [&](unsigned id) { activeStartId = activeEndId = id; };
    auto extendActiveRange = [&](unsigned id) { activeEndId = id; };
    auto isRangeEmpty = [&]() -> bool { return (activeStartId == ~(static_cast<unsigned>(0))); };
    auto isExtendRange = [&](unsigned id) -> bool { return (id == activeEndId + 1); };
    auto flushActiveRange = [&]() {
        if (activeStartId == activeEndId) {
            mEnq.decodeSingle(static_cast<int>(activeStartId));
#           ifdef DEBUG_MSG_DECODEALL
            {
                std::cerr << ">> MergeActionTracker.cc decodeAll() flushActiveRange"
                          << " single:" << activeStartId << " mMachineId:" << mMachineId << '\n';
            }
#           endif // end DEBUG_MSG_DECODEALL
#           ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
            {
                std::ostringstream ostr;
                ostr << "decodeAll(single:" << activeStartId << ')';
                pushBackDebugEncodeSequence(ostr.str());
            }
#           endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
        } else {
            mEnq.decodeRange(static_cast<int>(activeStartId), static_cast<int>(activeEndId));
#           ifdef DEBUG_MSG_DECODEALL
            {
                std::cerr << ">> MergeActionTracker.cc decodeAll() flushActiveRange"
                          << " range start:" << activeStartId << " end:" << activeEndId
                          << " mMachineId:" << mMachineId << '\n';
            }
#           endif // end DEBUG_MSG_DECODEALL
#           ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
            {
                std::ostringstream ostr;
                ostr << "decodeAll(range:" << activeStartId << '-' << activeEndId << ')';
                pushBackDebugEncodeSequence(ostr.str());
            }
#           endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
        }
        mLastSendActionId = activeEndId;
    };

    //
    // In order to minimize encoded data size, we will try to store range info
    // (for example, from 10 to 15) instead of individual sendAction id (like 10, 11, 12, 13, 14, 15).
    // This is the main logic to find range info from the input sendActionIdData vector array.
    //                                                            
    resetActiveRange();
    for (const unsigned id : sendActionIdData) {
        if (isRangeEmpty()) startActiveRange(id);
        else if (isExtendRange(id)) extendActiveRange(id);
        else {
            flushActiveRange();
            resetActiveRange();
            startActiveRange(id);
        }
    }
    flushActiveRange();

#   ifdef DEBUG_MSG_DECODEALL
    std::cerr << ">> MergeActionTracker.cc decodeAll() done mMachineId:" << mMachineId << '\n';
    // std::cerr << mEnq.showDebug() << " mMachineId:" << mMachineId << '\n';
#   endif // end DEBUG_MSG_DECODEALL
}

void
MergeActionTracker::mergeFull()
{
#   ifdef DEBUG_MSG_MERGE_FULL
    std::cerr << ">> MergeActionTracker.cc mergeFull() start\n";
#   endif // end DEBUG_MSG_MERGE_FULL

    mEnq.mergeAllTiles();
    mLastPartialMergeTileId = 0; // special case

#   ifdef DEBUG_MSG_MERGE_PARTIAL
    std::cerr << ">> MergeActionTracker.cc mergeFull() done mMachineId:" << mMachineId << '\n';
#   endif // end DEBUG_MSG_MERGE_PARTIAL

#   ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
    pushBackDebugEncodeSequence("mergeFull()");
#   endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
}

void
MergeActionTracker::mergePartial(const std::vector<char>& partialMergeTilesTbl)
{
#   ifdef DEBUG_MSG_MERGE_PARTIAL
    std::cerr << ">> MergeActionTracker.cc mergePartial() start. mMachineId:" << mMachineId << '\n';
#   endif // end DEBUG_MSG_MERGE_PARTIAL

    unsigned activeStartId, activeEndId;

    auto resetActiveRange = [&]() { activeStartId = ~0; activeEndId = ~0; };
    auto startActiveRange = [&](unsigned id) { activeStartId = activeEndId = id; };
    auto extendActiveRange = [&](unsigned id) { activeEndId = id; };
    auto isRangeActive = [&]() -> bool { return (activeStartId != ~(static_cast<unsigned>(0))); };
    auto flushActiveRange = [&]() {
        if (activeStartId == activeEndId) {
            mEnq.mergeTileSingle(static_cast<int>(activeStartId));
#           ifdef DEBUG_MSG_MERGE_PARTIAL
            {
                std::cerr << ">> MergeActionTracker.cc mergePartial() flushActiveRange"
                          << " single:" << activeStartId << " mMachineId:" << mMachineId << '\n';
            }
#           endif // end DEBUG_MSG_MERGE_PARTIAL            
#           ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
            {
                std::ostringstream ostr;
                ostr << "mergePartial(single:" << activeStartId << ')';
                pushBackDebugEncodeSequence(ostr.str());
            }
#           endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
        } else {
            mEnq.mergeTileRange(static_cast<int>(activeStartId), static_cast<int>(activeEndId));
#           ifdef DEBUG_MSG_MERGE_PARTIAL
            {
                std::cerr << ">> MergeActionTracker.cc mergePartial() flushActiveRange"
                          << " range start:" << activeStartId << " end:" << activeEndId
                          << " mMachineId:" << mMachineId << '\n';
            }
#           endif // end DEBUG_MSG_MERGE_PARTIAL            
#           ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
            {
                std::ostringstream ostr;
                ostr << "mergePartial(range:" << activeStartId << '-' << activeEndId << ')';
                pushBackDebugEncodeSequence(ostr.str());
            }
#           endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
        }
        mLastPartialMergeTileId = activeEndId;
    };

    //
    // In order to minimize encoded data size, we will try to store range tile id
    // (for example, from 10 to 15) instead of individual tile id (like 10, 11, 12, 13, 14, 15).
    // This is the main logic to find the range tile id from the input partialMergeTilesTbl.
    //                                                            
    resetActiveRange();
    for (unsigned id = 0; id < static_cast<unsigned>(partialMergeTilesTbl.size()); ++id) {
        if (static_cast<bool>(partialMergeTilesTbl[id])) {
            if (!isRangeActive()) startActiveRange(id);
            else extendActiveRange(id);
        } else {
            if (isRangeActive()) {
                flushActiveRange();
                resetActiveRange();
            }
        }
    }
    if (isRangeActive()) flushActiveRange();

#   ifdef DEBUG_MSG_MERGE_PARTIAL
    std::cerr << ">> MergeActionTracker.cc mergePartial() done mMachineId:" << mMachineId << '\n';
#   endif // end DEBUG_MSG_MERGE_PARTIAL
}

void
MergeActionTracker::encodeData(scene_rdl2::cache::CacheEnqueue& enqueue)
{
    mEnq.endOfData(); // executes finalize internally
#   ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
    pushBackDebugEncodeSequence("endOfdata()");
#   endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO

    size_t dataSize = mData.size();
#   ifdef DEBUG_MSG_ENCODE_DATA
    std::cerr << ">> MergeActionTracker.cc encodeData() dataSize:" << dataSize << '\n';
#   endif // end DEBUG_MSG_ENCODE_DATA
    enqueue.enqVLSizeT(dataSize);
    if (dataSize > 0) {
        enqueue.enqByteData(mData.data(), dataSize);

#       ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
        {
            std::cerr << ">> MergeActionTracker.cc encodeData() mDebugEncodeSequence {\n"
                      << scene_rdl2::str_util::addIndent(mDebugEncodeSequence) << '\n'
                      << "} mMachineId:" << mMachineId << '\n';
        }
#       endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO

#       ifdef DEBUG_MSG_ENCODE_DATA
        std::cerr << scene_rdl2::rdl2::ValueContainerUtil::hexDump(">> MergeActionTracker.cc encodeData()",
                                                                   mData.data(), dataSize)
                  << " mMachineId:" << mMachineId << '\n';
#       endif // end DEBUG_MSG_ENCODE_DATA
    }

    //------------------------------

    resetEncode();
}

// static function
void
MergeActionTracker::decodeDataSkipOnMCRTComputation(scene_rdl2::cache::CacheDequeue& dequeue)
{
    size_t dataSize = dequeue.deqVLSizeT();
    if (dataSize) {
        dequeue.skipByteData(dataSize);
#       ifdef DEBUG_MSG_DECODE_DATA_SKIP
        std::cerr << ">> MergeActionTracker.cc decodedataSkipOnMCRTComputation() "
                  << "dataSize:" << dataSize << '\n';
#       endif // end DEBUG_MSG_DECODE_DATA_SKIP
    }
}

void
MergeActionTracker::decodeDataOnMCRTComputation(scene_rdl2::cache::CacheDequeue& dequeue)
{
    mLastSendActionId = 0;
    mLastPartialMergeTileId = 0;

    size_t dataSize = dequeue.deqVLSizeT();
#   ifdef DEBUG_MSG_DECODE_DATA
    std::cerr << ">> MergeActionTracker.cc decodeDataOnMCRTComputation() dataSize:" << dataSize << '\n';
#   endif // end DEBUG_MSG_DECODE_DATA
    if (!dataSize) {
        mData.clear();
        return;
    }
  
    mData.resize(dataSize);
    dequeue.deqByteData(&mData[0], dataSize);

#   ifdef DEBUG_MSG_DECODE_DATA
    std::cerr
        << scene_rdl2::rdl2::ValueContainerUtil::
        hexDump(">> MergeActionTracker.cc decodeDataOnMCRTComputation() mData dump",
                &mData[0], dataSize) << '\n';
    std::cerr << ">> MergeActionTracker.cc decodeDataOnMCRTComputation() done\n";
#   endif // end DEBUG_MSG_DECODE_DATA
}

std::string
MergeActionTracker::dumpData() const
{
    std::ostringstream ostr;
    ostr << "MergeActionTracker {\n"
         << "  mData.size():" << mData.size() << '\n'
         << scene_rdl2::str_util::addIndent(dumpDataAsAscii(mData)) << '\n'
         << "}";
    return ostr.str();
}

//------------------------------------------------------------------------------------------

// static function
std::string
MergeActionTracker::dumpDataAsAscii(const std::string& data)
{
    std::ostringstream ostr;

    MergeSequenceDequeue deq(data.data(), data.size());
    std::string error;
    if (!deq.decodeLoop(error,
                        [&](unsigned sendImageActionId) -> bool { // decodeSingle
                            ostr << "decodeSingle " << sendImageActionId << ',';
                            return true;
                        },
                        [&](unsigned start, unsigned end) -> bool { // decodeRange
                            ostr << "decodeRange " << start << ' ' << end << ',';
                            return true;
                        },
                        [&](unsigned tileId) -> bool { // tileSingle
                            ostr << "tileSingle " << tileId << ',';
                            return true;
                        },
                        [&](unsigned start, unsigned end) -> bool { // tileRange
                            ostr << "tileRange " << start << ' ' << end << ',';
                            return true;
                        },
                        [&]() -> bool { // tileAll
                            ostr << "tileAll,";
                            return true;
                        },
                        [&]() -> bool { // endOfData
                            ostr << "endOfData";
                            return true;
                        })) {
        ostr << "decode failed. error:>" << error << "<";
    }
    return ostr.str();
}

#ifdef MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO
void
MergeActionTracker::pushBackDebugEncodeSequence(const std::string& str)
{
    if (mDebugEncodeSequence.empty()) {
        mDebugEncodeSequence = str;
    } else {
        mDebugEncodeSequence += '\n' + str;
    }
}
#endif // end MERGE_ACTION_TRACKER_DEBUG_ENCODE_INFO

} // namespace mcrt_dataio
