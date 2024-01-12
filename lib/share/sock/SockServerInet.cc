// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "SockServerInet.h"
#include "SockServerConnection.h"

#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <arpa/inet.h>          // inet_ntoa()
#include <netinet/in.h>         // struct sockaddr_in
#include <string.h>             // memset
#include <sys/socket.h>         // socket()
#include <unistd.h>             // read()/write()/close()

namespace mcrt_dataio {

bool
SockServerInet::open(const int serverPortNumber)
{
    mPort = serverPortNumber;

    if (mPort == 0) {
        //
        // This is a special case. 
        // We have to open server port (bind/listen) here with searching the unused port
        // automatically. You can get opened port number by getPortNum() after complete
        // this function.
        // Otherwise open socket port will be processed when newClientConnection() is called.
        // (i.e. delaied access).
        // This means if you never call newClientConnection() API, socket is never opened
        // even you define port number.
        //
        if (!baseSockBindAndListen()) {
            return false;
        }
    }

    return true;
}

SockServerInet::ConnectionShPtr
SockServerInet::newClientConnection()
{
    if (mBaseSock == -1) {
        if (!baseSockBindAndListen()) {
            return ConnectionShPtr(nullptr); // Failed to open and setup baseSock
        }
    }

    int cSock;
    std::string clientHost;
    int clientPort;
    if (!acceptNewSocket(&cSock, clientHost, &clientPort)) {
        return ConnectionShPtr(nullptr); // accept new socket operation failed.
    }

    if (cSock == -1) {
        return ConnectionShPtr(nullptr); // no active socket -> try again later
    }

    //
    // We got new active socket
    //
    ConnectionShPtr cConnection(new SockServerConnection);
    cConnection->setInetSock(cSock, clientHost, clientPort);

    std::ostringstream ostr;
    ostr << "new inet domain connection (" << clientHost << " port:" << clientPort << ") "
         << "was established ...";
    std::cerr << ">> SockServerInet.cc " << ostr.str() << '\n';

    return cConnection;
}

//------------------------------------------------------------------------------------------

bool    
SockServerInet::baseSockBindAndListen()
{
    if (mBaseSock != -1) return true;

    if ((mBaseSock = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        mBaseSock = -1;
        return false;
    }

    // keepAlive ?
    // This code, we don't use keepAlive setting here. We should consider keepAliev
    // configuration if this communication is used under very low frequency with
    // unreliable connection. At this moment, without keepAlive setting looks reasonable.

    //
    // Set the close-on-exec flag so that the socket will not get inherited by child processes
    //
    fcntl(mBaseSock, F_SETFD, FD_CLOEXEC);

    //
    // set non blocking ::accept()
    //
    fcntl(mBaseSock, F_SETFL, FNDELAY);

    //
    // Set up the reuse server addresses automatically and bind to the specified port.
    //
    int status = 1;
    (void)setsockopt(mBaseSock, SOL_SOCKET, SO_REUSEADDR, (char *)&status, sizeof(status));
    if (status == -1) {
        closeBaseSock();
        return false;
    }

    //
    // Setup socket info
    //
    struct sockaddr_in in;
    memset((void *)&in, 0x0, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = INADDR_ANY;
    in.sin_port = htons(static_cast<uint16_t>(mPort)); // put in by net order

    //
    // bind the socket to the port number
    //
    if (::bind(mBaseSock, (struct sockaddr *)&in, sizeof(in)) < 0) {
        closeBaseSock();
        return false;
    }

    if (mPort == 0) {
        //
        // grab the port of the server
        //
        socklen_t inLen = sizeof(in);
        if (::getsockname(mBaseSock, (sockaddr *)&in, &inLen) != 0) {
            closeBaseSock();
            return false;
        }
        mPort = ntohs(in.sin_port);
    }

    //
    // do listen
    //
    if (::listen(mBaseSock, 5) < 0) {
        closeBaseSock();
        return false;
    }

    return true;
}

bool
SockServerInet::acceptNewSocket(int *sock, std::string &clientHost, int *clientPort) const
{
    struct sockaddr_in client;
    unsigned int addrlen = sizeof(client);

    if ((*sock = ::accept(mBaseSock, (struct sockaddr *)&client, &addrlen)) == -1) {
        int errNum = errno;
        if (errNum == EAGAIN) { // Resource temporarily unavailable
            return true;        // retry again : *sock = -1

        } else if (errNum == EMFILE || // too many open files
                   errNum == ENFILE) { // too many open files in system
            std::cerr << ">> SockServerInet.cc ERROR : acceptNewSocket(): "
                      << "error:" << errno << " (" << strerror(errno) << ")\n";
            return false;   // error
                
        } else if (errNum == EINTR) {
            //
            // interrupted by other system call -> we should try again ?
            // Please check Unix Network Programming P.67
            //
            std::cerr << ">> SockServerInet.cc ERROR : acceptNewSocket(): "
                      << "error:" << errno << " (" << strerror(errno) << ")\n";
            return false;   // error

        } else {
            std::cerr << ">> SockServerInet.cc ERROR : acceptNewSocket(): "
                      << "error:" << errno << " (" << strerror(errno) << ")\n";
            return false;   // error
        }
    }

    clientHost = inet_ntoa(client.sin_addr);
    *clientPort = ntohs(client.sin_port);

    return true;
}

void    
SockServerInet::closeBaseSock()
{
    if (mBaseSock != -1) {
        ::close(mBaseSock);
        mBaseSock = -1;
    }
}

} // namespace mcrt_dataio

