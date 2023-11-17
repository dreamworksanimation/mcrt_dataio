// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "McrtNodeInfoMapItem.h"

namespace mcrt_dataio {

McrtNodeInfoMapItem::McrtNodeInfoMapItem()
    : mMcrtNodeInfo(/* decodeOnly = */ false,
                    /* valueKeepDurationSec = */ 0.0f)
    , mInfoCodec("globalNodeInfo", false) // decodeOnly = false
{
}

bool
McrtNodeInfoMapItem::encode(std::string &outputData)
{
    mMcrtNodeInfo.flushEncodeData();
    mInfoCodec.encodeTable("mcrtNodeInfoMap",
                           std::to_string(mMcrtNodeInfo.getMachineId()),
                           mMcrtNodeInfo.getInfoCodec());
    return mInfoCodec.encode(outputData);
}

} // namespace mcrt_dataio
