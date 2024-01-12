// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "SockCoreSimple.h"

#include <cstdint>              // uintptr_t
#include <cstring>              // strerror
#include <errno.h>
#include <sstream>
#include <unistd.h>             // read()/write()/close()

namespace mcrt_dataio {

int
SockCoreSimple::busyRecv(void *recvBuff, const size_t recvByteSize)
//
// return received data size (positive or 0) or error code (negative)
//                 +n : receive data size byte
//                  0 : skip receive operation
//    RECV_STATUS_EOF : EOF (negative value)
//  RECV_STATUS_ERROR : error (negative value)
//
{
    if (recvByteSize == 0) return 0; // skip
    return recvData(recvBuff, recvByteSize);
}

void
SockCoreSimple::close()
{
    closeSock();
}

//------------------------------------------------------------------------------------------

bool
SockCoreSimple::sendData(const void *sendBuff, const size_t sendByteSize)
//
// blocking busy send
//
{
    if (mSock == -1) return true; // closed socket case. -> skip send and return true

    fd_set orig_mask, curr_mask;
    FD_ZERO(&orig_mask);
    FD_SET(mSock, &orig_mask);

    const void *cDataPtr = sendBuff;
    size_t nleft = sendByteSize;
    while (1) {
        //
        // blocking wait until socket is ready
        //
        curr_mask = orig_mask; // need init due to select overwrite memory
        select(mSock+1, nullptr, &curr_mask, nullptr, nullptr); // block until mSock is ready to write

        //
        // send data
        //
        ssize_t sentByte = ::write(mSock, cDataPtr, nleft);
        if (sentByte == 0) {
            continue;     // retry without wait
        } else if (sentByte < 0) {
            // We should add more error code detection routine here.
            if (errno == EAGAIN) continue; // no wait retry
            if (errno == EINTR) continue; // no wait retry
            if (errno == EPIPE) { // Broken pipe
                connectionClosed();
                return false;   // We should return false this case
            }
        } else {
            nleft -= static_cast<size_t>(sentByte);
            if (nleft == 0) break; // sent all -> finish
            cDataPtr = (const void *)((uintptr_t)cDataPtr + (uintptr_t)sentByte);
        }
    } // while (1)

    return true;
}

int
SockCoreSimple::recvData(void *recvBuff, const size_t recvByteSize)
//
// blocking busy read. You have to specify recvByteSize.
// return received data size (positive or 0) or error code (negative)
//                 +n : received data size
//                  0 : skip recv operation
//    RECV_STATUS_EOF : EOF (negative value)
//  RECV_STATUS_ERROR : error (negative value)
//
{
    if (recvByteSize == 0) return 0; // skip recv operation

    fd_set orig_mask, curr_mask;
    FD_ZERO(&orig_mask);
    FD_SET(mSock, &orig_mask);

    size_t leftSize = recvByteSize;

    void *readPtr = (void *)((uintptr_t)recvBuff);

    //
    // retry loop
    //
    size_t completedSize = 0;
    while (1) {
        //
        // blocking wait until socket is ready
        //
        curr_mask = orig_mask; // need init due to select overwrite memory
        select(mSock+1, &curr_mask, nullptr, nullptr, nullptr); // block until mSock is ready to read

        //
        // receive data
        //
        ssize_t size = ::read(mSock, (void *)readPtr, leftSize);

        if (size == 0) {
            connectionClosed();
            return RECV_STATUS_EOF; // EOF
            
        } else if (size > 0) {
            completedSize += static_cast<size_t>(size);
            leftSize -= static_cast<size_t>(size);
            if (leftSize == 0) {
                break;
            }

            readPtr = (void *)((uintptr_t)recvBuff + (uintptr_t)completedSize);
            // try again
            
        } else {
            if (errno == EAGAIN) {
                // try again
            } else if (errno == EBADF) { // Bad file descriptor
                //
                // Probably other side of socket is killed somehow
                //
                connectionClosed();
                return RECV_STATUS_EOF;      // EOF

            } else {
                /* Needs more work for error message
                std::ostringstream ostr;
                ostr << "unknown socket receive error. read():" << size << " "
                     << "errno:" << errno << " (" << strerror(errno) << ")";
                */
                return RECV_STATUS_ERROR;      // error
            }
        }
    }

    return static_cast<int>(completedSize); // return total received data size
}


void
SockCoreSimple::connectionClosed()
// unexpected connection closed
{
    closeSock();

    // dead hook operation is here
}

void    
SockCoreSimple::closeSock()
{
    if (mSock != -1) {
        ::close(mSock);
        mSock = -1;
    }
}

} // namespace mcrt_dataio

