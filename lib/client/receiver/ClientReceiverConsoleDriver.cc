// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ClientReceiverConsoleDriver.h"

#include <mcrt_messages/GenericMessage.h>
#include <mcrt_messages/RenderMessages.h>
#include <mcrt_messages/ViewportMessage.h>

namespace mcrt_dataio {

void
ClientReceiverConsoleDriver::parserConfigure(Parser &parser)
{
    parser.description("debugConsole top level command");
    parser.opt("genericMsg", "...command...", "send generic message",
               [&](Arg &arg) -> bool {
                   return sendMessage([&]() -> const MessageContentConstPtr {
                           mcrt::GenericMessage::Ptr gMsg = std::make_shared<mcrt::GenericMessage>();
                           gMsg->mValue = arg.currArgCmdLine();
                           arg.shiftArgAll();
                           return gMsg;
                       });
               });
    parser.opt("start", "", "start render",
               [&](Arg &arg) -> bool {
                   return sendMessage(mcrt::RenderMessages::createControlMessage(false));
               });
    parser.opt("stop", "", "stop render",
               [&](Arg &) -> bool {
                   return sendMessage(mcrt::RenderMessages::createControlMessage(true));
               });
    parser.opt("aov", "...command...", "AOV related command",
               [&](Arg &arg) -> bool { return mParserAov.main(arg.childArg()); });
    parser.opt("dispatch", "...command...", "dispatch computation related command",
               [&](Arg& arg) -> bool { return mParserDispatch.main(arg.childArg()); });
    parser.opt("mcrt", "...command...", "mcrt computation related command",
               [&](Arg &arg) -> bool { return mParserMcrt.main(arg.childArg()); });
    parser.opt("merge", "...command...", "merge computation related command",
               [&](Arg& arg) -> bool { return mParserMerge.main(arg.childArg()); });
    parser.opt("pick", "...command...", "pixel picker command",
               [&](Arg &arg) -> bool { return mParserPick.main(arg.childArg()); });
    parser.opt("invalidate", "...command...", "invalidate resources",
               [&](Arg &arg) -> bool { return mParserInvalidate.main(arg.childArg()); });
    parser.opt("clientReceiver", "...command...", "clientReceiver command",
               [&](Arg &arg) -> bool {
                   if (!mFbReceiver) return false;
                   return mFbReceiver->getParser().main(arg.childArg());
               });
    parser.opt("feedback", "<on|off|show>", "enable/disable image feedback logic",
               [&](Arg& arg) -> bool { return cmdFeedback(arg); });
    parser.opt("feedbackInterval", "<intervalSec|show>", "set feedback interval by sec",
               [&](Arg& arg) -> bool { return cmdFeedbackInterval(arg); });

    //------------------------------

    mParserAov.description("AOV related command");
    mParserAov.opt("ls", "", "list all AOV name",
                   [&](Arg &arg) -> bool { return cmdAovLs(arg); });
    mParserAov.opt("pix", "<x> <y> <AOVname>", "show pixel value",
                   [&](Arg &arg) -> bool { return cmdAovPix(arg); });

    //------------------------------

    mParserInvalidate.description("invalidate texture command");
    mParserInvalidate.opt("tex", "...", "invalidate textures (set list of texture name)",
                          [&](Arg &arg) -> bool {
                              return sendMessage([&]() -> const MessageContentConstPtr {
                                      std::vector<std::string> rsc = arg.currArg();
                                      arg.shiftArgAll();
                                      return mcrt::RenderMessages::createInvalidateResourcesMessage(rsc);
                                  });
                          });
    mParserInvalidate.opt("all", "", "invalidate all textures",
                          [&](Arg &arg) -> bool {
                              return sendMessage([&]() -> const MessageContentConstPtr {
                                      std::vector<std::string> rsc = {"*"};
                                      return mcrt::RenderMessages::createInvalidateResourcesMessage(rsc);
                                  });
                          });

    //------------------------------

    mParserDispatch.description("dispatch computation command");
    mParserDispatch.opt("cmd", "...command...", "dispatch debug command",
                        [&](Arg &arg) -> bool {
                            return sendMessage([&]() -> const MessageContentConstPtr {
                                    mcrt::GenericMessage::Ptr gMsg = std::make_shared<mcrt::GenericMessage>();
                                    std::ostringstream ostr;
                                    ostr << "cmd -3 " << arg.childArg().currArgCmdLine();
                                    gMsg->mValue = ostr.str();
                                    return gMsg;
                                });
                        });

    //------------------------------

    mParserMcrt.description("mcrt computation command");
    mParserMcrt.opt("rank", "<id>", "set destination rankId (start from 0)",
                    [&](Arg &arg) -> bool {
                        mParserMcrtRankId = (arg++).as<int>(0);
                        return true;
                    });
    mParserMcrt.opt("rankAll", "", "set destination as all rank",
                    [&](Arg &) -> bool {
                        mParserMcrtRankId = -1;
                        return true;
                    });
    mParserMcrt.opt("cmd", "...command...", "mcrt debug command",
                    [&](Arg &arg) -> bool {
                        return sendMessage([&]() -> const MessageContentConstPtr {
                                mcrt::GenericMessage::Ptr gMsg = std::make_shared<mcrt::GenericMessage>();
                                std::ostringstream ostr;
                                ostr << "cmd " << mParserMcrtRankId << " " << arg.childArg().currArgCmdLine();
                                gMsg->mValue = ostr.str();
                                return gMsg;
                            });
                    });
    mParserMcrt.opt("show", "", "show mcrt send rankInfo",
                    [&](Arg& arg) -> bool { return arg.msg(showRankInfo() + '\n'); });

    //------------------------------

    mParserMerge.description("merge computation command");
    mParserMerge.opt("cmd", "...command...", "merge debug command",
                     [&](Arg &arg) -> bool {
                         return sendMessage([&]() -> const MessageContentConstPtr {
                                 mcrt::GenericMessage::Ptr gMsg = std::make_shared<mcrt::GenericMessage>();
                                 std::ostringstream ostr;
                                 ostr << "cmd -2 " << arg.childArg().currArgCmdLine();
                                 gMsg->mValue = ostr.str();
                                 return gMsg;
                             });
                     });

    //------------------------------

    mParserPick.description("pixel picker command");
    mParserPick.opt("0", "<sx> <sy>", "material",
                    [&](Arg &arg) -> bool { return cmdPick(arg, 0); });
    mParserPick.opt("1", "<sx> <sy>", "light contributions",
                   [&](Arg &arg) -> bool { return cmdPick(arg, 1); });
    mParserPick.opt("2", "<sx> <sy>", "geometry",
                    [&](Arg &arg) -> bool { return cmdPick(arg, 2); });
    mParserPick.opt("3", "<sx> <sy>", "geometry part",
                    [&](Arg &arg) -> bool { return cmdPick(arg, 3); });
    mParserPick.opt("4", "<sx> <sy>", "position and normal (not supported yet)",
                    [&](Arg &arg) -> bool { return cmdPick(arg, 4); });
    mParserPick.opt("5", "<sx> <sy>", "cell inspector (not supported yet)",
                    [&](Arg &arg) -> bool { return cmdPick(arg, 5); });
}

bool
ClientReceiverConsoleDriver::cmdAovLs(Arg &arg)
{
    if (!mFbReceiver) return arg.msg("fbReceiver is empty\n");
    if (mFbReceiver->getProgress() < 0.0f) {
        return arg.msg("image has not been received yet\n");
    }

    std::ostringstream ostr;
    ostr << "aov name {\n"
         << "  *Beauty\n"
         << "  *PixelInfo\n"
         << "  *HeatMap\n"
         << "  *Weight\n"
         << "  *BeautyOdd\n";
    for (unsigned i = 0; i < mFbReceiver->getTotalRenderOutput(); ++i) {
        ostr << "  " << mFbReceiver->getRenderOutputName(i) << '\n';
    }
    ostr << "}";
    return arg.msg(ostr.str() + '\n');
}

bool
ClientReceiverConsoleDriver::cmdAovPix(Arg &arg)
{
    int sx = arg.as<int>(0);
    int sy = arg.as<int>(1);
    const std::string &aovName = arg(2);
    arg += 3;
    
    if (mFbReceiver) {
        return arg.msg(mFbReceiver->showPix(sx, sy, aovName) + '\n');
    }
    return arg.msg("fbReceiver is empty\n");
}

bool
ClientReceiverConsoleDriver::cmdPick(Arg &arg, int mode) const
{
    int sx = arg.as<int>(0);
    int sy = arg.as<int>(1);
    arg += 2;

    return sendMessage([&]() -> const MessageContentConstPtr {
            mcrt::JSONMessage::Ptr jMsg =
                mcrt::JSONMessage::create(mcrt::RenderMessages::PICK_MESSAGE_ID,
                                          mcrt::RenderMessages::PICK_MESSAGE_NAME);
            jMsg->messagePayload()[mcrt::RenderMessages::PICK_MESSAGE_PAYLOAD_PIXEL][Json::Value::ArrayIndex(0)] = sx;
            jMsg->messagePayload()[mcrt::RenderMessages::PICK_MESSAGE_PAYLOAD_PIXEL][Json::Value::ArrayIndex(1)] = sy;
            jMsg->messagePayload()[mcrt::RenderMessages::PICK_MESSAGE_PAYLOAD_MODE] = mode;
            return jMsg;
        });
}

bool
ClientReceiverConsoleDriver::cmdFeedback(Arg& arg)
{
    sendCommandToAllMcrtAndMerge("feedback " + (arg++)());
    return true;
}

bool
ClientReceiverConsoleDriver::cmdFeedbackInterval(Arg& arg)
{
    sendCommandToAllMcrtAndMerge("feedbackInterval " + (arg++)());
    return true;
}

void
ClientReceiverConsoleDriver::sendCommandToAllMcrtAndMerge(const std::string& command)
{
    auto cmdGen = [&](const std::string& key) {
        mcrt::GenericMessage::Ptr gMsg = std::make_shared<mcrt::GenericMessage>();
        gMsg->mValue = std::string("cmd ") + key + ' ' + command;
        return gMsg;
    };

    constexpr const char* DESTINATION_ALL_MCRT = "-1";
    constexpr const char* DESTINATION_MERGE = "-2";

    sendMessage([&]() -> const MessageContentConstPtr { return cmdGen(DESTINATION_ALL_MCRT); });
    sendMessage([&]() -> const MessageContentConstPtr { return cmdGen(DESTINATION_MERGE); });
}

std::string
ClientReceiverConsoleDriver::showRankInfo() const
{
    std::ostringstream ostr;
    ostr << "send mcrt rankInfo {\n"
         << "  mParserMcrtRankId:" << mParserMcrtRankId << " (-1 = allRank)\n"
         << "}";
    return ostr.str();
}

} // namespace mcrt_dataio
