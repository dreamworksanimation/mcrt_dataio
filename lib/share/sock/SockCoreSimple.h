// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#pragma once

#include <stddef.h>             // size_t

namespace mcrt_dataio {

class SockCoreSimple
//
// This class is a wrapper for very basic send/receive operation for single socket
// communication as busy mode.
//
{
public:
    static const int RECV_STATUS_EOF   = -1;
    static const int RECV_STATUS_ERROR = -2;

    SockCoreSimple() :
        mSock(-1)
    {}
    ~SockCoreSimple() { close(); }

    void setSock(int sock) { mSock = sock; }
    
    // blocking send
    bool busySend(const void *sendBuff, const size_t sendByteSize)
    {
        return sendData(sendBuff, sendByteSize);
    }

    // blocking busy receive
    // return received data size (positive or 0) or error code (negative)
    //                 +n : receive data size byte
    //                  0 : skip receive operation
    //    RECV_STATUS_EOF : EOF (negative value)
    //  RECV_STATUS_ERROR : error (negative value)
    int busyRecv(void *recvBuff, const size_t recvByteSize);

    // blocking close
    void close();

private:
    int mSock;

    bool sendData(const void *sendBuff, const size_t sendByteSize); // busy send

    // return received data size (positive or 0) or error code (negative)
    //                 +n : receive data size byte
    //                  0 : skip receive operation
    //    RECV_STATUS_EOF : EOF (negative value)
    //  RECV_STATUS_ERROR : error (negative value)
    int recvData(void *recvBuff, const size_t recvByteSize); // busy read

    void connectionClosed();
    void closeSock();
};

} // namespace mcrt_dataio

