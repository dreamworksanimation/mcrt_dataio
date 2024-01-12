// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Mcrt.h"

#include <scene_rdl2/render/util/StrUtil.h>

namespace verifyFeedback {

bool
McrtMachine::readPPM(const std::string& filePath,
                     const unsigned feedbackId, const unsigned machineId)
{
    auto msgOut = [](const std::string& msg) -> bool {
        std::cerr << msg << '\n';
        return true;
    };

    mFeedbackId = feedbackId;
    mMachineId = machineId;

    makeFilename(filePath, true);
    return (mFeedbackBeauty.readBeautyPPM(mFeedbackBeautyFilename, msgOut) &&
            mFeedbackBeautyNumSample.readBeautyNumSamplePPM(mFeedbackBeautyNumSampleFilename, msgOut) &&
            mDecodedBeauty.readBeautyPPM(mDecodedBeautyFilename, msgOut) &&
            mDecodedBeautyNumSample.readBeautyNumSamplePPM(mDecodedBeautyNumSampleFilename, msgOut) &&
            mMergedBeauty.readBeautyPPM(mMergedBeautyFilename, msgOut) &&
            mMergedBeautyNumSample.readBeautyNumSamplePPM(mMergedBeautyNumSampleFilename, msgOut) &&
            mMinusOneBeauty.readBeautyPPM(mMinusOneBeautyFilename, msgOut) &&
            mMinusOneBeautyNumSample.readBeautyNumSamplePPM(mMinusOneBeautyNumSampleFilename, msgOut));
}

bool
McrtMachine::readFBD(const std::string& filePath,
                     const unsigned feedbackId, const unsigned machineId)
{
    auto msgOut = [](const std::string& msg) -> bool {
        std::cerr << msg << '\n';
        return true;
    };

    mFeedbackId = feedbackId;
    mMachineId = machineId;

    makeFilename(filePath, false);
    return (mFeedbackBeauty.readBeautyFBD(mFeedbackBeautyFilename, msgOut) &&
            mFeedbackBeautyNumSample.readBeautyNumSampleFBD(mFeedbackBeautyNumSampleFilename, msgOut) &&
            mDecodedBeauty.readBeautyFBD(mDecodedBeautyFilename, msgOut) &&
            mDecodedBeautyNumSample.readBeautyNumSampleFBD(mDecodedBeautyNumSampleFilename, msgOut) &&
            mMergedBeauty.readBeautyFBD(mMergedBeautyFilename, msgOut) &&
            mMergedBeautyNumSample.readBeautyNumSampleFBD(mMergedBeautyNumSampleFilename, msgOut) &&
            mMinusOneBeauty.readBeautyFBD(mMinusOneBeautyFilename, msgOut) &&
            mMinusOneBeautyNumSample.readBeautyNumSampleFBD(mMinusOneBeautyNumSampleFilename, msgOut));
}

bool
McrtMachine::isSameFeedback(const Fb& beauty, const Fb& beautyNumSample) const
{
    return (beauty == mFeedbackBeauty && beautyNumSample == mFeedbackBeautyNumSample);
}

bool
McrtMachine::isSameDecoded(const Fb& beauty, const Fb& beautyNumSample) const
{
    return (beauty == mDecodedBeauty && beautyNumSample == mDecodedBeautyNumSample);
}

std::string
McrtMachine::show() const
{
    std::ostringstream ostr;
    ostr << "MergeMachine {\n"
         << "  mFeedbackId:" << mFeedbackId << '\n'
         << "  mMachineId :" << mMachineId << '\n'
         << "  mFeedbackBeautyFilename         :" << mFeedbackBeautyFilename << '\n'
         << "  mFeedbackBeautyNumSampleFilename:" << mFeedbackBeautyNumSampleFilename << '\n'
         << "  mDecodedBeautyFilename          :" << mDecodedBeautyFilename << '\n'
         << "  mDecodedBeautyNumSampleFilename :" << mDecodedBeautyNumSampleFilename << '\n'
         << "  mMergedBeautyFilename           :" << mMergedBeautyFilename << '\n'
         << "  mMergedBeautyNumSampleFilename  :" << mMergedBeautyNumSampleFilename << '\n'
         << "  mMinusOneBeautyFilename         :" << mMinusOneBeautyFilename << '\n'
         << "  mMinusOneBeautyNumSampleFilename:" << mMinusOneBeautyNumSampleFilename << '\n'
         << "  mFeedbackBeauty.width :" << mFeedbackBeauty.getWidth() << '\n'
         << "  mFeedbackBeauty.height:" << mFeedbackBeauty.getHeight() << '\n'
         << "  mFeedbackBeautyNumSample.width :" << mFeedbackBeautyNumSample.getWidth() << '\n'
         << "  mFeedbackBeautyNumSample.height:" << mFeedbackBeautyNumSample.getHeight() << '\n'
         << "  mDecodedBeauty.width  :" << mDecodedBeauty.getWidth() << '\n'
         << "  mDecodedBeauty.height :" << mDecodedBeauty.getHeight() << '\n'
         << "  mDecodedBeautyNumSample.width  :" << mDecodedBeautyNumSample.getWidth() << '\n'
         << "  mDecodedBeautyNumSample.height :" << mDecodedBeautyNumSample.getHeight() << '\n'
         << "  mMergedBeauty.width   :" << mMergedBeauty.getWidth() << '\n'
         << "  mMergedBeauty.height  :" << mMergedBeauty.getHeight() << '\n'
         << "  mMergedBeautyNumSample.width   :" << mMergedBeautyNumSample.getWidth() << '\n'
         << "  mMergedBeautyNumSample.height  :" << mMergedBeautyNumSample.getHeight() << '\n'
         << "  mMinusOneBeauty.width :" << mMinusOneBeauty.getWidth() << '\n'
         << "  mMinusOneBeauty.height:" << mMinusOneBeauty.getHeight() << '\n'
         << "  mMinusOneBeautyNumSample.width :" << mMinusOneBeautyNumSample.getWidth() << '\n'
         << "  mMinusOneBeautyNumSample.height:" << mMinusOneBeautyNumSample.getHeight() << '\n'
         << "}";
    return ostr.str();
}

void
McrtMachine::makeFilename(const std::string& filePath, bool isPpm)
{
    std::ostringstream ostr;
    ostr << filePath << "mcrt"
         << "_fId" << mFeedbackId
         << "_mId" << mMachineId;
    std::string head = ostr.str();

    if (isPpm) {
        mFeedbackBeautyFilename          = head + "_beauty_feedback.ppm";
        mFeedbackBeautyNumSampleFilename = head + "_beautyNumSample_feedback.ppm";    
        mDecodedBeautyFilename           = head + "_beauty_decoded.ppm";
        mDecodedBeautyNumSampleFilename  = head + "_beautyNumSample_decoded.ppm";    
        mMergedBeautyFilename            = head + "_beauty_merged.ppm";
        mMergedBeautyNumSampleFilename   = head + "_beautyNumSample_merged.ppm";    
        mMinusOneBeautyFilename          = head + "_beauty_minusOne.ppm";
        mMinusOneBeautyNumSampleFilename = head + "_beautyNumSample_minusOne.ppm";
    } else {
        mFeedbackBeautyFilename          = head + "_beauty_feedback.fbd";
        mFeedbackBeautyNumSampleFilename = head + "_beautyNumSample_feedback.fbd";    
        mDecodedBeautyFilename           = head + "_beauty_decoded.fbd";
        mDecodedBeautyNumSampleFilename  = head + "_beautyNumSample_decoded.fbd";    
        mMergedBeautyFilename            = head + "_beauty_merged.fbd";
        mMergedBeautyNumSampleFilename   = head + "_beautyNumSample_merged.fbd";    
        mMinusOneBeautyFilename          = head + "_beauty_minusOne.fbd";
        mMinusOneBeautyNumSampleFilename = head + "_beautyNumSample_minusOne.fbd";
    }
}

//------------------------------------------------------------------------------------------

bool
Mcrt::readPPM(const std::string& filePath, // ended by '/'
              const unsigned feedbackId)
{
    mFeedbackId = feedbackId;

    for (unsigned machineId = 0; machineId < mMachineTbl.size(); ++machineId) {
        if (!mMachineTbl[machineId].readPPM(filePath, mFeedbackId, machineId)) {
            return false;
        }
    }
    return true;
}

bool
Mcrt::readFBD(const std::string& filePath, // ended by '/'
              const unsigned feedbackId)
{
    mFeedbackId = feedbackId;

    for (unsigned machineId = 0; machineId < mMachineTbl.size(); ++machineId) {
        if (!mMachineTbl[machineId].readFBD(filePath, mFeedbackId, machineId)) {
            return false;
        }
    }
    return true;
}

bool
Mcrt::crawlAllMachine(const std::function<bool(const McrtMachine& mcrt)>& func) const
{
    for (size_t machineId = 0; machineId < mMachineTbl.size(); ++machineId) {
        if (!func(mMachineTbl[machineId])) return false;
    }
    return true;
}

bool
Mcrt::combineMergedAll(Fb& beautyOut, Fb& beautyNumSampleOut) const
{
    beautyOut.resize(getWidth(), getHeight());
    beautyOut.clear();
    beautyNumSampleOut.resize(getWidth(), getHeight());
    beautyNumSampleOut.clear();

    bool flag = true;
    crawlAllMachine([&](const McrtMachine& mcrt) -> bool {
            if (!Fb::merge(beautyOut, beautyNumSampleOut,
                           mcrt.getMergedBeauty(), mcrt.getMergedBeautyNumSample())) {
                std::cerr << "combine merged all failed\n";
                flag = false;
                return false;
            }
            return true;
        });
    return flag;
}

std::string
Mcrt::show() const
{
    std::ostringstream ostr;
    ostr << "Mcrt {\n"
         << "  mFeedbackId:" << mFeedbackId << '\n'
         << "  mMachineTbl (size:" << mMachineTbl.size() << ") {\n";
    for (size_t i = 0; i < mMachineTbl.size(); ++i) {
        ostr << scene_rdl2::str_util::addIndent(mMachineTbl[i].show(), 2) << '\n';
    }
    ostr << "  }\n"
         << "}";
    return ostr.str();
}

unsigned
Mcrt::getWidth() const
{
    if (mMachineTbl.empty()) return 0;
    return mMachineTbl[0].getWidth();
}

unsigned    
Mcrt::getHeight() const
{
    if (mMachineTbl.empty()) return 0;
    return mMachineTbl[0].getHeight();
}

} // namespace verifyFeedback
