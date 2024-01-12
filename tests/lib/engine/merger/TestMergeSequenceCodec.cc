// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TestMergeSequenceCodec.h"

#include <mcrt_dataio/engine/merger/MergeSequenceEnqueue.h>
#include <mcrt_dataio/engine/merger/MergeSequenceDequeue.h>

#include <scene_rdl2/common/grid_util/Arg.h>

#include <string>
#include <sstream>
#include <vector>

namespace mcrt_dataio {
namespace unittest {

void
TestMergeSequenceCodec::testSequence()
{
    CPPUNIT_ASSERT("testSequence" &&
                   main(std::string("decodeSingle 10,"
                                    "mergeAllTiles,"
                                    "decodeSingle 11,"
                                    "mergeTileRange 123 234,"
                                    "decodeRange 12 21,"
                                    "mergeTileSingle 235,"
                                    "mergeTileRange 236 456,"
                                    "decodeSingle 22,"
                                    "mergeAllTiles,"
                                    "endOfData")));
}
    
bool
TestMergeSequenceCodec::main(const std::string& input) const
{
    std::vector<scene_rdl2::grid_util::Arg> cmds;
    std::string work;
    std::stringstream ss{input};
    while (std::getline(ss, work, ',')) {
        cmds.emplace_back("", work);
    }

    //------------------------------

    std::string data;
    MergeSequenceEnqueue enq(&data);
    for (size_t i = 0; i < cmds.size(); ++i) {
        scene_rdl2::grid_util::Arg& arg = cmds[i];
        
        const std::string cmd = (arg++)();

        if (cmd == "decodeSingle") {
            enq.decodeSingle(arg.as<int>(0));
        } else if (cmd == "decodeRange") {
            enq.decodeRange(arg.as<int>(0), arg.as<int>(1));
        } else if (cmd == "mergeTileSingle") {
            enq.mergeTileSingle(arg.as<int>(0));
        } else if (cmd == "mergeTileRange") {
            enq.mergeTileRange(arg.as<int>(0), arg.as<int>(1));
        } else if (cmd == "mergeAllTiles") {
            enq.mergeAllTiles();
        } else if (cmd == "endOfData") {
            enq.endOfData();
        } else {
            std::cerr << "ERROR : encode failed. unknown input command:" << cmd << '\n';
            return false;
        }
    }

    //------------------------------

    mcrt_dataio::MergeSequenceDequeue deq(data.data(), data.size());

    std::string output;
    std::string errorMsg;
    if (!deq.decodeLoop(errorMsg,
                        [&](unsigned int sendImageActionId) -> bool { // decodeSingleFunc
                            std::ostringstream ostr;
                            ostr << "decodeSingle " << sendImageActionId << ',';
                            output += ostr.str();
                            return true;
                        },
                        [&](unsigned int startSendImageActionId,
                            unsigned int endSendImageActionId) -> bool { // decodeRangeFunc
                            std::ostringstream ostr;
                            ostr << "decodeRange " << startSendImageActionId
                                 << ' ' << endSendImageActionId << ',';
                            output += ostr.str();
                            return true;
                        },
                        [&](unsigned int tileId) -> bool { // mergeTileSingleFunc
                            std::ostringstream ostr;
                            ostr << "mergeTileSingle " << tileId << ',';
                            output += ostr.str();
                            return true;
                        },
                        [&](unsigned int startTileId, unsigned int endTileId) -> bool { // mergeTileRangeFunc
                            std::ostringstream ostr;
                            ostr << "mergeTileRange " << startTileId << ' ' << endTileId << ',';
                            output += ostr.str();
                            return true;
                        },
                        [&]() -> bool { // mergeAllTiles
                            output += "mergeAllTiles,";
                            return true;
                        },
                        [&]() -> bool { // eodFunc
                            output += "endOfData";
                            return true;
                        })) {
        std::cerr << "ERROR : decode failed. " << errorMsg << '\n';
    }
    
    if (input != output) {
        std::cerr << " input:" << input << '\n'
                  << "output:" << output << '\n';
    }

    return input == output;
}

} // namespace unittest
} // namespace mcrt_dataio
