// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "SockServerUnix.h"
#include "SockServerConnection.h"

#include <cstring>              // memset()
#include <fcntl.h>              // fcntl
#include <iostream>
#include <sstream>
#include <linux/un.h>           // UNIX_PATH_MAX
#include <sys/socket.h>
#include <unistd.h>             // read()/write()/close()

namespace mcrt_dataio {

SockServerUnix::~SockServerUnix()    
{
    unlink(mPath.c_str());
}

bool
SockServerUnix::open(const std::string &path, int port)
{
    if (path.empty()) return false;

    std::ostringstream ostr;
    ostr << path << '.' << port; // create unix-domain path with port number

    mPath = ostr.str();
    if (mPath.size() > (UNIX_PATH_MAX - 1)) return false;

    return true;
}

SockServerUnix::ConnectionShPtr
SockServerUnix::newClientConnection()
{
    if (mBaseSock == -1) {
        //
        // Open and bind/listen procedure is executed by delayed manner.
        // Socket is constructed when the first time of newClientConnection is called.
        // Until then the socket is never opened.
        //            
        if (!baseSockBindAndListen()) {
            return ConnectionShPtr(nullptr);
        }
    }

    int cSock;
    std::string clientPath;
    if (!acceptNewSocket(&cSock, clientPath)) {
        return ConnectionShPtr(nullptr);
    }

    if (cSock == -1) {
        return ConnectionShPtr(nullptr); // no active socket -> try again
    }

    //
    // We got new active socket
    //
    ConnectionShPtr cConnection(new SockServerConnection);
    cConnection->setUnixSock(cSock, clientPath);

    std::stringstream ostr;
    ostr << "new unix domain connection (" << clientPath << ") was establised ...";
    std::cerr << ">> SockServerUnix.cc " << ostr.str() << '\n';

    return cConnection;
}

bool    
SockServerUnix::baseSockBindAndListen()
{
    if (mBaseSock != -1) return true;

    if ((mBaseSock = ::socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        mBaseSock = -1;
        return false;           // socket() error
    }

    if (mPath[0] != '@') {
        unlink(mPath.c_str());      // first of all unlink domain socket path file
    }

    // keepAlive ?
    // this code, we don't use keepAlive setting here. We should consider keepAlie
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
    // Set up the reuse server addresses automatically and bind to the specified path.
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
    struct sockaddr_un un;
    memset((void *)&un, 0x0, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, mPath.c_str());
    size_t len = sizeof(un.sun_family) + strlen(un.sun_path);

    if (mPath[0] == '@') {
        un.sun_path[0] = 0x0;   // set abstruct namespace mode
    }

    //
    // bind the socket to the path
    //
    if (::bind(mBaseSock, (sockaddr *)&un, static_cast<socklen_t>(len)) < 0) {
        closeBaseSock();
        return false;
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
SockServerUnix::acceptNewSocket(int *sock, std::string &clientPath) const
{
    struct sockaddr_un client;
    unsigned int addrlen = sizeof(client);
    memset(&client, 0x0, sizeof(client));

    if ((*sock = ::accept(mBaseSock, (struct sockaddr *)&client, &addrlen)) == -1) {
        int errNum = errno;
        if (errNum == EAGAIN) { // Resource temporarily unavailable
            return true;        // retry again : *sock = -1

        } else if (errNum == EMFILE ||
                   errNum == ENFILE) {
            std::cerr << ">> SockServerUnix.cc ERROR : acceptNewSocket(): "
                      << "error:" << errno << " (" << strerror(errno) << ")\n";
            return false;       // error
            
        } else if (errNum == EINTR) {
            //
            // interrupted by other system call -> we should try again ?
            // Please check Unix Network Programming P.67
            //
            std::cerr << ">> SockServerUnix.cc ERROR : acceptNewSocket(): "
                      << "error:" << errno << " (" << strerror(errno) << ")\n";
            return false;   // error

        } else {
            std::cerr << ">> SockServerUnix.cc ERROR : acceptNewSocket(): "
                      << "error:" << errno << " (" << strerror(errno) << ")\n";
            return false;   // error
        }
    }

    // clientPath = client.sun_path; // Somehow we can not get path from accept()
    clientPath = mPath;

    return true;
}

void    
SockServerUnix::closeBaseSock()
{
    if (mBaseSock != -1) {
        ::close(mBaseSock);
        mBaseSock = -1;
    }
}

} // namespace mcrt_dataio

