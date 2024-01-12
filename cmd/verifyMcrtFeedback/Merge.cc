// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Merge.h"

#include <scene_rdl2/render/util/StrUtil.h>

namespace verifyFeedback {

bool
MergeMachine::readPPM(const std::string& filePath, // ended by '/'
                      const unsigned feedbackId, const unsigned machineId)
{
    auto msgOut = [](const std::string& msg) -> bool {
        std::cerr << msg << '\n';
        return true;
    };

    mFeedbackId = feedbackId;
    mMachineId = machineId;

    makeFilename(filePath, true);
    return (mBeauty.readBeautyPPM(mBeautyFilename, msgOut) &&
            mBeautyNumSample.readBeautyNumSamplePPM(mBeautyNumSampleFilename, msgOut));
}

bool
MergeMachine::readFBD(const std::string& filePath, // ended by '/'
                      const unsigned feedbackId, const unsigned machineId)
{
    auto msgOut = [](const std::string& msg) -> bool {
        std::cerr << msg << '\n';
        return true;
    };

    mFeedbackId = feedbackId;
    mMachineId = machineId;

    makeFilename(filePath, false);
    return (mBeauty.readBeautyFBD(mBeautyFilename, msgOut) &&
            mBeautyNumSample.readBeautyNumSampleFBD(mBeautyNumSampleFilename, msgOut));
}

std::string
MergeMachine::show() const
{
    std::ostringstream ostr;
    ostr << "MergeMachine {\n"
         << "  mFeedbackId:" << mFeedbackId << '\n'
         << "  mMachineId :" << mMachineId << '\n'
         << "  mBeautyFilename         :" << mBeautyFilename << '\n'
         << "  mBeautyNumSampleFilename:" << mBeautyNumSampleFilename << '\n'
         << "  mBeauty.width :" << mBeauty.getWidth() << '\n'
         << "  mBeauty.height:" << mBeauty.getHeight() << '\n'
         << "  mBeautyNumSample.width :" << mBeautyNumSample.getWidth() << '\n'
         << "  mBeautyNumSample.height:" << mBeautyNumSample.getHeight() << '\n'
         << "}";
    return ostr.str();
}

void
MergeMachine::makeFilename(const std::string& filePath, // ended by '/'
                           bool isPpm)
{
    std::ostringstream ostr;
    ostr << filePath << "merge"
         << "_fId" << mFeedbackId
         << "_mId" << mMachineId;
    std::string head = ostr.str();

    if (isPpm) {
        mBeautyFilename = head + "_beauty.ppm";
        mBeautyNumSampleFilename = head + "_beautyNumSample.ppm";
    } else {
        mBeautyFilename = head + "_beauty.fbd";
        mBeautyNumSampleFilename = head + "_beautyNumSample.fbd";
    }
}

//------------------------------------------------------------------------------------------

bool
Merge::readPPM(const std::string& filePath, // ended by '/'
               const unsigned feedbackId)
{
    mFeedbackId = feedbackId;

    auto msgOut = [](const std::string& msg) -> bool {
        std::cerr << msg << '\n';
        return true;
    };

    makeFilename(filePath, mFeedbackId, true);
    if (!mMergeAllBeauty.readBeautyPPM(mMergeAllBeautyFilename, msgOut) ||
        !mMergeAllBeautyNumSample.readBeautyNumSamplePPM(mMergeAllBeautyNumSampleFilename, msgOut)) {
        return false;
    }

    for (unsigned machineId = 0; machineId < mMachineTbl.size(); ++machineId) {
        if (!mMachineTbl[machineId].readPPM(filePath, mFeedbackId, machineId)) {
            return false;
        }
    }
    return true;
}

bool
Merge::readFBD(const std::string& filePath, // ended by '/'
               const unsigned feedbackId)
{
    mFeedbackId = feedbackId;

    auto msgOut = [](const std::string& msg) -> bool {
        std::cerr << msg << '\n';
        return true;
    };

    makeFilename(filePath, mFeedbackId, false);
    if (!mMergeAllBeauty.readBeautyFBD(mMergeAllBeautyFilename, msgOut) ||
        !mMergeAllBeautyNumSample.readBeautyNumSampleFBD(mMergeAllBeautyNumSampleFilename, msgOut)) {
        return false;
    }

    for (unsigned machineId = 0; machineId < mMachineTbl.size(); ++machineId) {
        if (!mMachineTbl[machineId].readFBD(filePath, mFeedbackId, machineId)) {
            return false;
        }
    }
    return true;
}

std::string
Merge::show() const
{
    std::ostringstream ostr;
    ostr << "Merge {\n"
         << "  mFeedbackId:" << mFeedbackId << '\n'
         << "  mMergeAllBeautyFilename         :" << mMergeAllBeautyFilename << '\n'
         << "  mMergeAllBeautyNumSampleFilename:" << mMergeAllBeautyNumSampleFilename << '\n'
         << "  mMergeAllBeauty.width :" << mMergeAllBeauty.getWidth() << '\n'
         << "  mMergeAllBeauty.height:" << mMergeAllBeauty.getHeight() << '\n'
         << "  mMergeAllBeautyNumSample.width :" << mMergeAllBeautyNumSample.getWidth() << '\n'
         << "  mMergeAllBeautyNumSample.height:" << mMergeAllBeautyNumSample.getHeight() << '\n'
         << "  mMachineTbl (size:" << mMachineTbl.size() << ") {\n";
    for (size_t i = 0; i < mMachineTbl.size(); ++i) {
        ostr << scene_rdl2::str_util::addIndent(mMachineTbl[i].show(), 2) << '\n';
    }
    ostr << "  }\n"
         << "}";
    return ostr.str();
}

void
Merge::makeFilename(const std::string& filePath, // ended by '/'
                    const unsigned feedbackId,
                    bool isPpm)
{
    std::ostringstream ostr;
    ostr << filePath << "mergeAll"
         << "_fId" << feedbackId;
    std::string head = ostr.str();

    if (isPpm) {
        mMergeAllBeautyFilename = head + "_beauty.ppm";
        mMergeAllBeautyNumSampleFilename = head + "_beautyNumSample.ppm";
    } else {
        mMergeAllBeautyFilename = head + "_beauty.fbd";
        mMergeAllBeautyNumSampleFilename = head + "_beautyNumSample.fbd";
    }
}

} // namespace verifyFeedback
