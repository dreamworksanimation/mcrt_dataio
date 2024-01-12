// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include <iostream>

#include <mcrt_dataio/share/sock/SockClient.h>
#include <mcrt_dataio/share/sock/SockServer.h>

//
// This small program is designed for testing socket related code of mcrt_dataio
// Please check the usage message for how to use sockTest.
//

namespace mcrt_dataio {

void
clientMain(mcrt_dataio::SockClient &clt)
{
    int dataSend = 123;
    if (!clt.send(&dataSend, sizeof(dataSend))) {
        std::cerr << "clientInet send failed\n";
        exit(0);
    }

    int dataRecv;
    int flag = clt.recv(&dataRecv, sizeof(int));
    if (flag != sizeof(int)) {
        std::cerr << "clientInet recv failed. flag:" << flag << '\n';
        exit(0);
    }

    std::cerr << "recv data:" << dataRecv << ' ';
    if (dataSend * 2 == dataRecv) {
        std::cerr << "OK\n";
    } else {
        std::cerr << "NG\n";
    }
}

void
clientInet(const std::string &svrHost, int svrPort)
{
    std::cerr << "client ... svrHost:" << svrHost << " svrPort:" << svrPort << '\n';

    mcrt_dataio::SockClient clt;
    if (!clt.open(svrHost, svrPort, "")) {
        std::cerr << "clt.open() failed\n";
        return;
    }
    clientMain(clt);
}

void
clientUnix(const std::string &svrPath, int svrPort)
{
    std::cerr << "client ... path:" << svrPath << '\n';

    mcrt_dataio::SockClient clt;
    if (!clt.open("localhost", svrPort, svrPath)) {
        std::cerr << "clt.open() failed\n";
        return;
    }
    clientMain(clt);
}

void
server(int port, const std::string &path)
{
    std::cerr << "server ... port:" << port << " path:" << path << '\n';

    bool shutdownFlag;
    mcrt_dataio::SockServer svr(&shutdownFlag);

    if (!svr.mainLoop(port, path,
                      [](mcrt_dataio::SockServer::ConnectionShPtr sockSvrConnection) {
                          int i;
                          int flag = sockSvrConnection->recv(&i, sizeof(i));
                          if (flag != sizeof(i)) {
                              std::cerr << "server : recv failed. flag:" << flag << '\n';
                          }
                          std::cerr << "recv:" << i << '\n';

                          i *= 2;
                          if (!sockSvrConnection->send(&i, sizeof(int))) {
                              std::cerr << "server : send failed.\n";
                          }
                      })) {
        std::cerr << "svr.mainLoop() failed.\n";
    }
}

} // namespace mcrt_dataio

int
main(int ac, char **av)
{
    auto isOption = [](const std::string optionStr, int currArgI, int countArg, int ac, char **av) -> bool {
        if (optionStr != av[currArgI]) return false;
        if (currArgI + countArg <= ac - 1) return true;
        std::cerr << "option argument count error of " << av[currArgI] << '\n';
        exit(0);
    };
    auto usage = [](char **av) {
        std::cout
        << "Usage : " << av[0] << " [options]" << '\n'
        << "[options]\n"
        << " -clti serverHost serverPort\n"
        << " -cltu serverPath serverPort\n"
        << " -svr port path\n"
        << "---------------------------------------------------------------------------------------------\n"
        << "Example of command line options for " << av[0] << "\n"
        << "Shell1 : server process shell on hostA and port is 20000\n"
        << "  " << av[0] << " -svr 20000 /tmp/tmp.abc\n"
        << "Shell2a : INTERNET-domain test : client process shell on different of hostA or on hostA\n"
        << "  " << av[0] << " -clti hostA 20000\n"
        << "  not use \"localhost\" for serverHost name because localhost configuration uses UNIX-domain IPC.\n"
        << "  UNIX-domain IPC test is done by -cltu option instead of -clti.\n"
        << "Shell2b : UNIX-domain test : client process shell on hostA\n"
        << "  " << av[0] << " -cltu /tmp/tmp.abc 20000\n"
        << "  use the same UNIX-domain serverPath. If you set a relative path for server sockTest, you \n"
        << "  should run Shell2b test in the same directory of Shell1.\n";
        exit(1);
    };

    for (int i = 1; i < ac; ++i) {
        if (isOption("-", i, 0, ac, av)) {
            usage(av);

        } else if (isOption("-clti", i, 2, ac, av)) {
            mcrt_dataio::clientInet(av[i+1], atoi(av[i+2]));
            break;

        } else if (isOption("-cltu", i, 1, ac, av)) {
            mcrt_dataio::clientUnix(av[i+1], atoi(av[i+2]));
            break;

        } else if (isOption("-svr", i, 2, ac, av)) {
            mcrt_dataio::server(atoi(av[i+1]), av[i+2]);
            break;

        } else {
            std::cerr << "unknown option :" << av[i] << '\n';
        }
    }

    return 1;
}

