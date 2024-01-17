// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "FbMsgMultiChans.h"
#include "GlobalNodeInfo.h"
#include "MergeActionTracker.h"

#include <scene_rdl2/common/fb_util/ActivePixels.h>
#include <scene_rdl2/common/grid_util/PackTiles.h>
#include <scene_rdl2/scene/rdl2/ValueContainerDeq.h>
#include <scene_rdl2/scene/rdl2/ValueContainerEnq.h>

#include <iomanip>
#include <sstream>

//#define DEBUG_DECODE_MSG // debug message for decode action

namespace mcrt_dataio {

bool
FbMsgMultiChans::push(const bool delayDecode,
                      const mcrt::ProgressiveFrame &progressive, scene_rdl2::grid_util::Fb &fb,
                      const bool parallelExec,
                      const bool skipLatencyLog)
{
    if (progressive.getProgress() < 0.0f) {
        // just in case. We skip info only message. Info only message is already processed
        // before this function
        return true;
    }

    /* useful debug message
    std::cerr << "FbMsgMultiChans.cc FbMsgMultiChans::push() rezed viewport "
              << "(" << progressive.getRezedViewport().mMinX << ',' << progressive.getRezedViewport().mMinY << ")-"
              << "(" << progressive.getRezedViewport().mMaxX << ',' << progressive.getRezedViewport().mMaxY << ")\n";
    */

    if (progressive.mSendImageActionId != ~static_cast<unsigned>(0)) {
        mSendImageActionIdData.push_back(progressive.mSendImageActionId);
    }
    mProgress = progressive.getProgress();

    mStatus = progressive.getStatus();
    if (mStatus == mcrt::BaseFrame::STARTED) {
        // std::cerr << ">> FbMsgMultiChans.cc FbMsgMultiChans::push() STARTED fb.reset\n"; // useful debug message
        fb.reset();
        reset();
        mHasStartedStatus = true;
    }

    if (progressive.mCoarsePassStatus == 1) { mCoarsePass = false; }

    if (progressive.hasViewport()) {
        mRoiViewportStatus = true;
        mRoiViewport = scene_rdl2::math::Viewport(progressive.getViewport().minX(),
                                             progressive.getViewport().minY(),
                                             progressive.getViewport().maxX(),
                                             progressive.getViewport().maxY());
    } else {
        mRoiViewportStatus = false;
    }

    if (mSnapshotStartTime == 0) {
        // Keep snapshotStartTime of 1st ProgressiveFrame as snapshot start time
        mSnapshotStartTime = progressive.mSnapshotStartTime;
    }

    //
    // We have to consider 2 different environment.
    // a) Merge action processing at Merge computation
    //    This should be always executed by MT.
    // b) ProgressiveFeedback processing at MCRT computation.
    //    This is probably not using MT (depending on the situation). We want to use CPU resources for pixel
    //    computation itself more but we also consider MT run depending on feedback processing cost.
    //        
    if (!parallelExec) {
        for (const mcrt::BaseFrame::DataBuffer &buffer: progressive.mBuffers) {
            if (!pushBuffer(delayDecode,
                            skipLatencyLog,
                            buffer.mName,
                            buffer.mData,
                            buffer.mDataLength,
                            fb)) {
                return false;       // error
            }
        }
    } else {
        if (delayDecode) {
            // This is single thread execution. Under delayDecode mode,
            // We don't need to run by multi-thread because we only copy shared_ptr.
            // Single thread is enough.
            for (const mcrt::BaseFrame::DataBuffer &buffer: progressive.mBuffers) {
                if (!pushBuffer(delayDecode,
                                skipLatencyLog,
                                buffer.mName,
                                buffer.mData,
                                buffer.mDataLength,
                                fb)) {
                    return false;       // error
                }
            }
        } else {
            // This is non-delayDecode mode, we have to decode everything here and
            // it requires multi-thread execution.
            if (!progressive.mBuffers.empty()) {
                std::vector<const mcrt::BaseFrame::DataBuffer *> bufferArray(progressive.mBuffers.size());
                for (size_t id = 0; id < progressive.mBuffers.size(); ++id) {
                    bufferArray[id] = &progressive.mBuffers[id];
                }
                bool errorST = false;
                tbb::blocked_range<size_t> range(0, bufferArray.size());
                tbb::parallel_for(range, [&](const tbb::blocked_range<size_t> &r) {
                        for (size_t id = r.begin(); id < r.end(); ++id) {
                            if (!pushBuffer(delayDecode,
                                            skipLatencyLog,
                                            bufferArray[id]->mName,
                                            bufferArray[id]->mData,
                                            bufferArray[id]->mDataLength,
                                            fb)) {
                                errorST = true;
                            }
                        }
                    });
                if (errorST) return false;
            }
        }
    }

    return true;
}

void
FbMsgMultiChans::encodeLatencyLog(scene_rdl2::rdl2::ValueContainerEnq &vContainerEnq)
{
    bool sw = (mMsgArray.find(latencyLogName) != mMsgArray.end())? true: false;

    vContainerEnq.enqBool(sw);
    if (sw) {
        mMsgArray[latencyLogName]->encode(vContainerEnq);
    }
}

std::string
FbMsgMultiChans::show(const std::string &hd) const
{
    std::ostringstream ostr;
    ostr << hd << "FbMsg {\n";
    ostr << hd << "  mProgress:" << mProgress << '\n';
    ostr << hd << "  mHasBeauty      :" << ((mHasBeauty      )? "true ": "false") << '\n';
    ostr << hd << "  mHasPixelInfo   :" << ((mHasPixelInfo   )? "true ": "false") << '\n';
    ostr << hd << "  mHasRenderOutput:" << ((mHasRenderOutput)? "true ": "false") << '\n';
    ostr << hd << "  mCoarsePass     :" << ((mCoarsePass     )? "true ": "false") << '\n';
    for (auto &&itr : mMsgArray) {
        ostr << hd << "  name:" << itr.first << " {\n";
        ostr << (itr.second)->show(hd + "    ") << '\n';
        ostr << hd << "  }\n";
    }
    ostr << hd << "}";
    return ostr.str();
}

std::string
FbMsgMultiChans::show() const
{
    using scene_rdl2::str_util::boolStr;
    using scene_rdl2::str_util::addIndent;

    auto showSendImageActionIdData = [&]() -> std::string {
        std::ostringstream ostr;
        ostr << "sendImageActionIdData (size:" << mSendImageActionIdData.size() << ") {\n";
        int w0 = scene_rdl2::str_util::getNumberOfDigits(mSendImageActionIdData.size());
        int w1 = scene_rdl2::str_util::getNumberOfDigits(mSendImageActionIdData.back());
        for (size_t i = 0; i < mSendImageActionIdData.size(); ++i) {
            ostr << "  id:" << std::setw(w0) << i
                 << " sendImageActionId:" << std::setw(w1) << mSendImageActionIdData[i] << '\n';
        }
        ostr << "}";
        return ostr.str();
    };
    auto showBaseFrameStatus = [](mcrt::BaseFrame::Status stats) -> std::string {
        switch (stats) {
        case mcrt::BaseFrame::Status::STARTED : return "STARTED";
        case mcrt::BaseFrame::Status::RENDERING : return "RENDERING";
        case mcrt::BaseFrame::Status::FINISHED : return "FINISHED";
        case mcrt::BaseFrame::Status::CANCELLED : return "CANCELLED";
        case mcrt::BaseFrame::Status::ERROR : return "ERROR";
        default : return "?";
        }
    };
    auto showViewport = [](const scene_rdl2::math::Viewport& viewport) -> std::string {
        std::ostringstream ostr;
        ostr
        << "(" << viewport.mMinX << ',' << viewport.mMinY << ")-"
        << "(" << viewport.mMaxX << ',' << viewport.mMaxY << ")";
        return ostr.str();
    };
    auto showMsgArray = [&]() -> std::string {
        std::ostringstream ostr;
        ostr << "mMsgArray size:" << mMsgArray.size() << " {\n";
        for (auto &&itr : mMsgArray) {
            ostr << "  name:" << itr.first << " {\n"
                 << addIndent((itr.second)->show(), 2) << '\n'
                 << "  }\n";
        }
        ostr << "}";
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "status {\n"
         << scene_rdl2::str_util::addIndent(showSendImageActionIdData()) << '\n'
         << "  mProgress:" << mProgress << '\n'
         << "  mStatus:" << showBaseFrameStatus(mStatus) << '\n'
         << "  mHasStartedStatus:" << boolStr(mHasStartedStatus) << '\n'
         << "  mCoarsePass:" << boolStr(mCoarsePass) << '\n'
         << "  mHasBeauty:" << boolStr(mHasBeauty) << " (valid by decodeData())\n"
         << "  mHasPixelInfo:" << boolStr(mHasPixelInfo) << " (valid by decodeData())\n"
         << "  mHasHeatMap:" << boolStr(mHasHeatMap) << " (valid by decodeData())\n"
         << "  mHasRenderBufferOdd:" << boolStr(mHasRenderBufferOdd) << " (valid by decodeData())\n"
         << "  mHasRenderOutput:" << boolStr(mHasRenderOutput) << " (valid by decodeData())\n"
         << "  mRoiViewportStatus:" << boolStr(mRoiViewportStatus) << '\n'
         << "  mRoiViewport:" << showViewport(mRoiViewport) << '\n'
         << "  mSnapshotStartTime:" << mSnapshotStartTime << '\n'
         << addIndent(showMsgArray()) << '\n'
         << "}";
    return ostr.str();
}

//-------------------------------------------------------------------------------------------------------------

bool
FbMsgMultiChans::pushBuffer(const bool delayDecode,
                            const bool skipLatencyLog,
                            const char *name,
                            DataPtr dataPtr,
                            const size_t dataSize,
                            scene_rdl2::grid_util::Fb &fb)
//
// fb internal information is accumulatively updated (and not initialized every call).
// fb is initialized (resized) internally based on message if needed (like resize situation).
//
{
    if (skipLatencyLog && (!strcmp(name, latencyLogName) || !strcmp(name, latencyLogUpstreamName))) {
        // Special mode for image feedback logic. Skip all latencyLog and latencyLogUpstream data.
        return true;
    }

    if (!strcmp(name, auxInfoName)) {
        pushAuxInfo(dataPtr.get(), dataSize);
        return true;
    }

    if (delayDecode || !strcmp(name, latencyLogName)) {
        //
        // delayDecode mode or latencyLog information
        //
        if (mMsgArray.find(name) == mMsgArray.end()) {
            try {
                mMsgArray[name] = FbMsgSingleChanShPtr(new FbMsgSingleChan);
            }
            catch (...) {
                return false;
            }
        }
        /* useful test dump
        std::cerr << ">> FbMsgMultiChans.cc latencyLog {\n"
                  << mMsgArray[latencyLogName]->showLatencyLog("  ") << '\n'
                  << "}" << std::endl;
        */
        return mMsgArray[name]->push(dataPtr, dataSize);
    }

    decodeData(name, dataPtr.get(), dataSize, fb);
    
    return true;
}

void
FbMsgMultiChans::pushAuxInfo(const void *data, const size_t dataSize)
{
    if (!mGlobalNodeInfo) return;
    
    scene_rdl2::rdl2::ValueContainerDeq cDeq(data, dataSize);
    std::vector<std::string> infoDataArray = cDeq.deqStringVector();
    mGlobalNodeInfo->decode(infoDataArray);
}

void
FbMsgMultiChans::decodeAll(scene_rdl2::grid_util::Fb& fb, MergeActionTracker* mergeActionTracker)
{
#   ifdef SINGLE_THREAD
    auto itr = mMsgArray.cbegin();
    while (1) {
        if (itr == mMsgArray.cend()) break; // end of loop

        const std::string &name = itr->first;
        if (!strcmp(name.c_str(), latencyLogName)) {
            itr++;              // decode skip latencyLog data
        } else {
            const std::vector<DataPtr>& datas = (itr->second)->dataArray();
            const std::vector<size_t>& dataSize = (itr->second)->dataSize();
            for (size_t i = 0; i < datas.size(); ++i) {
                decodeData(name.c_str(), datas[i].get(), dataSize[i], fb);
            }
            itr = mMsgArray.erase(itr); // remove data
        }
    }
#   else // else SINGLE_THREAD
    //
    // each AOV data is decoded in parallel
    //
    std::vector<const std::string *> ptrSingleChanNameArray;
    std::vector<FbMsgSingleChan *> ptrSingleChanArray;
    auto itr = mMsgArray.cbegin();
    while (1) {
        if (itr == mMsgArray.cend()) break;
        const std::string &name = itr->first;
        if (strcmp(name.c_str(), latencyLogName)) {
            // non latencyLog case, we should push info
            ptrSingleChanNameArray.push_back(&(itr->first));
            ptrSingleChanArray.push_back((itr->second).get());
        }
        itr++;
    }
    tbb::blocked_range<size_t> range(0, ptrSingleChanArray.size());
    tbb::parallel_for(range, [&](const tbb::blocked_range<size_t> &r) {
            for (size_t id = r.begin(); id < r.end(); ++id) {
                const std::string &name = *ptrSingleChanNameArray[id];
                const std::vector<DataPtr> &datas = ptrSingleChanArray[id]->dataArray();
                const std::vector<size_t> &dataSize = ptrSingleChanArray[id]->dataSize();
                for (size_t i = 0; i < datas.size(); ++i) {
                    decodeData(name.c_str(), datas[i].get(), dataSize[i], fb);
                }
            } // id
        });
    itr = mMsgArray.cbegin();
    while (1) {
        if (itr == mMsgArray.cend()) break;
        const std::string &name = itr->first;
        if (!strcmp(name.c_str(), latencyLogName)) {
            itr++;              // skip latencyLog data
        } else {
            itr = mMsgArray.erase(itr); // remove data
        }
    }
#   endif // end else SINGLE_THREAD     

    //------------------------------
    //
    // Update mergeActionTracker
    //
    if (mergeActionTracker) {
        mergeActionTracker->decodeAll(mSendImageActionIdData);
    }
    mSendImageActionIdData.clear();
}

void    
FbMsgMultiChans::decodeData(const char* name,
                            const void* data,
                            const size_t dataSize,
                            scene_rdl2::grid_util::Fb& fb)
{
    // scene_rdl2::grid_util::PackTiles::debugMode(true); // for PackTiles debug

    //
    // decode data
    //
    switch (scene_rdl2::grid_util::PackTiles::decodeDataType(data, dataSize)) {
    case scene_rdl2::grid_util::PackTiles::DataType::BEAUTY_WITH_NUMSAMPLE :
        // beauty with numSample
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passA\n";
#       endif // end DEBUG_DECODE_MSG
        decodeBeautyWithNumSample(data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::BEAUTY :
        // beauty only (not include numSample)
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passB\n";
#       endif // end DEBUG_DECODE_MSG
        decodeBeauty(data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::BEAUTYODD_WITH_NUMSAMPLE :
        // beautyOdd with numSample
        // Actually we do not have {coarse,Fine}PassPrecision info for renderBufferOdd.
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passC\n";
#       endif // end DEBUG_DECODE_MSG
        decodeBeautyOddWithNumSample(data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::BEAUTYODD :
        // beautyOdd only (not include numSample)
        // Actually we do not have {coarse,Fine}PassPrecision info for renderBufferOdd.
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passD\n";
#       endif // end DEBUG_DECODE_MSG
        decodeBeautyOdd(data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::PIXELINFO :
        // pixelInfo
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passE\n";
#       endif // end DEBUG_DECODE_MSG
        decodePixelInfo(name, data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::HEATMAP_WITH_NUMSAMPLE :
        // heatMap with numSample
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passF\n";
#       endif // end DEBUG_DECODE_MSG
        decodeHeatMapWithNumSample(name, data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::HEATMAP :
        // heatMap only (not include numSample)
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passG\n";
#       endif // end DEBUG_DECODE_MSG
        decodeHeatMap(name, data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::WEIGHT :
        // weight buffer
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passH\n";
#       endif // end DEBUG_DECODE_MSG
        decodeWeight(name, data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::REFERENCE :
        // renderOutput reference AOVs (Beauty, Alpha, HeatMap, Weight, BeautyAux, AlphaAux)
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passI\n";
#       endif // end DEBUG_DECODE_MSG
        decodeReference(name, data, dataSize, fb);
        break;
    case scene_rdl2::grid_util::PackTiles::DataType::UNDEF :
        // skip unknown data type
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passJ\n";
#       endif // end DEBUG_DECODE_MSG
        break;
    default :
        // renderOutput AOVs
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeData() passK\n";
#       endif // end DEBUG_DECODE_MSG
        decodeRenderOutputAOV(name, data, dataSize, fb);
        break;
    }
    scene_rdl2::grid_util::PackTiles::debugMode(false);
#   ifdef DEBUG_DECODE_MSG
    std::cerr << ">> FbMsgMultiChans.cc decodeData() finish\n";
#   endif // end DEBUG_DECODE_MSG
}

void
FbMsgMultiChans::decodeBeautyWithNumSample(const void* data,
                                           const size_t dataSize,
                                           scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};

    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyWithNumSample() before PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decode(false, // renderBufferOdd = false
                   data,
                   dataSize,
                   true, // storeNumSampleData = true
                   workActivePixels,
                   fb.getRenderBufferTiled(), // normalized color
                   fb.getNumSampleBufferTiled(),
                   fb.getRenderBufferCoarsePassPrecision(),
                   fb.getRenderBufferFinePassPrecision(),
                   activeDecodeAction)) {
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyWithNumSample() after PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyWithNumSample() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data

    if (!workActivePixels.isSameSize(fb.getActivePixels())) {
        // resolution changed, we should pick current workActivePixels
        fb.getActivePixels().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        (void)fb.getActivePixels().orOp(workActivePixels);
    }
    mHasBeauty = true;
}

void
FbMsgMultiChans::decodeBeauty(const void* data,
                              const size_t dataSize,
                              scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};

    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeauty() before PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decode(false, // renderBufferOdd = false
                   data,
                   dataSize,
                   workActivePixels,
                   fb.getRenderBufferTiled(), // RGBA : float * 4
                   fb.getRenderBufferCoarsePassPrecision(),
                   fb.getRenderBufferFinePassPrecision(),
                   activeDecodeAction)) {
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeauty() after PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeBeauty() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data
    
    if (!workActivePixels.isSameSize(fb.getActivePixels())) {
        // resolution changed, we should pick current workActivePixels
        fb.getActivePixels().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        fb.getActivePixels().orOp(workActivePixels);
    }
}

void
FbMsgMultiChans::decodeBeautyOddWithNumSample(const void* data,
                                              const size_t dataSize,
                                              scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};
    
    scene_rdl2::grid_util::CoarsePassPrecision dummyCoarsePassPrecision;
    scene_rdl2::grid_util::FinePassPrecision dummyFinePassPrecision;
    fb.setupRenderBufferOdd(nullptr);
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyOddWithNumSample() before PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decode(true, // renderBufferOdd
                   data,
                   dataSize,
                   true, // storeNumSampleData
                   workActivePixels,
                   fb.getRenderBufferOddTiled(), // RGBA : float * 4 : normalized color
                   fb.getRenderBufferOddNumSampleBufferTiled(),
                   dummyCoarsePassPrecision,
                   dummyFinePassPrecision,
                   activeDecodeAction)) {
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyOddWithNumSample() after PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyOddWithNumSample() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data

    if (!workActivePixels.isSameSize(fb.getActivePixelsRenderBufferOdd())) {
        // resolution changed, we should pick current workActivePixels.
        fb.getActivePixelsRenderBufferOdd().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        (void)fb.getActivePixelsRenderBufferOdd().orOp(workActivePixels);
    }
    mHasRenderBufferOdd = true;
}

void
FbMsgMultiChans::decodeBeautyOdd(const void* data,
                                 const size_t dataSize,
                                 scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};
    
    scene_rdl2::grid_util::CoarsePassPrecision dummyCoarsePassPrecision;
    scene_rdl2::grid_util::FinePassPrecision dummyFinePassPrecision;
    fb.setupRenderBufferOdd(nullptr);
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyOdd() before PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decode(true, // renderBufferOdd
                   data,
                   dataSize,
                   workActivePixels,
                   fb.getRenderBufferOddTiled(), // RGBA : float * 4 : normalized color
                   dummyCoarsePassPrecision,
                   dummyFinePassPrecision,
                   activeDecodeAction)) {
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyOdd() after PackTiles::decode()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeBeautyOdd() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data

    if (!workActivePixels.isSameSize(fb.getActivePixelsRenderBufferOdd())) {
        // resolution changed, we should pick current workActivePixels.
        fb.getActivePixelsRenderBufferOdd().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        (void)fb.getActivePixelsRenderBufferOdd().orOp(workActivePixels);
    }
    mHasRenderBufferOdd = true;
}

void
FbMsgMultiChans::decodePixelInfo(const char* name,
                                 const void *data,
                                 const size_t dataSize,
                                 scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};
    
    fb.setupPixelInfo(nullptr, name);
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodePixelInfo() before PackTiles::decodePixelInfo()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decodePixelInfo(data,
                            dataSize,
                            workActivePixels,
                            fb.getPixelInfoBufferTiled(),
                            fb.getPixelInfoCoarsePassPrecision(),
                            fb.getPixelInfoFinePassPrecision(),
                            activeDecodeAction)) {
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodePixelInfo() after PackTiles::decodePixelInfo()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodePixelInfo() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data

    if (!workActivePixels.isSameSize(fb.getActivePixelsPixelInfo())) {
        // resolution changed, we should pick current workActivePixels.
        fb.getActivePixelsPixelInfo().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        (void)fb.getActivePixelsPixelInfo().orOp(workActivePixels);
    }
    mHasPixelInfo = true;
}
    
void
FbMsgMultiChans::decodeHeatMapWithNumSample(const char* name,
                                            const void *data,
                                            const size_t dataSize,
                                            scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};

    fb.setupHeatMap(nullptr, name);
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeHeatMapWithNumSample() before PackTiles::decodeHeatMap()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decodeHeatMap(data,
                          dataSize,
                          true, // storeNumSampleData
                          workActivePixels,
                          fb.getHeatMapSecBufferTiled(),
                          fb.getHeatMapNumSampleBufferTiled(),
                          activeDecodeAction)) {
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeHeatMapWithNumSample() after PackTiles::decodeHeatMap()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeHeatMapWithNumSample() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data

    if (!workActivePixels.isSameSize(fb.getActivePixelsHeatMap())) {
        // resolution changed, we should pick current workActivePixels.
        fb.getActivePixelsHeatMap().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        (void)fb.getActivePixelsHeatMap().orOp(workActivePixels);
    }
    mHasHeatMap = true;
}

void
FbMsgMultiChans::decodeHeatMap(const char* name,
                               const void *data,
                               const size_t dataSize,
                               scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};

    fb.setupHeatMap(nullptr, name);
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeHeatMap() before PackTiles::decodeHeatMap()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decodeHeatMap(data,
                          dataSize,
                          workActivePixels,
                          fb.getHeatMapSecBufferTiled(),
                          activeDecodeAction)) { // Sec : float
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeHeatMap() after PackTiles::decodeHeatMap()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeHeatMap() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data

    if (!workActivePixels.isSameSize(fb.getActivePixelsHeatMap())) {
        // resolution changed, we should pick current workActivePixels.
        fb.getActivePixelsHeatMap().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        (void)fb.getActivePixelsHeatMap().orOp(workActivePixels);
    }
    mHasHeatMap = true;
}
    
void
FbMsgMultiChans::decodeWeight(const char* name,
                              const void *data,
                              const size_t dataSize,
                              scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};

    fb.setupWeightBuffer(nullptr, name);
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeWeight() before PackTiles::decodeWeightBuffer()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decodeWeightBuffer(data,
                               dataSize,
                               workActivePixels,
                               fb.getWeightBufferTiled(),
                               fb.getWeightBufferCoarsePassPrecision(),
                               fb.getWeightBufferFinePassPrecision(),
                               activeDecodeAction)) {
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeWeight() after PackTiles::decodeWeightBuffer()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeWeight() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return; // no decoded data

    if (!workActivePixels.isSameSize(fb.getActivePixelsWeightBuffer())) {
        // resolution changed, we should pick current workActivePixels.
        fb.getActivePixelsWeightBuffer().copy(workActivePixels);
    } else {
        // update activePixels info by OR bitmask operation
        (void)fb.getActivePixelsWeightBuffer().orOp(workActivePixels);
    }
}
    
void
FbMsgMultiChans::decodeReference(const char* name,
                                 const void *data,
                                 const size_t dataSize,
                                 scene_rdl2::grid_util::Fb& fb)
{
    FbAovShPtr fbAov = fb.getAov(name);
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeReference() before PackTiles::decodeRenderOutputReference()\n";
#       endif // end DEBUG_DECODE_MSG
        scene_rdl2::grid_util::PackTiles::decodeRenderOutputReference(data,
                                                                      dataSize,
                                                                      fbAov); // update fbAov internal info
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeReference() after PackTiles::decodeRenderOutputReference()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeReference() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    mHasRenderOutput = true;
}

void
FbMsgMultiChans::decodeRenderOutputAOV(const char* name,
                                       const void *data,
                                       const size_t dataSize,
                                       scene_rdl2::grid_util::Fb& fb)
{
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    bool activeDecodeAction {false};

    FbAovShPtr fbAov = fb.getAov(name);
    auto oldFmt = fbAov->getFormat();
    try {
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeRenderOutputAOV() before PackTiles::decodeRenderOutput()\n";
#       endif // end DEBUG_DECODE_MSG
        if (!scene_rdl2::grid_util::PackTiles::
            decodeRenderOutput(data,
                               dataSize,
                               true, // storeNumSampleData
                               workActivePixels,
                               fbAov,
                               activeDecodeAction)) { // done fbAov memory setup if needed internally
            return;
        }
#       ifdef DEBUG_DECODE_MSG
        std::cerr << ">> FbMsgMultiChans.cc decodeRenderOutputAOV() after PackTiles::decodeRenderOutput()\n";
#       endif // end DEBUG_DECODE_MSG
    } catch (scene_rdl2::except::RuntimeError& e) {
        std::cerr << ">> FbMsgMultiChans.cc decodeRenderOutputAOV() PackTiles::decode() failed."
                  << " RuntimeError:" << e.what() << '\n';
    }
    if (!activeDecodeAction) return;
    
    if (oldFmt != fbAov->getFormat() || !workActivePixels.isSameSize(fbAov->getActivePixels())) {
        // resolution/format changed, we should pick current workActivePixels
        fbAov->getActivePixels().copy(workActivePixels);
    } else {
        // update activePixels information by OR bitmask operation
        (void)fbAov->getActivePixels().orOp(workActivePixels);
    }
    mHasRenderOutput = true;
}

void
FbMsgMultiChans::parserConfigure()
{
    mParser.description("FbMsgMultiChan command");
    mParser.opt("show", "", "show internal status. might be pretty long info",
                [&](Arg& arg) -> bool { return arg.msg(show() + '\n'); });
}

} // namespace mcrt_dataio
