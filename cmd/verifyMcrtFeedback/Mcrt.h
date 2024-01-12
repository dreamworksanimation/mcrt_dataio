// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Fb.h"

namespace verifyFeedback {

class McrtMachine
{
public:
    McrtMachine() = default;

    bool readPPM(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId, const unsigned machineId);
    bool readFBD(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId, const unsigned machineId);

    unsigned getWidth() const { return mFeedbackBeauty.getWidth(); }
    unsigned getHeight() const { return mFeedbackBeauty.getHeight(); }

    unsigned getMachineId() const { return mMachineId; }

    bool isSameFeedback(const Fb& beauty, const Fb& beautyNumSample) const;
    bool isSameDecoded(const Fb& beauty, const Fb& beautyNumSample) const;

    const Fb& getMergedBeauty() const { return mMergedBeauty; }
    const Fb& getMergedBeautyNumSample() const { return mMergedBeautyNumSample; }

    const Fb& getMinusOneBeauty() const { return mMinusOneBeauty; }
    const Fb& getMinusOneBeautyNumSample() const { return mMinusOneBeautyNumSample; }

    std::string show() const;

private:
    void makeFilename(const std::string& filePath, bool isPpm);

    //------------------------------
    
    unsigned mFeedbackId {~static_cast<unsigned>(0)};
    unsigned mMachineId {~static_cast<unsigned>(0)};

    std::string mFeedbackBeautyFilename;
    std::string mFeedbackBeautyNumSampleFilename;
    std::string mDecodedBeautyFilename;
    std::string mDecodedBeautyNumSampleFilename;
    std::string mMergedBeautyFilename;
    std::string mMergedBeautyNumSampleFilename;
    std::string mMinusOneBeautyFilename;
    std::string mMinusOneBeautyNumSampleFilename;

    Fb mFeedbackBeauty;
    Fb mFeedbackBeautyNumSample;
    Fb mDecodedBeauty;
    Fb mDecodedBeautyNumSample;
    Fb mMergedBeauty;
    Fb mMergedBeautyNumSample;
    Fb mMinusOneBeauty;
    Fb mMinusOneBeautyNumSample;
};

class Mcrt
{
public:
    explicit Mcrt(const unsigned numMachines)
        : mMachineTbl(numMachines)
    {}

    bool readPPM(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId);
    bool readFBD(const std::string& filePath, // ended by '/'
                 const unsigned feedbackId);

    unsigned getWidth() const;
    unsigned getHeight() const;

    bool crawlAllMachine(const std::function<bool(const McrtMachine& mcrt)>& func) const;

    bool combineMergedAll(Fb& beautyOut, Fb& beautyNumSampleOut) const;

    std::string show() const;

private:

    unsigned mFeedbackId;

    std::vector<McrtMachine> mMachineTbl;
};

} // namespace verifyFeedback
