// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file McrtControl.h
///
/// @breaf -- MCRT control command parse/run utility --
///
/// This file includes McrtControl class and it is used by computations to analyze McrtControl command
/// and execute command with proper arguments by callback.
/// So far we only have one McrtControl command and it's "RenderStop". This is why we only have one
/// callBack function to call run() function. In the future, more commands might be added.
///
#pragma once

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

    //------------------------------

    /// @brief Check given msgStr is McrtControl command line or not.
    ///
    /// @param cmdLine command line to check
    /// @return Return true if msgStr is McrtControl command line or false if it is not.
    ///
    /// @detail
    /// You can call this function in order to make sure given msgStr is McrtControl command line
    /// or not. So far we only have one McrtControl and tested with one command. But in the future,
    /// this API checks multiple candidates of McrtCommands and return true if hit with one of them.
    /// Return false if given command line is not McrtControl command.
    /// This API only do test and any further operation for command line itself.
    ///
    static bool isCommand(const std::string &cmdLine);

    /// @brief Execute McrtControl command itself
    ///
    /// @param cmdLine command line for execution
    /// @param callBackRenderCompleteProcedure call-back function if command is "RenderCompleted".
    /// @param callBackGlobalProgressUpdate call-back function if command is "GlobalProgress".
    /// @return Return callBack result status or false is it's not a McrtCommand or error happend.
    ///
    /// @detail
    /// So far we only have two McrtControl commands. This is why we only have two call-back arguments.
    /// We will have more than two call-back function definitions when we add more McrtCommands in the future.
    /// If given msgStr is not McrtControl command or command format is wrong, this API returns false.
    /// Otherwise this API returns call-back function result.
    /// callBack function's 1st argument is syncId which you should compare with computation's
    /// current syncId. McrtControl commands always has syncId and intended only executed computation's
    /// syncId is equal as McrtControl command's syncId.
    ///
    bool run(const std::string& cmdLine,
             const std::function<bool(uint32_t /*syncId*/)>& callBackRenderCompleteProcedure,
             const std::function<void(uint32_t /*syncId*/, float /*fraction*/)>& callBackGlobalProgressUpdate);

protected:    
    int mMachineId;
};

} // namespace mcrt_dataio
