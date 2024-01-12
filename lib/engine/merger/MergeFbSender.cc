// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "MergeFbSender.h"

#include <scene_rdl2/common/grid_util/FbReferenceType.h>
#include <scene_rdl2/common/grid_util/PackTiles.h>
#include <scene_rdl2/common/grid_util/ProgressiveFrameBufferName.h>

//#define DEBUG_MSG

namespace mcrt_dataio {

void
MergeFbSender::init(const scene_rdl2::math::Viewport &rezedViewport)
//
// w and h are original size and not need to be tile size aligned
//
{
    mFb.init(rezedViewport);
    mFbActivePixels.init(rezedViewport.width(), rezedViewport.height());

    mMin = 0;                   // for statistical analyze
    mMax = 0;
}

void
MergeFbSender::setHeaderInfoAndFbReset(FbMsgSingleFrame* currFbMsgSingleFrame,
                                       const mcrt::BaseFrame::Status* overwriteFrameStatusPtr)
{
    // This MergeFbSender is used for 2 purposes.
    //   a) send progressiveFrame message to the client at merge computation
    //   b) send progressiveFeedback message to MCRT computation at merge computation.
    // In case of a, we simply use frame status by providing currFbMsgSingleFrame.
    // But the case of b, we need to overwrite the status by specifying the status by argument.
    if (overwriteFrameStatusPtr) {
        mFrameStatus = *overwriteFrameStatusPtr;
    } else {
        mFrameStatus = currFbMsgSingleFrame->getStatus();
    }

    mProgressFraction = currFbMsgSingleFrame->getProgressFraction();
    mSnapshotStartTime = currFbMsgSingleFrame->getSnapshotStartTime();
    mCoarsePassStatus = (currFbMsgSingleFrame->isCoarsePassDone())? false: true;
    mDenoiserAlbedoInputName = currFbMsgSingleFrame->getDenoiserAlbedoInputName();
    mDenoiserNormalInputName = currFbMsgSingleFrame->getDenoiserNormalInputName();

    if (mFrameStatus == mcrt::BaseFrame::STARTED) {
        fbReset(); // we need to reset previous fb result to create activePixels information properly.
    }

    mBeautyHDRITest = HdriTestCondition::INIT; // condition of HDRI test for beauty buffer
}

void
MergeFbSender::encodeUpstreamLatencyLog(FbMsgSingleFrame *frame)
{
    mUpstreamLatencyLogWork.clear();
    scene_rdl2::rdl2::ValueContainerEnq vContainerEnq(&mUpstreamLatencyLogWork);
    frame->encodeLatencyLog(vContainerEnq);
    vContainerEnq.finalize();
}

void
MergeFbSender::addBeautyBuff(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_BEAUTY);
    {
        PackTilePrecision packTilePrecision =
            calcPackTilePrecision(mFb.getRenderBufferCoarsePassPrecision(),
                                  mFb.getRenderBufferFinePassPrecision(),
                                  [&]() -> PackTilePrecision { // runtimeDecisionFunc for coarse pass
                                      return getBeautyHDRITestResult();
                                  });
        mWork.clear();
        mLastBeautyBufferSize =
            scene_rdl2::grid_util::PackTiles::
            encode(false,
                   mFbActivePixels.getActivePixels(),
                   mFb.getRenderBufferTiled(),
                   mWork,
                   packTilePrecision,
                   mFb.getRenderBufferCoarsePassPrecision(),
                   mFb.getRenderBufferFinePassPrecision(),
                   sha1HashSw); // RGBA : float * 4
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_BEAUTY);

    /* runtime verify
    if (!scene_rdl2::grid_util::PackTiles::verifyEncodeResultMerge(mWork.data(), mWork.size(), mFb)) {
        std::cerr << "verify NG" << std::endl;
    } else {
        std::cerr << "verify OK" << std::endl;
    }
    */

    /* SHA1 hash verify for debug
    if (sha1HashSw) {
        if (!scene_rdl2::grid_util::PackTiles::verifyDecodeHash(mWork.data(), mWork.size())) {
            std::cerr << ">> MergeFbSender.cc hashVerify NG" << std::endl;
        } else {
            std::cerr << ">> MergeFbSender.cc hashVerify OK" << std::endl;
        }
    }
    */
    /* useful debug dump
    std::cerr << scene_rdl2::grid_util::PackTiles::showHash(">> progmcrt_merge MergeFbSender.cc ",
                                                       (const unsigned char *)mWork.data())
              << std::endl;
    */

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastBeautyBufferSize,
                       scene_rdl2::grid_util::ProgressiveFrameBufferName::Beauty,
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_BEAUTY);
    mLatencyLog.addDataSize(mLastBeautyBufferSize);
}

