// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "McrtControl.h"

#include <mcrt_dataio/share/util/ClockDelta.h>
#include <mcrt_dataio/share/util/MiscUtil.h>

#include <scene_rdl2/common/grid_util/LatencyLog.h>

#include <sstream>
#include <string>
#include <vector>

//#define DEBUG_MESSAGE

namespace {

static constexpr char MCRT_CONTROL_COMMAND[]  = "MCRT-control";

//
// McrtControl command format definition
//
static constexpr char CMD_CLOCKDELTACLIENT[] = "clockDeltaClient <nodeId> <serverName> <port> <path>";
static constexpr char CMD_CLOCKOFFSET[]      = "clockOffset <hostName> <offsetMs>";
static constexpr char CMD_COMPLETED[]        = "completed <syncId>";
static constexpr char CMD_GLOBALPROGRESS[]   = "globalProgress <syncId> <fraction>";

using TokenArray = std::vector<std::string>;
using callBackEvalCmd = std::function<void(const TokenArray &tokenArray)>;

std::string
getCmdName(const std::string& cmdDef)
{
    std::istringstream istr(cmdDef);
    std::string cmdName;
    istr >> cmdName;
    return cmdName;
}

int    
getCmdArgCount(const std::string& cmdDef)
{
    std::istringstream istr(cmdDef);
    std::string token;
    int argCount = 0;
    while (istr >> token) ++argCount;
    return argCount - 1; // subtract 1 due to we should not include cmdName part.
}

bool
isCmd(const std::string& cmdLine,
      const callBackEvalCmd& callBack_clockDeltaClient = nullptr,
      const callBackEvalCmd& callBack_clockOffset = nullptr,
      const callBackEvalCmd& callBack_completed = nullptr,
      const callBackEvalCmd& callBack_globalProgress = nullptr)
{
    auto convCmdLineToTokenArray = [&]() -> std::vector<std::string> {
        std::vector<std::string> tokenArray;
        std::istringstream istr(cmdLine);
        std::string token;
        while (istr >> token) {
            tokenArray.push_back(token);
        }
        return tokenArray;
    };
    auto isMcrtControlCommand = [&](const std::string& cmdDef,
                                    const std::vector<std::string>& tokenArray) -> bool {
        auto cmdParse = [](const std::string& cmdName,
                           const std::vector<std::string>& tokenArray,
                           int numArg) -> bool {
            if (tokenArray.size() < 2) return false;
            if (tokenArray[0] != MCRT_CONTROL_COMMAND) return false; // This is not a MCRT-control command
            if (tokenArray[1] != cmdName) return false;
            if (static_cast<int>(tokenArray.size()) != numArg + 2) return false;
            return true;
        };
        return cmdParse(getCmdName(cmdDef), tokenArray, getCmdArgCount(cmdDef));
    };

    std::vector<std::string> tokenArray = convCmdLineToTokenArray();
    if (isMcrtControlCommand(CMD_CLOCKDELTACLIENT, tokenArray)) {
        if (callBack_clockDeltaClient) {
            callBack_clockDeltaClient(tokenArray);
        }
    } else if (isMcrtControlCommand(CMD_CLOCKOFFSET, tokenArray)) {
        if (callBack_clockOffset) {
            callBack_clockOffset(tokenArray);
        }
    } else if (isMcrtControlCommand(CMD_COMPLETED, tokenArray)) {
        if (callBack_completed) {
            callBack_completed(tokenArray);
        }
    } else if (isMcrtControlCommand(CMD_GLOBALPROGRESS, tokenArray)) {
        if (callBack_globalProgress) {
            callBack_globalProgress(tokenArray);
        }
    } else {
        return false;
    }
    return true;
}
    
} // namespace

//-------------------------------------------------------------------------------------------------------------

