// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

//
// -- utility functions for FbMsg --
//
// Utility functions for debugging which related FbMsg* classes
//

#include <string>

namespace mcrt_dataio {

class FbMsgUtil
{
public:
    static std::string hexDump(const std::string &hd,
                               const std::string &titleMsg,
                               const void *buff,
                               const size_t size,
                               const size_t maxDisplaySize = 0);
}; // FbMsgUtil

} // namespace mcrt_dataio