void
MergeFbSender::MergeFbSender::addBeautyBuffWithNumSample(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_BEAUTY_NUMSAMPLE);
    {
        PackTilePrecision packTilePrecision =
            calcPackTilePrecision(mFb.getRenderBufferCoarsePassPrecision(),
                                  mFb.getRenderBufferFinePassPrecision(),
                                  [&]() -> PackTilePrecision { // runtimeDecisionFunc for coarse pass
                                      return getBeautyHDRITestResult();
                                  });
        mWork.clear();
        mLastBeautyBufferNumSampleSize =
            scene_rdl2::grid_util::PackTiles::
            encode(false, // renderBufferOdd
                   mFbActivePixels.getActivePixels(),
                   mFb.getRenderBufferTiled(),
                   mFb.getNumSampleBufferTiled(),
                   mWork,
                   packTilePrecision,
                   mFb.getRenderBufferCoarsePassPrecision(),
                   mFb.getRenderBufferFinePassPrecision(),
                   sha1HashSw); // RGBA + numSample : float * 4 + u_int
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_BEAUTY_NUMSAMPLE);

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastBeautyBufferNumSampleSize,
                       scene_rdl2::grid_util::ProgressiveFrameBufferName::Beauty,
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_BEAUTY_NUMSAMPLE);
    mLatencyLog.addDataSize(mLastBeautyBufferNumSampleSize);
}

void
MergeFbSender::addPixelInfo(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_PIXELINFO);
    {
        PackTilePrecision packTilePrecision =
            calcPackTilePrecision(mFb.getPixelInfoCoarsePassPrecision(),
                                  mFb.getPixelInfoFinePassPrecision());
        mWork.clear();
        mLastPixelInfoSize =
            scene_rdl2::grid_util::PackTiles::
            encodePixelInfo(mFbActivePixels.getActivePixelsPixelInfo(),
                            mFb.getPixelInfoBufferTiled(),
                            mWork,
                            packTilePrecision,
                            mFb.getPixelInfoCoarsePassPrecision(),
                            mFb.getPixelInfoFinePassPrecision(),
                            sha1HashSw);
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_PIXELINFO);

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastPixelInfoSize,
                       mFb.getPixelInfoName().c_str(),
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_PIXELINFO);
    mLatencyLog.addDataSize(mLastPixelInfoSize);
}

void
MergeFbSender::addHeatMap(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_HEATMAP);
    {
        mWork.clear();
        mLastHeatMapSize =
            scene_rdl2::grid_util::PackTiles::
            encodeHeatMap(mFbActivePixels.getActivePixelsHeatMap(),
                          mFb.getHeatMapSecBufferTiled(),
                          mWork,
                          sha1HashSw);
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_HEATMAP);

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastHeatMapSize,
                       mFb.getHeatMapName().c_str(),
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_HEATMAP);
    mLatencyLog.addDataSize(mLastHeatMapSize);
}

void
MergeFbSender::addHeatMapWithNumSample(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_HEATMAP_NUMSAMPLE);
    {
        mWork.clear();
        mLastHeatMapNumSampleSize =
            scene_rdl2::grid_util::PackTiles::
            encodeHeatMap(mFbActivePixels.getActivePixelsHeatMap(),
                          mFb.getHeatMapSecBufferTiled(),
                          mFb.getWeightBufferTiled(),
                          mWork,
                          false, // noNumSampleMode
                          sha1HashSw);
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_HEATMAP_NUMSAMPLE);

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastHeatMapNumSampleSize,
                       mFb.getHeatMapName().c_str(),
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_HEATMAP_NUMSAMPLE);
    mLatencyLog.addDataSize(mLastHeatMapNumSampleSize);
}

