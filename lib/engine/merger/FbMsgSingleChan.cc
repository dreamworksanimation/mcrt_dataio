// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "FbMsgSingleChan.h"
#include "FbMsgUtil.h"

#include <scene_rdl2/common/grid_util/LatencyLog.h>
#include <scene_rdl2/render/util/StrUtil.h>

namespace mcrt_dataio {

void
FbMsgSingleChan::encode(scene_rdl2::rdl2::ValueContainerEnq &vContainerEnq) const
//
// encode data and store into vContainerEnq : used by latencyLog info
//
{
    vContainerEnq.enqVLSizeT(mDataArray.size());
    for (size_t i = 0; i < mDataArray.size(); ++i) {
        vContainerEnq.enqVLSizeT(mDataSize[i]);
        vContainerEnq.enqByteData(mDataArray[i].get(), mDataSize[i]);
    }
}

std::string
FbMsgSingleChan::showLatencyLog(const std::string &hd) const
//
// This is special API just for debug purpose. Only works if this message is "latencyLog"     
//
{
    scene_rdl2::grid_util::LatencyLog latencyLog;

    std::ostringstream ostr;
    ostr << hd << "latencyLog (total:" << mDataArray.size() << ") {\n";
    for (size_t i = 0; i < mDataArray.size(); ++i) {
        latencyLog.decode(static_cast<const void *>(mDataArray[i].get()), mDataSize[i]);
        ostr << latencyLog.show(hd + "  ") << '\n';
    }
    ostr << hd << '}';
    return ostr.str();
}

std::string
FbMsgSingleChan::show(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd
         << "FbMsgSingleChan (DataType:" << dataTypeStr(mDataType) << ") "
         << "(total:" << mDataArray.size() << ") {\n";
    for (size_t i = 0; i < mDataArray.size(); ++i) {
        size_t maxDispSize = 1024;
        ostr << FbMsgUtil::hexDump(hd + "  ",
                                   "i:" + std::to_string(i),
                                   mDataArray[i].get(),
                                   mDataSize[i],
                                   maxDispSize)
             << '\n';
    }
    ostr << hd << "}";
    return ostr.str();
}

std::string
FbMsgSingleChan::show() const
{
    const int w = scene_rdl2::str_util::getNumberOfDigits(static_cast<unsigned>(mDataSize.size()));

    std::ostringstream ostr;
    ostr << "FbMsgSingleChan"
         << " (DataType:" << dataTypeStr(mDataType) << ")"
         << " (total:" << mDataArray.size() << ") {\n";
    for (size_t i = 0; i < mDataSize.size(); ++i) {
        ostr << "  i:" << std::setw(w) << i
             << " size:" << scene_rdl2::str_util::byteStr(mDataSize[i])
             << " dataAddr:0x" << std::hex << reinterpret_cast<uintptr_t>(mDataArray[i].get()) << std::dec
             << '\n';
    }
    ostr << "}";
    return ostr.str();
}

// static function
std::string
FbMsgSingleChan::dataTypeStr(const DataType type)
{
    switch (type) {
    case DataType::FB_DATA: return "FB_DATA";
    case DataType::LATENCY_LOG: return "LATENCY_LOG";
    case DataType::VEC_PACKET: return "VEC_PACKET";
    case DataType::UNKNOWN: return "UNKNOWN";
    default : return "?";
    }
}

} // namespace mcrt_dataio
