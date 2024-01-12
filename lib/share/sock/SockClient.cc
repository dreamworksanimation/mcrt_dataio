// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "SockClient.h"

#include <scene_rdl2/common/grid_util/LiteralUtil.h>
#include <scene_rdl2/common/grid_util/SockUtil.h>

#include <chrono>
#include <cstring>              // strerror
#include <iostream>
#include <sstream>
#include <thread>
#include <netdb.h>              // gethostbyname()
#include <linux/un.h>           // sockaddr_un
#include <netinet/in.h>         // struct sockaddr_in
#include <netinet/tcp.h>        // TCP_NODELAY
#include <signal.h>
#include <unistd.h>

using namespace scene_rdl2::grid_util; // for user defined literals of LiteralUtil.h

namespace {

static bool
getHostByName(const std::string &serverHostName, const int serverPortNumber,
              struct sockaddr_in *in)
{
    //
    // find out desired host
    //
    // Sometime host info is corrupt even some info is returned (h_addr_list[0] is null). 
    // In this case, we should try gethostbyname() again.
    //
    struct hostent *hp;

    static const int retryMax = 16;
    for (int i = 1; i <= retryMax; ++i) {
        if ((hp = ::gethostbyname(serverHostName.c_str())) == nullptr) {
            /* Needs more work for error message */
            std::ostringstream ostr;
            ostr << "gethostbyname() failed " << "errno:" << errno << " (" << strerror(errno) << ")";
            std::cerr << ">> SockClient.cc ERROR : " << ostr.str() << '\n';
        }
        if (hp != nullptr && hp->h_addr_list[0] != 0x0) break;
        if (i == retryMax) {
            return false;
        }
    }

    //
    // setup socket structure
    //
    in->sin_family = static_cast<sa_family_t>(hp->h_addrtype);
    memcpy(static_cast<void *>(&(in->sin_addr.s_addr)),
           static_cast<const void *>(hp->h_addr_list[0]), hp->h_length);
    in->sin_port = htons(static_cast<uint16_t>(serverPortNumber));

    return true;
}

} // namespace

