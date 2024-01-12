// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "MergeSequenceEnqueue.h"

#include <scene_rdl2/render/util/StrUtil.h>

namespace mcrt_dataio {

std::string    
MergeSequenceEnqueue::showDebug() const
{
    return scene_rdl2::str_util::stringCat("MergeSequenceEnqueue {\n",
                                           scene_rdl2::str_util::addIndent(mEnqueue.showDebug()), '\n',
                                           "}");

}

} // namespace mcrt_dataio
