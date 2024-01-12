// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

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
//   fAc : feedback active condition (bool true/false)
//   fBp : feedback message send(=merge) or recv(=mcrt) bandwidth (MByte/sec)
//   fFp : feedback message send(=merge) or recv(=mcrt) fps
//   fEv : feedback message encode(=merge) or decode(=mcrt) time (millisec)
//   fIt : feedback interval (sec)
//   fLt : feedback latency at mcrt (millisec)
//
// Example
// Global information is separately stored. Global info includes all back-end node information.
// From this info, you can understand session's machine environment
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
idRangeCheck(const int startId, int& endId, const mcrt_dataio::InfoRecMaster& recMaster)
{
    int maxId = static_cast<int>(recMaster.getItemTotal()) - 1;
    if (startId > maxId) {
        std::cerr << "Error : startId:" << startId << " out of range. maxId:" << maxId << '\n';
        return false;
    }
    if (maxId < endId) endId = maxId;
    return true;
}

void
show(const std::string& key, const mcrt_dataio::InfoRecMaster& recMaster)
{
    if (key == "all") {
        std::cout << recMaster.show() << std::endl;
    } else if (key == "time") {
        std::cerr << "key:" << key << " is not supported yet\n";
    } else if (key == "cpu" || key == "mem" || key == "snp" || key == "snd" ||
               key == "prg" || key == "rcv" || key == "rnd" || key == "rps" ||
               key == "ltc" || key == "clk" ||
               key == "fAc" || key == "fBp" || key == "fFp" || key == "fEv" ||
               key == "fIt" || key == "fLt") {
        std::cout << recMaster.showTable(key) << std::endl;
    }
}

void
showItem(const int id, const std::string& key, const mcrt_dataio::InfoRecMaster& recMaster)
{
    if (id >= static_cast<int>(recMaster.getItemTotal())) {
        std::cerr << "ERROR : id:" << id << " is out of range\n";
        return;
    }

    if (key == "all") {
        std::cout << recMaster.getRecItem(id)->show() << std::endl;
    } else if (key == "time") {
        std::cout << recMaster.getRecItem(id)->getTimeStampStr() << std::endl;
    } else if (key == "cpu" || key == "mem" || key == "snp" || key == "snd" ||
               key == "prg" || key == "rcv" || key == "rnd" || key == "rps" ||
               key == "ltc" || key == "clk" ||
               key == "fAc" || key == "fBp" || key == "fFp" || key == "fEv" ||
               key == "fIt" || key == "fLt") {
        std::cout << recMaster.getRecItem(id)->showTable(key) << std::endl;
    }
}

void
plotDumpMcrt(const int startId, int endId, const std::string& key, const mcrt_dataio::InfoRecMaster& recMaster)
{
    if (!idRangeCheck(startId, endId, recMaster)) return;

    if (key == "cpu" || key == "mem" || key == "snp" || key == "snd" ||
        key == "clk" ||
        key == "fBp" || key == "fFp" || key == "fEv" || key == "fLt") {
        std::cout << recMaster.showMcrt(key, startId, endId) << '\n';
    } else {
        std::cerr << "key:" << key << " is not supported yet for this option\n";
    }
}

void
plotDumpMcrtAvg(const int startId, int endId, const std::string& key, const mcrt_dataio::InfoRecMaster& recMaster)
{
    if (!idRangeCheck(startId, endId, recMaster)) return;

    if (key == "cpu" || key == "mem" || key == "snp" || key == "snd" ||
        key == "clk" ||
        key == "fBp" || key == "fFp" || key == "fEv" || key == "fLt") {
        std::cout << recMaster.showMcrtAvg(key, startId, endId) << '\n';
    } else {
        std::cerr << "key:" << key << " is not supported yet for this option\n";
    }
}

void
plotDumpMerge(const int startId, int endId, const std::string& key, const mcrt_dataio::InfoRecMaster& recMaster)
{
    if (!idRangeCheck(startId, endId, recMaster)) return;

    if (key == "cpu" || key == "mem" || key == "snd" ||
        key == "png" ||
        key == "rcv" || key == "clk" ||
        key == "fBp" || key == "fFp" || key == "fEv") {
        std::cout << recMaster.showMerge(key, startId, endId) << '\n';
    } else {
        std::cerr << "key:" << key << " is not supported yet for this option\n";
    }
}

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
         << "  -show <key> : show info about all timeStamps\n"
         << "  -showGlobal : show infoGlobal only\n"
         << "  -showItemTotal : show itemTotal number\n"
         << "  -showItem n <key> : show n-th item\n"
         << "  -plotDumpMcrt startId endId <key> : dump mcrt value\n"
         << "  -plotDumpMcrtAvg startId endId <key> : dump mcrt averated value\n"
         << "  -plotDumpMerge startId endId <key> : dump merge value\n"
         << "<key>\n"
         << "   all : all info\n"
         << "  time : display time\n"
         << "   cpu : CPU usage\n"
         << "   mem : Mem usage\n"
         << "   snp : snapshot to send time (millisec)\n"
         << "   snd : send bandwidth (Mbyte/Sec)\n"
         << "   prg : progress (%)\n"
         << "   rcv : merger receive bandwidth (Mbyte/Sec)\n"
         << "   rnd : render active condition (bool)\n"
         << "   rps : render prep stats (enum)\n"
         << "   ltc : client latency (sec)\n"
         << "   clk : clockDelta offset (millisec)\n"
         << "   fAc : feedback active condition (bool)\n"
         << "   fBp : feedback message send/recv bandwidth\n"
         << "   fFp : feedback message send/recv fps\n"
         << "   fEv : feedback message encode/decode time (millisec)\n"
         << "   fIt : feedback interval (sec)\n"
         << "   fLt : feedback latency at mcrt (millisec)";
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
            show(av[i+1], recMaster);
            i++;

        } else if (opt == "-showGlobal") {
            std::cout << recMaster.getGlobal().show() << std::endl;
        } else if (opt == "-showItemTotal") {
            std::cout << recMaster.getItemTotal() << std::endl;
        } else if (opt == "-showItem") {
            if (!argCountCheck(i, ac, 2)) return 1;
            showItem(atoi(av[i+1]), av[i+2], recMaster);
            i += 2;

        } else if (opt == "-plotDumpMcrt") {
            if (!argCountCheck(i, ac, 3)) return 1;
            plotDumpMcrt(atoi(av[i+1]), atoi(av[i+2]), av[i+3], recMaster);
            i += 3;

        } else if (opt == "-plotDumpMcrtAvg") {
            if (!argCountCheck(i, ac, 3)) return 1;
            plotDumpMcrtAvg(atoi(av[i+1]), atoi(av[i+2]), av[i+3], recMaster);
            i += 3;

        } else if (opt == "-plotDumpMerge") {
            if (!argCountCheck(i, ac, 3)) return 1;
            plotDumpMerge(atoi(av[i+1]), atoi(av[i+2]), av[i+3], recMaster);
            i += 3;

        } else {
            switch (argId) {
            case 0 :
                fileName = opt;
                std::cout << "# fileName:" << fileName << std::endl;
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