namespace mcrt_dataio {

bool
SockClient::open(const std::string &hostName,
                 const int port,
                 const std::string &unixDomainSockPath)
{
    signal(SIGPIPE, SIG_IGN);   // ignore SIGPIPE (Connection reset by peer)

    mHostName = hostName;
    mPort = port;
    mUnixDomainSockPath = unixDomainSockPath;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    static const int retryMax = 10;
    static constexpr std::chrono::milliseconds retryInterval(500);

    //
    // retry openMain() loop
    //
    for (int i = 0; i < retryMax - 2; ++i) {
        if (openSockMain(false)) return true;
        if (i == 0) {
            std::ostringstream ostr;
            ostr << "retry open socket. host:>" << mHostName << "< port:" << std::to_string(mPort);
            std::cerr << ostr.str();
        }
        std::cerr << ".";
        std::this_thread::sleep_for(retryInterval);
    }

    if (!openSockMain(true)) {
        // Needs more work for error message
        std::ostringstream ostr;
        ostr << "ERROR : Could not open server connection. serverHost:" << mHostName
             << " port:" << std::to_string(mPort);
        std::cerr << ostr.str() << '\n';
        return false;
    }

    return true;
}

bool
SockClient::openSockMain(const bool errorDisplayST)
{
    if (mHostName == "localhost") {
        return openUnixSockMain(errorDisplayST);
    } else {
        return openInetSockMain(errorDisplayST);
    }
}

bool
SockClient::openInetSockMain(const bool errorDisplayST)
{
    struct sockaddr_in in;

    if (!getHostByName(mHostName, mPort, &in)) {
        return false;
    }

    //
    // get an internet domain socket
    //
    int sock;
    if ((sock = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        if (errorDisplayST) {
            std::cerr << ">> SockClient.cc ERROR : socket() failed\n";
            return false;
        }
    }

    //
    // set socket option
    //
    int optV = 1;               // true
    if (::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&optV, sizeof(int)) < 0) {
        if (errorDisplayST) {
            std::cerr << ">> SockClient.cc ERROR : SocketClient.cc setsockopt() failed. TCP_NODELAY\n";
        }
        return false;
    }

    //
    // send/recv internal buffer size setup
    // We can not set more than /proc/sys/net/core/rmem_max value
    // Default value is set at /proc/sys/net/core/rmem_default
    // I'll try to set 32MByte but probably this value is more than rmem_max
    //
    if (!setSockBufferSize(sock, SOL_SOCKET, static_cast<int>(32_MiB))) {
        if (errorDisplayST) {
            std::cerr << ">> SockClient.cc ERROR : setSockBufferSize() for Internet-domain sock failed.\n";
        }
        return false;
    }

    /* debug info dump
    {
        socklen_t len;
        int cOptV;
        if (getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&cOptV, &len) < 0) {
            std::cerr << ">> SockClient.cc ERROR : getsockopt() failed\n";
            return false;
        }
        std::cerr << ">> SockClient.cc >> TCP_NODELAY:" + std::to_string(cOptV);

        int cSendBuffSize, cRecvBuffSize;
        if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&cSendBuffSize, &len) < 0 ||
            getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&cRecvBuffSize, &len) < 0) {
            std::cerr << ">> SockClient.cc ERROR : getsockopt() failed\n";
            return false;
        }
        std::cerr << ">> SockClient.cc >> sendBuffsize:" << std::to_string(cSendBuffSize)
                  << " recvBuffSize:" << std::to_string(cRecvBuffSize) << '\n';
    }
    */

    //
    // connect to port on host
    //
    if (::connect(sock, (struct sockaddr *)&in, sizeof(in)) == -1) {
        if (errorDisplayST) {
            std::cerr << ">> SockClient.cc ERROR : connect() failed\n";
        }
        close();
        return false;
    }

    mCore.setSock(sock);

    return true;
}

bool
SockClient::openUnixSockMain(const bool errorDisplayST)
{
    //
    // get an unix domain socket
    //
    int sock;
    if ((sock = ::socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        if (errorDisplayST) {
            std::cerr << ">> SockClient.cc ERROR : openUnixSockMain() socket() failed.\n";
            return false;
        }
    }

    //
    // send/recv internal buffer size setup
    // We can not set more than /proc/sys/net/core/rmem_max value
    // Default value is set at /proc/sys/net/core/rmem_default
    // I'll try to set 32MByte but probably this value is more than rmem_max
    //
    if (!setSockBufferSize(sock, SOL_SOCKET, static_cast<int>(32_MiB))) {
        if (errorDisplayST) {
            std::cerr << ">> SockClient.cc ERROR : setSockBufferSize() for Unix-domain sock failed.\n";
        }
        return false;
    }

    /* debug info dump
    {
        socklen_t len;
        int cSendBuffSize, cRecvBuffSize;
        if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&cSendBuffSize, &len) < 0 ||
            getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&cRecvBuffSize, &len) < 0) {
            std::cerr << ">> SockClient.cc ERROR : getsockopt() failed\n";
            return false;
        }
        std::cerr << ">> SockClient.cc sendBuffsize:" << std::to_string(cSendBuffSize)
                  << " recvBuffSize:" << std::to_string(cRecvBuffSize) << '\n';
    }
    */

    std::ostringstream ostr;
    if (mUnixDomainSockPath == "") {
        // We need to think about unique name. This is temporary solution.
        ostr << "/tmp/SockClient.localhost." << mPort; // unix-domain socket name is created here.
    } else {
        ostr << mUnixDomainSockPath << '.' << mPort; // unix-domain socket name is created here.
    }

    struct sockaddr_un un;
    memset((void *)&un, 0x0, sizeof(un));
    un.sun_family = AF_UNIX;
    strncat(un.sun_path, ostr.str().c_str(), sizeof(un.sun_path) - 1);
    size_t len = sizeof(un.sun_family) + strlen(un.sun_path);

    if (un.sun_path[0] == '@') {
        un.sun_path[0] = 0x0;   // set abstruct namespace mode
    }
    std::cerr << ">> SockClient.cc Unix-domain socket path:" << ostr.str() << '\n';

    //
    // connect to port on host
    //
    if (::connect(sock, (struct sockaddr *)&un, static_cast<socklen_t>(len)) == -1) {
        if (errorDisplayST) {
            std::cerr << ">> SockClient.cc ERROR : connect() failed\n";
        }
        close();
        return false;
    }

    mCore.setSock(sock);

    return true;
}

} // namespace mcrt_dataio

