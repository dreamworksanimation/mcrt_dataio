// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "VerifyFeedback.h"

#include <scene_rdl2/render/util/StrUtil.h>

namespace verifyFeedback {

bool
VerifyFeedback::main(int ac, char **av)
{
    return mParser.main(scene_rdl2::grid_util::Arg(ac, av));
}

//------------------------------------------------------------------------------------------

void
VerifyFeedback::init(const unsigned numMachines)
{
    mNumMachines = numMachines;

    mMerge.reset(new Merge(mNumMachines));
    mMcrt.reset(new Mcrt(mNumMachines));
}

bool
VerifyFeedback::readPPM(const unsigned feedbackId)
{
    if (mMerge) {
        if (!mMerge->readPPM(mFilePath, feedbackId)) return false;
    } else {
        std::cerr << "ERROR: mMerge is empty\n";
    }

    if (mMcrt) {
        if (!mMcrt->readPPM(mFilePath, feedbackId)) return false;
    } else {
        std::cerr << "ERROR: mMcrt is empty\n";
    }

    return true;
}

bool
VerifyFeedback::readFBD(const unsigned feedbackId)
{
    if (mMerge) {
        if (!mMerge->readFBD(mFilePath, feedbackId)) return false;
    } else {
        std::cerr << "ERROR: mMerge is empty\n";
    }

    if (mMcrt) {
        if (!mMcrt->readFBD(mFilePath, feedbackId)) return false;
    } else {
        std::cerr << "ERROR: mMcrt is empty\n";
    }

    return true;
}

bool
VerifyFeedback::verify() const
{
    if (!mMerge || !mMcrt) return false;

    bool flag = true;
    if (!verifyMergeAllWithMcrtFeedback()) flag = false;
    if (!verifyMergeWithMcrtDecoded()) flag = false;
    if (!verifyReconstructMergeWithFeedback()) flag = false;
    if (!verifyMinusOne()) flag = false;

    return flag;
}

bool
VerifyFeedback::verifyMergeAllWithMcrtFeedback() const
{
    std::cerr << "Verify MergeAll with MCRT feedback start ...";

    if (!mMcrt->crawlAllMachine([&](const McrtMachine& mcrt) -> bool {
                return mcrt.isSameFeedback(mMerge->getMergeAllBeauty(), mMerge->getMergeAllBeautyNumSample());
            })) {
        std::cerr << "Verify MergeAll with MCRT feedback failed.\n";
        return false;
    }

    std::cerr << " OK!\n";
    return true;
}

bool
VerifyFeedback::verifyMergeWithMcrtDecoded() const
{
    std::cerr << "Verify Merge with McrtDecoded start ...";

    if (!mMcrt->crawlAllMachine([&](const McrtMachine& mcrt) -> bool {
                return mcrt.isSameDecoded(mMerge->getMachine(mcrt.getMachineId()).getBeauty(),
                                          mMerge->getMachine(mcrt.getMachineId()).getBeautyNumSample());
            })) {
        std::cerr << "Verify Merge with McrtDecoded failed.\n";
        return false;
    }

    std::cerr << " OK!\n";
    return true;
}

bool
VerifyFeedback::verifyReconstructMergeWithFeedback() const    
{
    std::cerr << "Verify reconstruct merge data with feedback start ...";

    Fb mergeBeauty, mergeBeautyNumSample;
    if (!mMcrt->combineMergedAll(mergeBeauty, mergeBeautyNumSample)) {
        std::cerr << "combine merged all action failed.\n";
        return false;
    }

    if (!mMcrt->crawlAllMachine([&](const McrtMachine& mcrt) -> bool {
                return mcrt.isSameFeedback(mergeBeauty, mergeBeautyNumSample);
            })) {
        std::cerr << "Verify reconstruct merge data with feedback failed\n";
        return false;
    }

    /* useful debug code
    const Fb& mergeAll = mMerge->getMergeAllBeauty();
    Fb diff = mergeAll - mergeBeauty;
    diff.abs();
    diff.normalize();

    mergeAll.writeBeautyPPM("/usr/home/tkato/ppm2/tmpMergeAll.ppm",
                            [&](const std::string& msg) -> bool {
                                std::cerr << msg << '\n';
                                return true;
                            });

    diff.writeBeautyPPM("/usr/home/tkato/ppm2/tmpMergeDiff.ppm",
                        [&](const std::string& msg) -> bool {
                            std::cerr << msg << '\n';
                            return true;
                        });

    mergeBeauty.writeBeautyPPM("/usr/home/tkato/ppm2/tmpMerge.ppm",
                               [&](const std::string& msg) -> bool {
                                   std::cerr << msg << '\n';
                                   return true;
                               });
    mergeBeautyNumSample.writeBeautyNumSamplePPM("/usr/home/tkato/ppm2/tmpMergeN.ppm",
                                                 [&](const std::string& msg) -> bool {
                                                     std::cerr << msg << '\n';
                                                     return true;
                                                 });
    */

    std::cerr << " OK!\n";
    return true;
}

bool
VerifyFeedback::verifyMinusOne() const
{
    std::cerr << "Verify minusOne data start ...";

    auto minusOneGen = [&](const unsigned omitMachineId,
                           Fb& minusOneBeauty, Fb& minusOneBeautyNumSample) -> bool {
        minusOneBeauty.clear();
        minusOneBeautyNumSample.clear();

        bool flag = true;
        mMcrt->crawlAllMachine([&](const McrtMachine& mcrt) -> bool {
                if (mcrt.getMachineId() != omitMachineId) {
                    if (!Fb::merge(minusOneBeauty, minusOneBeautyNumSample,
                                   mcrt.getMergedBeauty(), mcrt.getMergedBeautyNumSample())) {
                        std::cerr << "minusOneGen failed\n";
                        flag = false;
                        return false;
                    }
                }
                return true;
            });
        return flag;
    };

    bool flag = true;
    if (!mMcrt->crawlAllMachine([&](const McrtMachine& mcrt) -> bool {
                Fb minusOneBeauty(mMcrt->getWidth(), mMcrt->getHeight());
                Fb minusOneBeautyNumSample(mMcrt->getWidth(), mMcrt->getHeight());
                if (!minusOneGen(mcrt.getMachineId(), minusOneBeauty, minusOneBeautyNumSample)) {
                    std::cerr << "verifyMinusOne failed. machineId:" << mcrt.getMachineId() << '\n';
                    flag = false;
                    return false;
                }
                
                /* useful debug code
                {
                    std::ostringstream ostr;
                    ostr << "/usr/home/tkato/ppm/minusOneVerify_mId" << mcrt.getMachineId() << ".ppm";
                    minusOneBeauty.writeBeautyPPM(ostr.str(),
                                                  [&](const std::string& msg) -> bool {
                                                      std::cerr << msg << '\n';
                                                      return true;
                                                  });
                }
                {
                    std::ostringstream ostr;
                    ostr << "/usr/home/tkato/ppm/minusOneVerify_mId" << mcrt.getMachineId() << "_diff.ppm";
                    Fb diff = minusOneBeauty - mcrt.getMinusOneBeauty();
                    diff.abs();
                    diff.normalize();
                    diff.writeBeautyPPM(ostr.str(),
                                        [&](const std::string& msg) -> bool {
                                            std::cerr << msg << '\n';
                                            return true;
                                        });
                }
                */

                if (minusOneBeauty != mcrt.getMinusOneBeauty() ||
                    minusOneBeautyNumSample != mcrt.getMinusOneBeautyNumSample()) {
                    std::cerr << "verifyMinusOne failed. result mismatch. machineId:" << mcrt.getMachineId() << '\n';
                    flag = false;
                    return false;
                }
                return true;
            })) {
    }

    if (flag) std::cerr << " OK!\n";
    return flag;
}

std::string
VerifyFeedback::show() const
{
    std::ostringstream ostr;
    ostr << "VerifyFeedback {\n"
         << "  mFilePath:" << mFilePath << '\n'
         << "  mNumMachines:" << mNumMachines << '\n';
    if (mMerge) {
        ostr << scene_rdl2::str_util::addIndent(mMerge->show()) << '\n';
    } else {
        ostr << "  mMerge is empty\n";
    }
    if (mMcrt) {
        ostr << scene_rdl2::str_util::addIndent(mMcrt->show()) << '\n';
    } else {
        ostr << "  mMcrt is empty\n";
    }
    ostr << "}";
    return ostr.str();
}

void
VerifyFeedback::parserConfigure()
{
    mParser.description("verifyFeedback command");
    mParser.opt("-init", "<numMachine>", "initialize memory. should be specified as first option",
                [&](Arg& arg) -> bool { init((arg++).as<unsigned>(0)); return true; });
    mParser.opt("-pathSet", "<filePath>", "set file path. should be ended by '/'",
                [&](Arg& arg) -> bool { mFilePath = (arg++)(); return true; });
    mParser.opt("-pathShow", "", "show filePath",
                [&](Arg& arg) -> bool { return arg.msg(std::string("mFilePath=") + mFilePath + '\n'); });
    mParser.opt("-readPPM", "<feedbackId>", "read ppm data",
                [&](Arg& arg) -> bool { return readPPM((arg++).as<int>(0)); });
    mParser.opt("-readFBD", "<feedbackId>", "read fbd data",
                [&](Arg& arg) -> bool { return readFBD((arg++).as<int>(0)); });
    mParser.opt("-show", "", "show internal info",
                [&](Arg& arg) -> bool { return arg.msg(show() + '\n'); });
    mParser.opt("-verify", "", "run verify",
                [&](Arg& arg) -> bool { return verify(); });
}

} // namespace verifyFeedback