void
MergeFbSender::addWeightBuffer(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_WEIGHTBUFFER);
    {
        PackTilePrecision packTilePrecision =
            calcPackTilePrecision(mFb.getWeightBufferCoarsePassPrecision(),
                                  mFb.getWeightBufferFinePassPrecision());
        mWork.clear();
        mLastWeightBufferSize =
            scene_rdl2::grid_util::PackTiles::
            encodeWeightBuffer(mFbActivePixels.getActivePixelsWeightBuffer(),
                               mFb.getWeightBufferTiled(),
                               mWork,
                               packTilePrecision,
                               mFb.getWeightBufferCoarsePassPrecision(),
                               mFb.getWeightBufferFinePassPrecision(),
                               sha1HashSw);
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_WEIGHTBUFFER);

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastWeightBufferSize,
                       mFb.getWeightBufferName().c_str(),
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_WEIGHTBUFFER);
    mLatencyLog.addDataSize(mLastWeightBufferSize);
}

void
MergeFbSender::addRenderBufferOdd(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_RENDERBUFFERODD);
    {
        PackTilePrecision packTilePrecision =
            calcPackTilePrecision(mFb.getRenderBufferCoarsePassPrecision(),
                                  mFb.getRenderBufferFinePassPrecision(),
                                  [&]() -> PackTilePrecision { // runtimeDecisionFunc for coarse pass
                                      return getBeautyHDRITestResult(); // access shared beauty HDRI test result
                                  });
        mWork.clear();
        // Actually we don't have {coarse, fine}PassPrecision info for renderBufferOdd.
        mLastRenderBufferOddSize =
            scene_rdl2::grid_util::PackTiles::
            encode(true,
                   mFbActivePixels.getActivePixelsRenderBufferOdd(),
                   mFb.getRenderBufferOddTiled(),
                   mWork,
                   packTilePrecision,
                   mFb.getRenderBufferCoarsePassPrecision(), // dummy value
                   mFb.getRenderBufferFinePassPrecision(), // dummy value
                   sha1HashSw); // RGBA : float * 4
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_RENDERBUFFERODD);

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastRenderBufferOddSize,
                       scene_rdl2::grid_util::ProgressiveFrameBufferName::RenderBufferOdd,
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_RENDERBUFFERODD);
    mLatencyLog.addDataSize(mLastRenderBufferOddSize);
}

void
MergeFbSender::addRenderBufferOddWithNumSample(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_RENDERBUFFERODD_NUMSAMPLE);
    {
        PackTilePrecision packTilePrecision =
            calcPackTilePrecision(mFb.getRenderBufferCoarsePassPrecision(),
                                  mFb.getRenderBufferFinePassPrecision(),
                                  [&]() -> PackTilePrecision { // runtimeDecisionFunc for coarse pass
                                      return getBeautyHDRITestResult(); // access shared beauty HDRI test result
                                  });
        mWork.clear();
        // Actually we don't have {coarse, fine}PassPrecision info for renderBufferOdd.
        mLastRenderBufferOddNumSampleSize =
            scene_rdl2::grid_util::PackTiles::
            encode(true, // renderBufferOdd
                   mFbActivePixels.getActivePixelsRenderBufferOdd(),
                   mFb.getRenderBufferOddTiled(),
                   mFb.getWeightBufferTiled(),
                   mWork,
                   packTilePrecision,
                   mFb.getRenderBufferCoarsePassPrecision(), // dummy value
                   mFb.getRenderBufferFinePassPrecision(), // dummy value
                   false, // noNumSampleMode
                   sha1HashSw); // RGBA : float * 4
    }
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_RENDERBUFFERODD_NUMSAMPLE);

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       mLastRenderBufferOddNumSampleSize,
                       scene_rdl2::grid_util::ProgressiveFrameBufferName::RenderBufferOdd,
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_RENDERBUFFERODD_NUMSAMPLE);
    mLatencyLog.addDataSize(mLastRenderBufferOddNumSampleSize);
}