namespace mcrt_dataio {

// static function    
std::string
McrtControl::msgGen_clockDeltaClient(const int nodeId,
                                     const std::string& serverName,
                                     const int port,
                                     const std::string& path)
{
    std::ostringstream ostr;
    ostr << MCRT_CONTROL_COMMAND << ' ' << getCmdName(CMD_CLOCKDELTACLIENT)
         << ' ' << nodeId
         << ' ' << serverName
         << ' ' << port
         << ' ' << path;
    return ostr.str();
}

// static function
std::string
McrtControl::msgGen_clockOffset(const std::string& hostName,
                                const float offsetMs)
{
    std::ostringstream ostr;
    ostr << MCRT_CONTROL_COMMAND << ' ' << getCmdName(CMD_CLOCKOFFSET)
         << ' ' << hostName
         << ' ' << offsetMs;
    return ostr.str();
}

// static function
std::string
McrtControl::msgGen_completed(const uint32_t syncId)
{
    std::ostringstream ostr;
    ostr << MCRT_CONTROL_COMMAND << ' ' << getCmdName(CMD_COMPLETED)
         << ' ' << syncId;
    return ostr.str();
}

// static function
std::string
McrtControl::msgGen_globalProgress(const uint32_t syncId,
                                   const float fraction)
{
    std::ostringstream ostr;
    ostr << MCRT_CONTROL_COMMAND << ' ' << getCmdName(CMD_GLOBALPROGRESS)
         << ' ' << syncId
         << ' ' << fraction;
    return ostr.str();
}

// static function
bool
McrtControl::isCommand(const std::string& cmdLine)
{
    return isCmd(cmdLine);
}

bool
McrtControl::run(const std::string& cmdLine,
                 const std::function<bool(uint32_t syncId)>& callBackRenderStopProcedure,
                 const std::function<void(uint32_t syncId, float fraction)>& callBackGlobalProgressUpdate)
{
    bool returnFlag = true;
    isCmd(cmdLine,
          [&](const std::vector<std::string>& tokenArray) { // callBack_clockDeltaClient
              // MCRT-control clockDeltaClient <nodeId> <serverName> <port> <path>
              if (std::stoi(tokenArray[2]) != mMachineId) return;
#             ifdef DEBUG_MESSAGE
              std::cerr << ">> McrtControl.cc ===>>> run clockdeltaClient <<<==="
                        << " nodeId:" << std::stoi(tokenArray[2]) << '\n'
                        << " mergeHostName:" << tokenArray[3] << '\n'
                        << " port:" << std::stoi(tokenArray[4]) << '\n'
                        << " path:" << tokenArray[5] << '\n';
#             endif // end DEBUG_MESSAGE
              returnFlag = ClockDelta::clientMain(tokenArray[3], // mergeHostName
                                                  std::stoi(tokenArray[4]), // port
                                                  tokenArray[5], // path
                                                  ClockDelta::NodeType::MCRT);
          },

          [&](const std::vector<std::string>& tokenArray) { // callBack_clockOffset
              // MCRT-control clockOffset <hostName> <offsetMs>
              if (tokenArray[2] != mcrt_dataio::MiscUtil::getHostName()) return;
#             ifdef DEBUG_MESSAGE
              std::cerr << ">> McrtControl.cc ===>>> run clockOffset <<<==="
                        << " hostName:" << tokenArray[2]
                        << " offsetMs:" << std::stof(tokenArray[3]) << " ms\n";
#             endif // end DEBUG_MESSAGE                  
              scene_rdl2::grid_util::LatencyClockOffset::getInstance().
                  setOffsetByMs(std::stof(tokenArray[3])); // set clock offset
          },
                     
          [&](const std::vector<std::string>& tokenArray) { // callBack_completed
              // MCRT-control completed <syncId>
              uint32_t syncId = static_cast<uint32_t>(std::stoul(tokenArray[2]));
#             ifdef DEBUG_MESSAGE
              std::cerr << ">> McrtControl.cc ===>>> run completed <<<=== syncId:" << syncId << '\n';
#             endif // end DEBUG_MESSAGE
              returnFlag = callBackRenderStopProcedure(syncId);
          },

          [&](const std::vector<std::string>& tokenArray) {
              // MCRT-control globalProgress <fraction>
              uint32_t syncId = static_cast<uint32_t>(std::stoul(tokenArray[2]));
              float fraction = std::stof(tokenArray[3]);
#             ifdef DEBUG_MESSAGE
              std::cerr << ">> McrtControl.cc ===>>> run globalProgress <<<==="
                        << " syncId:" syncId
                        << " fraction:" << fraction << '\n';
#             endif // end DEBUG_MESSAGE              
              callBackGlobalProgressUpdate(syncId, fraction);
          }
          );
    return returnFlag;
}

} // namespace mcrt_dataio
