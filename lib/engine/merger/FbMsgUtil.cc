// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "FbMsgUtil.h"

#include <iomanip>
#include <sstream>

namespace mcrt_dataio {

// static function
std::string
FbMsgUtil::hexDump(const std::string &hd,
                   const std::string &titleMsg,
                   const void *buff,
                   const size_t size,
                   const size_t maxDisplaySize) // 0 : no limit
//
// general purpose hexadecimal dump
//
{
    const char *startBuff = static_cast<const char *>(buff);
    const char *endBuff   = &startBuff[size];
    if (maxDisplaySize) {
        endBuff = &startBuff[std::min(maxDisplaySize, size)];
    }

    std::ostringstream ostr;

    ostr << hd << "hexDump";
    if (titleMsg.size() > 0) ostr << " " << titleMsg;
    ostr << " size:" << size << " {\n";

    const char *cPtrA = startBuff;
    const char *cPtrB = startBuff;

    static const char *SEPARATOR = "-";

    int itemCount = 0;
    while (1) {
        if (cPtrA == endBuff) {
            if (itemCount != 0) {
                for (int i = itemCount + 1; i <= 16; i++) {
                    ostr << "  ";
                    if (i == 8) {
                        ostr << " " << SEPARATOR << " ";
                    } else {
                        ostr << " ";
                    }
                }
                ostr << " |  ";
                for (int i = 0; i < itemCount; i++) {
                    if (isprint(static_cast<int>(*cPtrB))) ostr << *cPtrB << " ";
                    else                                   ostr << "  ";
                    if (i == 7) ostr << " " << SEPARATOR << "  ";
                    cPtrB++;
                }
                ostr << std::endl;
            }
            break;
        }

        itemCount++;

        if (itemCount == 1) {
            size_t cSize = (uintptr_t)cPtrA - (uintptr_t)startBuff;
            ostr << hd << "  0x" << std::hex << std::setw(4) << std::setfill('0') << cSize << std::dec << ": ";
        }

        ostr << std::setw(2) << std::setfill('0') << std::hex << ((static_cast<int>(*cPtrA)) & 0xff)
             << std::dec;

        if (itemCount == 16) {
            ostr << "  |  \" ";
            for (int i = 0; i < 16; ++i) {
                if ((*cPtrB) == '"') ostr << "\\" << *cPtrB;
                else if (isprint(static_cast<int>(*cPtrB))) ostr << " " << *cPtrB;
                else ostr << "  ";
                if (i == 7) ostr << " " << SEPARATOR << "  ";
                cPtrB++;
            }
            ostr << " \"" << std::endl;
            itemCount = 0;
        } else if (itemCount == 8) {
            ostr << " " << SEPARATOR << " ";
        } else {
            ostr << " ";
        }

        cPtrA++;
    }

    if (maxDisplaySize < size) {
        ostr << hd << "  ... skip output ...\n";
    }
    ostr << hd << "}";

    return ostr.str();
}

} // namespace mcrt_dataio
