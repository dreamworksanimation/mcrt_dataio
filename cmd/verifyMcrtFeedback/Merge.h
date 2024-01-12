// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Fb.h"

namespace verifyFeedback {

class MergeMachine
{
public:
    MergeMachine()
        : mFeedbackId(~0)
        , mMachineId(~0)
    {}

    bool readPPM(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId, const unsigned machineId);
    bool readFBD(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId, const unsigned machineId);

    const Fb& getBeauty() const { return mBeauty; }
    const Fb& getBeautyNumSample() const { return mBeautyNumSample; }

    std::string show() const;

private:
    void makeFilename(const std::string& filePath, // ended by '/'
                      bool isPpm);

    //------------------------------

    unsigned mFeedbackId;
    unsigned mMachineId;
    std::string mBeautyFilename;
    std::string mBeautyNumSampleFilename;

    Fb mBeauty;
    Fb mBeautyNumSample;
};

class Merge
{
public:
    explicit Merge(const unsigned numMachines)
        : mMachineTbl(numMachines)
    {}

    bool readPPM(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId);
    bool readFBD(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId);

    const Fb& getMergeAllBeauty() const { return mMergeAllBeauty; }
    const Fb& getMergeAllBeautyNumSample() const { return mMergeAllBeautyNumSample; }

    const MergeMachine& getMachine(unsigned machineId) const { return mMachineTbl[machineId]; }

    std::string show() const;

private:
    void makeFilename(const std::string& filePath, // ended by '/'
                      const unsigned feedbackId,
                      bool isPpm);

    //------------------------------

    unsigned mFeedbackId;

    std::string mMergeAllBeautyFilename;
    std::string mMergeAllBeautyNumSampleFilename;

    Fb mMergeAllBeauty;
    Fb mMergeAllBeautyNumSample;

    std::vector<MergeMachine> mMachineTbl;
};

} // namespace verifyFeedback
