// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include "McrtNodeInfo.h"

#include <mcrt_dataio/share/codec/InfoCodec.h>

#include <string>

namespace mcrt_dataio {

class McrtNodeInfoMapItem
//
// This class is a wrapper class of grid_util::McrtNodeInfo and information wise this class does not add any
// information to the grid_util::McrtNodeInfo. However there is a reason to use this wrapper class instead
// of directly using grid_util::McrtNodeInfo.
//
// The McrtNodeInfo is finally sent to the merge computation node and is stored inside 
// mcrt_dataio::GlobalNodeInfo as one element of mcrtNodeInfoMap table. In order to accomplish this we have
// to convert McrtNodeInfo as globalNodeInfo/mcrtNodeInfoMap[mcrtId] data. This class is required to do this
// conversion. See constructor of this class and encode function for more detail.
//
// If you want to add additional infoRec related information to this object, you should add inside
// grid_util::McrtNodeInfo instead of member of this class.
//
{
public:
    McrtNodeInfoMapItem();

    McrtNodeInfo &getMcrtNodeInfo() { return mMcrtNodeInfo; }

    bool encode(std::string &outputData);

private:

    McrtNodeInfo mMcrtNodeInfo;

    InfoCodec mInfoCodec;
};

} // namespace mcrt_dataio