void    
MergeFbSender::addRenderOutput(mcrt::BaseFrame::Ptr message)
{
    static const bool sha1HashSw = false;

    mLastRenderOutputSize = 0;

    mFbActivePixels.activeRenderOutputCrawler
        ([&](const std::string &aovName, const scene_rdl2::fb_util::ActivePixels &activePixels) {
            if (!mFb.findAov(aovName)) return;
            scene_rdl2::grid_util::Fb::FbAovShPtr fbAov = mFb.getAov(aovName);

            if (!fbAov->getStatus()) return; // just in case

            size_t dataSize = 0;
            mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_START_RENDEROUTPUT);
            {
                mWork.clear();
                if (fbAov->getReferenceType() == scene_rdl2::grid_util::FbReferenceType::UNDEF) {
                    // regular AOV buffer
                    PackTilePrecision packTilePrecision =
                        calcPackTilePrecision(fbAov->getCoarsePassPrecision(),
                                              fbAov->getFinePassPrecision(),
                                              [&]() -> PackTilePrecision { // coarsePass runtimeDecisionFunc
                                                  if (renderOutputHDRITest(fbAov)) {
                                                      return PackTilePrecision::H16;
                                                  } else {
                                                      return PackTilePrecision::UC8;
                                                  }
                                              });
                    dataSize =
                        scene_rdl2::grid_util::PackTiles::
                        encodeRenderOutputMerge(activePixels,
                                                fbAov->getBufferTiled(),
                                                fbAov->getDefaultValue(),
                                                mWork,
                                                packTilePrecision,
                                                fbAov->getClosestFilterStatus(),
                                                fbAov->getCoarsePassPrecision(),
                                                fbAov->getFinePassPrecision(),
                                                sha1HashSw);
                } else {
                    // reference type AOV buffer
                    dataSize =
                        scene_rdl2::grid_util::PackTiles::
                        encodeRenderOutputReference(fbAov->getReferenceType(),
                                                    mWork,
                                                    sha1HashSw);
                }
            }
            mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ENCODE_END_RENDEROUTPUT);

            // for performance analyze
            mLastRenderOutputSize += dataSize;

            message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                               dataSize,
                               fbAov->getAovName().c_str(),
                               mcrt::BaseFrame::ENCODING_UNKNOWN);
            mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_ADDBUFFER_END_RENDEROUTPUT);
            mLatencyLog.addDataSize(dataSize);
        });
}

void    
MergeFbSender::addRenderOutputWithNumSample(mcrt::BaseFrame::Ptr message)
{
    //
    // We don't need this API at this moment but might be needed in the future when we need to send
    // back AOV info from Merge to MCRT computation by progressiveFeedback message for some reason.
    // This API is reserved for that purpose.
    //
}

void
MergeFbSender::addLatencyLog(mcrt::BaseFrame::Ptr message)
{
    mLatencyLog.setName("merge");
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_SEND_MSG);

    {
        size_t dataSize = mLastBeautyBufferSize + mLastBeautyBufferNumSampleSize;
        if (mFb.getPixelInfoStatus()) {
            dataSize += mLastPixelInfoSize;
        }
        if (mFb.getHeatMapStatus()) {
            dataSize += mLastHeatMapSize;
        }  
        if (mFb.getWeightBufferStatus()) {
            dataSize += mLastWeightBufferSize;
        }
        if (mFb.getRenderBufferOddStatus()) {
            dataSize += mLastRenderBufferOddSize + mLastRenderBufferOddNumSampleSize;
        }
        if (mFb.getRenderOutputStatus()) {
            dataSize += mLastRenderOutputSize;
        }

        bool dispST = false;
        if (message->getStatus() == mcrt::BaseFrame::STARTED) {
            mMin = dataSize;
            mMax = dataSize;
            dispST = true;
        } else {
            if (dataSize < mMin) { mMin = dataSize; dispST = true; }
            if (mMax < dataSize) { mMax = dataSize; dispST = true; }
        }
        if (dispST) {
#           ifdef DEBUG_MSG
            std::cerr << ">> MergeFbSender.cc merger message size min:" << mMin << " max:" << mMax
                      << std::endl;
#           endif // end DEBUG_MSG
        }
    }

    {
        mWork.clear();              // We have to clear work
        scene_rdl2::rdl2::ValueContainerEnq vContainerEnq(&mWork);

        mLatencyLog.encode(vContainerEnq);

        size_t dataSize = vContainerEnq.finalize();

        /* useful test dump for debug
        std::cerr << ">> progmcrt_merge MergeFbSender.cc dataSize:" << dataSize << std::endl;
        {
            scene_rdl2::grid_util::LatencyLog tmpLog;

            scene_rdl2::rdl2::ValueContainerDeq vContainerDeq(mWork.data(), dataSize);
            tmpLog.decode(vContainerDeq);
            std::cerr << ">> progmcrt_merge MergeFbSender.cc dataSize:" << dataSize << " {\n"
                      << tmpLog.show("  ") << '\n'
                      << "}" << std::endl;
        }
        */
        message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                           dataSize,
                           scene_rdl2::grid_util::ProgressiveFrameBufferName::LatencyLog,
                           mcrt::BaseFrame::ENCODING_UNKNOWN);
    }

    //------------------------------
    //
    // upstream latency log
    //
    if (mUpstreamLatencyLogWork.size()) {
        message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mUpstreamLatencyLogWork)),
                           mUpstreamLatencyLogWork.size(),
                           scene_rdl2::grid_util::ProgressiveFrameBufferName::LatencyLogUpstream,
                           mcrt::BaseFrame::ENCODING_UNKNOWN);
    }
}

