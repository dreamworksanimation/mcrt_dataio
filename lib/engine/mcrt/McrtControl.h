// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file McrtControl.h
///
/// @breaf -- MCRT control command parse/run utility --
///
/// This file includes McrtControl class and it is used by computations to analyze McrtControl command
/// and execute command with proper arguments by callback.
///
#pragma once

#include <cstdint>
#include <functional>
#include <iostream>

namespace mcrt_dataio {

class McrtControl
{
public:
    explicit McrtControl(int machineId) :
        mMachineId(machineId)
    {}

    /// @brief Create "ClockDeltaClient" command string for McrtControl-command
    /// @param nodeId Specify nodeId (= machineId) which is used to identify client process
    /// @param serverName Specify the server hostname which is running clockDelta server code
    /// @param port Specify the server port number of the server host for clockDelta communication
    /// @param path Specify the Unix-domain IPC pass of the server host for clockDelta communication
    /// @return Return message string that is used for "ClockDeltaClient" McrtControl-command
    ///
    /// @detail
    /// This API is used to create a message string for "ClockDeltaClient" McrtControl-command.
    static std::string msgGen_clockDeltaClient(const int nodeId,
                                               const std::string &serverName,
                                               const int port,
                                               const std::string &path);

    /// @brief Create "ClockOffset" command string for McrtControl-command
    /// @param hostname Specify hostname which receives clockOffset value
    /// @param offsetMs Specify internal clock offset delta timing by milliseconds.
    /// @return Return message string that is used for "ClockOffset" McrtControl-command
    ///
    /// @detail
    /// This API is used to create a message string for "ClockOffset" McrtControl-command.
    static std::string msgGen_clockOffset(const std::string &hostName,
                                          const float offsetMs);

    /// @brief Create "RenderComplete" command string for McrtControl-command
    /// @param syncId Intended syncId value condition to execute command
    /// @return Return message string that is used for "RenderComplete" McrtControl-command
    ///
    /// @detail
    /// This API is used to create a message string for "RenderComplete" McrtControl-command.
    static std::string msgGen_completed(const uint32_t syncId);

    /// @brief Create "GlobalProgress" command string for McrtControl-command
    /// @param syncId Intended syncId value condition to execute command
    /// @param progressFraction updated global progress fraction 
    /// @return Return message string that is used for "GlobalProgress" McrtControl-command.
    ///
    /// @detail
    /// This API is used to create a message string for "GlobalProgress" McrtControl-command.
    static std::string msgGen_globalProgress(const uint32_t syncId,
                                             const float progressFraction);

    /// @brief Create "ForceRenderStart" command string for McrtControl-command
    /// @return Return message string that is used for "ForceRenderStart" McrtControl-command.
    ///
    /// @detail
    /// This API is used to create a message string for "ForceRenderStart" McrtControl-command.
    static std::string msgGen_forceRenderStart();

    //------------------------------

    /// @brief Check given msgStr is McrtControl command line or not.
    ///
    /// @param cmdLine command line to check
    /// @return Return true if msgStr is McrtControl command line or false if it is not.
    ///
    /// @detail
    /// You can call this function in order to make sure given msgStr is McrtControl command line
    /// or not. So far, we only have a few McrtControl commands and have tested with few commands.
    /// But in the future, this API checks many candidates of McrtCommands and returns true if it
    /// finds one of them.
    /// Return false if given command line is not McrtControl command.
    /// This API only do test and any further operation for command line itself.
    ///
    static bool isCommand(const std::string &cmdLine);

    /// @brief Execute McrtControl command itself
    ///
    /// @param cmdLine command line for execution
    /// @param callBackRenderCompleteProcedure call-back function if command is "RenderCompleted".
    /// @param callBackGlobalProgressUpdate call-back function if command is "GlobalProgress".
    /// @param callBackForceRenderStart call-back function if command is "ForceRenderStart".
    /// @Return Return callBack result status or false is it's not a McrtCommand or error happened.
    ///
    /// @detail
    /// So far we only have 3 McrtControl commands. This is why we only have 3 call-back arguments.
    /// We will have more than 3 call-back function definitions when we add more McrtCommands in the future.
    /// If given msgStr is not McrtControl command or command format is wrong, this API returns false.
    /// Otherwise, this API executes the callback and returns its result.
    ///
    /// Most CallBack function's 1st argument is syncId which you should compare with computation's
    /// current syncId. McrtControl commands always has syncId and intended only executed computation's
    /// syncId is equal as McrtControl command's syncId.
    ///
    /// CallBackForceRenderStart for "ForceRenderStart" MCRT-control message does not have a syncId
    /// argument because "ForceRenderStart" is a special command that is not related to the syncId. 
    /// Usually, backend computation processes all the received messages at once. Then the backend
    /// computation starts rendering if it finds no more received messages. This means, rendering
    /// does not start if there are lots of incoming messages.
    /// This behavior is reasonable for most of the arras/moonray context.
    /// However, sometimes this is no good if you send multiple continuous camera positions once
    /// and want to get each camera position's rendered image at least one frame; unfortunately,
    /// you only get the last camera position's image because all the continuous camera position
    /// messages are processed at once, and the last camera position is finally valid.
    /// If you put in the "ForceRenderStart" during multiple continuous messages, backend computation
    /// stops processing messages at "ForceRenderStart" messages, and starts rendering, then suspends
    /// message processing until at least a single snapshot is sent to the downstream. After the
    /// snapshot happens and sends data to the downstream, the backend engine stops the current
    /// rendering and resumes processing the already received queue. 
    /// You can include multiple "ForceRenderStart" messages into the sequence of multiple other
    /// messages as well. "ForceRenderStart" message guarantees the start of rendering in terms
    /// of all the messages just before "ForceRenderStart" and sends at least a single frame to the
    /// downstream. After that, the suspending received queue is resumed processing.
    /// This is the only solution if you don't want to process all the received messages at once.
    ///
    bool run(const std::string& cmdLine,
             const std::function<bool(uint32_t /*syncId*/)>& callBackRenderCompleteProcedure,
             const std::function<void(uint32_t /*syncId*/, float /*fraction*/)>& callBackGlobalProgressUpdate,
             const std::function<void()>& callBackForceRenderStart);

protected:    
    int mMachineId;
};

} // namespace mcrt_dataio
