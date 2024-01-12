// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TestMergeTracker.h"

#include <mcrt_dataio/engine/merger/MergeActionTracker.h>

//#define DEBUG_MSG

namespace mcrt_dataio {
namespace unittest {

void
TestMergeTracker::testCodec()
{
    CPPUNIT_ASSERT("testCodec" &&
                   main(std::string("decodeAll 12 13 15 16 17 -1,"
                                    "mergeFull,"
                                    "mergePartial t t f f f t f f t t t e"),
                        std::string("MergeActionTracker {\n"
                                    "  mData.size():24\n"
                                    "  decodeRange 12 13,decodeRange 15 17,tileAll,tileRange 0 1,"
                                    "tileSingle 5,tileRange 8 10,endOfData\n"
                                    "}")));
}

bool
TestMergeTracker::main(const std::string& input, const std::string& target) const
{
    std::string data;
    if (!encodeTestData(input, data)) {
        return false;
    }

    return decodeTestData(data) == target;
}

bool
TestMergeTracker::encodeTestData(const std::string& input, std::string& outData) const
{
    outData.clear();
    scene_rdl2::cache::CacheEnqueue enqueue(&outData);

    std::vector<scene_rdl2::grid_util::Arg> cmds = convertToArgs(input);

    MergeActionTracker mergeActionTracker;
    for (size_t i = 0; i < cmds.size(); ++i) {
        scene_rdl2::grid_util::Arg& arg = cmds[i];

        const std::string cmd = (arg++)();
        if (cmd == "decodeAll") {
            std::vector<unsigned> sendActionIdData;
            while (true) {
                int id = (arg++).as<int>(0);
                if (id == -1) break;
                sendActionIdData.push_back(static_cast<unsigned>(id));
            }
            mergeActionTracker.decodeAll(sendActionIdData);
            
#           ifdef DEBUG_MSG
            std::ostringstream ostr;
            ostr << "decodeAll ";
            for (size_t j = 0; j < sendActionIdData.size(); ++j) {
                ostr << sendActionIdData[j] << ' ';
            }
            ostr << -1;
            std::cerr << ostr.str() << '\n';
#           endif // DEBUG_MSG
        } else if (cmd == "mergeFull") {
            mergeActionTracker.mergeFull();

#           ifdef DEBUG_MSG
            std::cerr << "mergeFull\n";
#           endif // end DEBUG_MSG
        } else if (cmd == "mergePartial") {
            std::vector<char> partialMergeTilesTbl;
            while (true) {
                std::string curr = (arg++)();
                if (curr == "e") break;
                if (curr[0] == 't') partialMergeTilesTbl.push_back(static_cast<char>(true));
                else partialMergeTilesTbl.push_back(static_cast<char>(false));
            }
            mergeActionTracker.mergePartial(partialMergeTilesTbl);

#           ifdef DEBUG_MSG
            std::ostringstream ostr;
            ostr << "mergePartial ";
            for (size_t j = 0; j < partialMergeTilesTbl.size(); ++j) {
                ostr << (static_cast<bool>(partialMergeTilesTbl[j]) ? 't' : 'f') << ' ';
            }
            ostr << 'e';
            std::cerr << ostr.str() << '\n';
#           endif // end DEBUG_MSG
        } else {
            std::cerr << "ERROR : encode failed. unknown input command:" << cmd << '\n';
            return false;
        }
    }

    mergeActionTracker.encodeData(enqueue);
    enqueue.finalize();

    return true;
}

std::vector<scene_rdl2::grid_util::Arg>
TestMergeTracker::convertToArgs(const std::string& input) const
{
    std::vector<scene_rdl2::grid_util::Arg> cmds;
    std::string work;
    std::stringstream ss{input};
    while (std::getline(ss, work, ',')) {
        cmds.emplace_back("", work);
    }
    return cmds;
}

std::string
TestMergeTracker::decodeTestData(const std::string& data) const
{
    scene_rdl2::cache::CacheDequeue dequeue(data.data(), data.size());

    MergeActionTracker mergeActionTracker;
    mergeActionTracker.decodeDataOnMCRTComputation(dequeue);
    return mergeActionTracker.dumpData();
}

} // namespace unittest
} // namespace mcrt_dataio