void
MergeFbSender::addAuxInfo(mcrt::BaseFrame::Ptr message,
                          const std::vector<std::string> &infoDataArray)
{
    mWork.clear();              // We have to clear work buffer
    scene_rdl2::rdl2::ValueContainerEnq cEnq(&mWork);

    cEnq.enqStringVector(infoDataArray);
    size_t dataSize = cEnq.finalize();

    message->addBuffer(mcrt::makeValPtr(duplicateWorkData(mWork)),
                       dataSize,
                       scene_rdl2::grid_util::ProgressiveFrameBufferName::AuxInfo,                       
                       mcrt::BaseFrame::ENCODING_UNKNOWN);
}

void
MergeFbSender::fbReset()
//
// only reset fb related information
//    
{
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_FBRESET_START);
    mFb.reset();
    mLatencyLog.enq(scene_rdl2::grid_util::LatencyItem::Key::MERGE_FBRESET_END);
}

MergeFbSender::PackTilePrecision
MergeFbSender::getBeautyHDRITestResult()
//
// Return beautyHDRI test result with test result cache logic in order to avoid duplicate
// execution of testing itself.
//    
{
    auto getResult = [&]() -> scene_rdl2::grid_util::PackTiles::PrecisionMode {
        return ((mBeautyHDRITest == HdriTestCondition::HDRI) ?
                scene_rdl2::grid_util::PackTiles::PrecisionMode::H16 :
                scene_rdl2::grid_util::PackTiles::PrecisionMode::UC8);
    };

    if (mBeautyHDRITest != HdriTestCondition::INIT) return getResult();

    mBeautyHDRITest = (beautyHDRITest()) ? HdriTestCondition::HDRI : HdriTestCondition::NON_HDRI;
    return getResult();
}

bool
MergeFbSender::beautyHDRITest() const
//
// HDR pixel existence test for beauty buffer
//
{
    size_t area = mFb.getAlignedWidth() * mFb.getAlignedHeight();
    // We want to use minLimit in order to ignore small number of HDRI pixels like firefly
    // Use experimental number here. This cost was around 0.5ms~2ms range in my HD reso test scene.
    // Future enhancement related ticket is MOONRAY-3588
    size_t minLimit = (size_t)((float)area * 0.005f); // 0.5% of whole pixels
    const scene_rdl2::math::Vec4f *c = mFb.getRenderBufferTiled().getData();

    size_t totalHDRI = 0;
    for (size_t i = 0; i < area; ++i, ++c) {
        if (c->x > 1.0f || c->y > 1.0f || c->z > 1.0f || c->w > 1.0f) {
            if (totalHDRI > minLimit) {
                return true;
            }
            totalHDRI++;
        }
    }

    return false;
}

