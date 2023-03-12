// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include <mcrt_dataio/share/codec/InfoRec.h>

#include <iostream>
#include <sstream>

//
// This small application is used in order to get information from iRec files which are dumped by
// ClientReceiverFb of client application. iRec data includes lots of statistical information about
// one arras session and supported all configuration of back-end engines (i.e. localonly to multi mcrt).
// iRec data structure is pretty simple. It is an array of statistical data at some particular
// moment (=item). Single item includes following information for computations (i.e. mcrt and merge)
// and client.
//
//   time : timestamp which this item is recorded at client application
//   cpu : cpu load
//   mem : memory usage
//   snp : time (millisec) spend between snapshot to send at mcrt computation node
//   snd : bandwidth regarding send (Mbyte/sec)
//   prg : rendering progress %
//   rcv : merge computation receive data bandwidth (Mbyte/sec)
//   rnd : rendering_active condition at mcrt computation (bool true/false)
//   rps : renderPrep status at mcrt computation
//   ltc : latency from snapshot at back-end engine to display at client (sec)
//   clk : internal clock delta offset on each hosts (only support multi machine configuration at this moment)
//
// Example
// Global information is separately stored. Global info includes all back-end node information.
// From this info, you can understand session¡Çs machine environment
// > infoRecDump <iRecFile> -showGlobal
//
// Get how many items are recorded.
// > infoRecDump <iRecFile> -showItemTotal
//
// You can get particular item's info
// > infoRecDump <iRecFile> -showItem 12 cpu # cpu info at itemId = 12
// > infoRecDump <iRecFile> -showItem 12 all # all info at itemId = 12
//
// You can get all items info
// > infoRecDump <iRecFile> -show snp # snapshot duration info for all items.
//

namespace mcrt_dataio {

bool
isHelp(int ac, char **av)
{
    auto isHelpOption = [](char *opt) -> bool {
        std::string optStr(opt);
        return (optStr == "-" ||
                optStr == "-h" || optStr == "-H" || optStr == "--h" || optStr == "--H" ||
                optStr == "-help" || optStr == "-HELP" || optStr == "--help" || optStr == "--HELP"); 
    };
    auto includeHelpOption = [&](int ac, char **av) -> bool {
        for (int i = 1; i < ac; ++i) {
            if (isHelpOption(av[i])) return true;
        }
        return false;
    };

    if (ac < 2 || includeHelpOption(ac, av)) return true;
    return false;
}

std::string
usage(char *progName)
{
    std::ostringstream ostr;
    ostr << "Usage : " << progName << " iRecFile [options]\n"
         << "[Options]\n"
         << "  -show <mode> : show info about all timeStamps\n"
         << "    <mode> : all : all info\n"
         << "             cpu : CPU usage\n"
         << "             mem : display Mem usage\n"
         << "             snp : display snapshot to send time (millisec)\n"
         << "             snd : display send bandwidth (Mbyte/Sec)\n"
         << "             prg : display progress (%)\n"
         << "             rcv : display merger receive bandwidth (Mbyte/Sec)\n"
         << "             rnd : display render active condition (bool)\n"
         << "             rps : display render prep stats (enum)\n"
         << "             ltc : display client latency (sec)\n"
         << "             clk : display clockDelta offset (millisec)\n"
         << "  -showGlobal : show infoGlobal only\n"
         << "  -showItemTotal : show itemTotal number\n"
         << "  -showItem n <mode> : show n-th item\n"
         << "    <mode> : all : all info for recItem\n"
         << "            time : display time\n"
         << "             cpu : display CPU usage\n"
         << "             mem : display Mem usage\n"
         << "             snp : display snapshot to send time (millisec)\n"
         << "             snd : display send bandwidth (Mbyte/Sec)\n"
         << "             prg : display progress (%)\n"
         << "             rcv : display merger receive bandwidth (Mbyte/Sec)\n"
         << "             rnd : display render active condition (bool)\n"
         << "             rps : display render prep stats (enum)\n"
         << "             ltc : display client latency (sec)\n"
         << "             clk : display clockDelta offset (millisec)\n";
    return ostr.str();
}

} // namespace mcrt_dataio

int
main(int ac, char **av)
{
    auto argCountCheck = [&](int i, int ac, int count) -> bool {
        if (i + count >= ac) {
            std::cerr << "ERROR : option argument count error" << std::endl;
            return false;
        }
        return true;
    };

    if (mcrt_dataio::isHelp(ac, av)) {
        std::cout << mcrt_dataio::usage(av[0]) << std::endl;
        return 0;
    }

    mcrt_dataio::InfoRecMaster recMaster;

    std::string fileName;
    int argId = 0;
    for (int i = 1; i < ac; ++i) {
        std::string opt = av[i];
        if (opt == "-show") {
            if (!argCountCheck(i, ac, 1)) return 1;
            std::string mode = av[++i];
            if (mode == "all") {
                std::cout << recMaster.show() << std::endl;
            } else if (mode == "cpu" || mode == "mem" || mode == "snp" || mode == "snd" ||
                       mode == "prg" || mode == "rcv" || mode == "rnd" || mode == "rps" ||
                       mode == "ltc" || mode == "clk") {
                std::cout << recMaster.showTable(mode) << std::endl;
            }

        } else if (opt == "-showGlobal") {
            std::cout << recMaster.getGlobal().show() << std::endl;
        } else if (opt == "-showItemTotal") {
            std::cout << recMaster.getItemTotal() << std::endl;
        } else if (opt == "-showItem") {
            if (!argCountCheck(i, ac, 2)) return 1;
            size_t id = atoi(av[++i]);
            if (id >= recMaster.getItemTotal()) {
                std::cerr << "id out of range" << std::endl;
                return 1;
            }
            std::string mode = av[++i];
            if (mode == "all") {
                std::cout << recMaster.getRecItem(id)->show() << std::endl;
            } else if (mode == "time") {
                std::cout << recMaster.getRecItem(id)->getTimeStampStr() << std::endl;
            } else if (mode == "cpu" || mode == "mem" || mode == "snp" || mode == "snd" ||
                       mode == "prg" || mode == "rcv" || mode == "rnd" || mode == "rps" ||
                       mode == "ltc" || mode == "clk") {
                std::cout << recMaster.getRecItem(id)->showTable(mode) << std::endl;
            }

        } else {
            switch (argId) {
            case 0 :
                fileName = opt;
                std::cout << "fileName:" << fileName << std::endl;
                if (!recMaster.load(fileName)) {
                    std::cerr << "load failed filename:" << fileName << std::endl;
                    return 0;
                }
                break;
            default :
                std::cerr << "ERROR : unknown option:" << opt << std::endl;
            }
            ++argId;
        }
    }

    return 0;
}

