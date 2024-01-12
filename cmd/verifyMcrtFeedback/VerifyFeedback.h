// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/common/grid_util/Arg.h>
#include <scene_rdl2/common/grid_util/Parser.h>

#include "Mcrt.h"
#include "Merge.h"

#include <memory> // unique_ptr

namespace verifyFeedback {

class VerifyFeedback
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;

    VerifyFeedback()
        : mFilePath("./")
        , mNumMachines(0)
        , mMerge(nullptr)
        , mMcrt(nullptr)
    {
        parserConfigure();
    }

    bool main(int ac, char **av);

private:

    void init(const unsigned numMachines);

    bool readPPM(const unsigned feedbackId);
    bool readFBD(const unsigned feedbackId);    

    bool verify() const;
    bool verifyMergeAllWithMcrtFeedback() const;
    bool verifyMergeWithMcrtDecoded() const;
    bool verifyReconstructMergeWithFeedback() const;
    bool verifyMinusOne() const;

    std::string show() const;

    void parserConfigure();

    //------------------------------

    std::string mFilePath;
    unsigned mNumMachines;

    std::unique_ptr<Merge> mMerge;
    std::unique_ptr<Mcrt> mMcrt;

    Parser mParser;
};

} // namespace verifyFeedback