bool
MergeFbSender::renderOutputHDRITest(const scene_rdl2::grid_util::Fb::FbAovShPtr fbAov) const
//
// HDR pixel existence test for renderOutput AOV buffer.
// The idea is the same as beautyHDRITest() and we are not testing whole pixels.
//    
{
    const scene_rdl2::grid_util::FbAov::ActivePixels &activePixels = fbAov->getActivePixels();
    const scene_rdl2::grid_util::FbAov::VariablePixelBuffer &buff = fbAov->getBufferTiled();
    const scene_rdl2::grid_util::FbAov::NumSampleBuffer &numSampleBuff = fbAov->getNumSampleBufferTiled();

    if (buff.getFormat() == scene_rdl2::fb_util::VariablePixelBuffer::Format::RGB888 ||
        buff.getFormat() == scene_rdl2::fb_util::VariablePixelBuffer::Format::RGBA8888) {
        return false; // uc8 based frame buffer
    }
    if (buff.getFormat() != scene_rdl2::fb_util::VariablePixelBuffer::Format::FLOAT &&
        buff.getFormat() != scene_rdl2::fb_util::VariablePixelBuffer::Format::FLOAT2 &&
        buff.getFormat() != scene_rdl2::fb_util::VariablePixelBuffer::Format::FLOAT3 &&
        buff.getFormat() != scene_rdl2::fb_util::VariablePixelBuffer::Format::FLOAT4) {
        return true; // We can not apply HDRI test here. return true as non 8bit range data
    }

    size_t area = activePixels.getAlignedWidth() * activePixels.getAlignedHeight();
    size_t minLimit = static_cast<size_t>((float)area * 0.005f); // 0.5% of whole pixels
    unsigned pixByte = buff.getSizeOfPixel();
    size_t pixFloatCount = pixByte / sizeof(float);
    const float *p = reinterpret_cast<const float *>(buff.getData());
    const unsigned int *ns = numSampleBuff.getData();

    size_t totalHDRI = 0;
    for (size_t i = 0; i < area; ++i, ++ns) {
        if (*ns > 0) {
            float max = static_cast<float>(*ns);
            int currTotalHDRIchan = 0;
            for (size_t j = 0; j < pixFloatCount ; ++j) {
                if (p[j] > max) currTotalHDRIchan++;
            }
            p += pixFloatCount;
            if (currTotalHDRIchan > 0) {
                if (totalHDRI > minLimit) {
                    return true; // HDRI fb
                }
                totalHDRI++;
            }
        }
    }
    return false; // non HDRI fb
}

MergeFbSender::PackTilePrecision
MergeFbSender::calcPackTilePrecision(const CoarsePassPrecision coarsePassPrecision,
                                     const FinePassPrecision finePassPrecision,
                                     PackTilePrecisionCalcFunc runtimeDecisionFunc) const
//
// This function should be called after call setHeaderInfoAndFbReset().
//
// setHeaderInfoAndFbReset() execute 2 things which related this function
//  a) set mCoarsePassStatus
//  b) reset mBeautyHDRITest as INIT => might need by precisionCalcFunc()
//     getBeautyHDRITestResult() is using mBeautyHDRITest condition and getBeautyHDRITestResult() is
//     called from precisionCalcFunc().
//
{
    auto calcCoarsePassPrecision = [&]() -> PackTilePrecision {
        PackTilePrecision precision = PackTilePrecision::F32;
        switch (coarsePassPrecision) {
        case CoarsePassPrecision::F32 : precision = PackTilePrecision::F32; break;
        case CoarsePassPrecision::H16 : precision = PackTilePrecision::H16; break;
        case CoarsePassPrecision::UC8 : precision = PackTilePrecision::UC8; break;
        case CoarsePassPrecision::RUNTIME_DECISION :
            if (runtimeDecisionFunc) precision = runtimeDecisionFunc();
            break;
        default : break;
        }
        return precision;
    };
    auto calcFinePassPrecision = [&]() -> PackTilePrecision {
        PackTilePrecision precision = PackTilePrecision::F32;
        switch (finePassPrecision) {
        case FinePassPrecision::F32 : precision = PackTilePrecision::F32; break;
        case FinePassPrecision::H16 : precision = PackTilePrecision::H16; break;
        }
        return precision;
    };

    PackTilePrecision precision = PackTilePrecision::F32;

    switch (mPrecisionControl) {
    case PrecisionControl::FULL32 :
        precision = PackTilePrecision::F32; // Always uses F32
        break;
    case PrecisionControl::FULL16 :
        if (mCoarsePassStatus) {
            if (coarsePassPrecision == CoarsePassPrecision::F32) {
                precision = PackTilePrecision::F32; // This data is not able to use H16
            } else {
                precision = PackTilePrecision::H16; // Use H16 if possible
            }
        } else {
            if (finePassPrecision == FinePassPrecision::F32) {
                precision = PackTilePrecision::F32; // This data is not able to use H16
            } else {
                precision = PackTilePrecision::H16; // Use H16 if possible
            }
        }
        break;
    case PrecisionControl::AUTO32 :
        if (mCoarsePassStatus) {
            precision = calcCoarsePassPrecision(); // respect coarse pass precision decision
        } else {
            precision = PackTilePrecision::F32;    // always use F32
        }
        break;
    case PrecisionControl::AUTO16 :
        if (mCoarsePassStatus) {
            precision = calcCoarsePassPrecision(); // respect coarse pass precision decision
        } else {
            precision = calcFinePassPrecision();   // respect fine pass precision decision
        }
        break;
    default : break;
    }

    return precision;
}

} // namespace mcrt_dataio
