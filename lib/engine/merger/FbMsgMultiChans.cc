// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "FbMsgMultiChans.h"
#include "GlobalNodeInfo.h"

#include <scene_rdl2/common/fb_util/ActivePixels.h>
#include <scene_rdl2/common/grid_util/PackTiles.h>
#include <scene_rdl2/scene/rdl2/ValueContainerDeq.h>
#include <scene_rdl2/scene/rdl2/ValueContainerEnq.h>

#include <sstream>

namespace mcrt_dataio {

const char *FbMsgMultiChans::latencyLogName  = "latencyLog";
const char *FbMsgMultiChans::auxInfoName     = "auxInfo";

//-------------------------------------------------------------------------------------------------------------

bool
FbMsgMultiChans::push(const bool delayDecode,
                      const mcrt::ProgressiveFrame &progressive, scene_rdl2::grid_util::Fb &fb)
{
    if (progressive.getProgress() < 0.0f) {
        // just in case. We skip info only message. Info only message is already processed
        // before this function
        return true;
    }

    mProgress = progressive.getProgress();

    mStatus = progressive.getStatus();
    if (mStatus == mcrt::BaseFrame::STARTED) {
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

#   ifdef SINGLE_THREAD
    for (const mcrt::BaseFrame::DataBuffer &buffer: progressive.mBuffers) {
        if (!pushBuffer(delayDecode,
                        buffer.mName,
                        buffer.mData,
                        buffer.mDataLength,
                        fb)) {
            return false;       // error
        }
    }
#   else // else SINGLE_THREAD
    if (delayDecode) {
        // This is single thread execution. Under delayDecode mode,
        // We don't need to run by multi-thread because we only copy shared_ptr.
        // Single thread is enough.
        for (const mcrt::BaseFrame::DataBuffer &buffer: progressive.mBuffers) {
            if (!pushBuffer(delayDecode,
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
        std::vector<const mcrt::BaseFrame::DataBuffer *> bufferArray;
        for (const mcrt::BaseFrame::DataBuffer &buffer: progressive.mBuffers) {
            bufferArray.push_back(&buffer);
        }
        if (bufferArray.size()) {
            bool errorST = false;
            tbb::blocked_range<size_t> range(0, bufferArray.size());
            tbb::parallel_for(range, [&](const tbb::blocked_range<size_t> &r) {
                    for (size_t id = r.begin(); id < r.end(); ++id) {
                        if (!pushBuffer(delayDecode,
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
#   endif // end !SINGLE_THREAD
    
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

//-------------------------------------------------------------------------------------------------------------

bool
FbMsgMultiChans::pushBuffer(const bool delayDecode,
                            const char *name,
                            DataPtr dataPtr,
                            const size_t dataSize,
                            scene_rdl2::grid_util::Fb &fb)
//
// fb internal information is accumulatively updated (and not initialized every call).
// fb is initialized (resized) internally based on message if needed (like resize situation).
//
{
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
FbMsgMultiChans::decodeAll(scene_rdl2::grid_util::Fb &fb)
{
#   ifdef SINGLE_THREAD
    auto itr = mMsgArray.cbegin();
    while (1) {
        if (itr == mMsgArray.cend()) break; // end of loop

        const std::string &name = itr->first;
        if (!strcmp(name.c_str(), latencyLogName)) {
            itr++;              // decode skip latencyLog data
        } else {
            const std::vector<std::vector<char>> &datas = (itr->second)->data();
            for (const std::vector<char> &data : datas) {
                decodeData(name.c_str(), data.data(), data.size(), fb);
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
#   endif // end !SINGLE_THREAD     
}

void    
FbMsgMultiChans::decodeData(const char *name, const void *data, const size_t dataSize,
                            scene_rdl2::grid_util::Fb &fb)
{
    //
    // PackTile codec data
    //
    scene_rdl2::grid_util::PackTiles::DataType dataType =
        scene_rdl2::grid_util::PackTiles::decodeDataType(data, dataSize);

    scene_rdl2::fb_util::ActivePixels workActivePixels;
    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::BEAUTY_WITH_NUMSAMPLE) {
        //
        // beauty
        //
        scene_rdl2::grid_util::PackTiles::
            decode(false, // renderBufferOdd
                   data,
                   dataSize,
                   true, // storeNumSampleData
                   workActivePixels,
                   fb.getRenderBufferTiled(), // normalized color
                   fb.getNumSampleBufferTiled(),
                   fb.getRenderBufferCoarsePassPrecision(),
                   fb.getRenderBufferFinePassPrecision());
        if (!workActivePixels.isSameSize(fb.getActivePixels())) {
            // resolution changed, we should pick current workActivePixels
            fb.getActivePixels().copy(workActivePixels);
        } else {
            // update activePixels info by OR bitmask operation
            (void)fb.getActivePixels().orOp(workActivePixels);
        }
        mHasBeauty = true;

    } else if (dataType == scene_rdl2::grid_util::PackTiles::DataType::BEAUTYODD_WITH_NUMSAMPLE) {
        //
        // beautyOdd
        //
        // Actually we don't have Ccoarse,Fine}PassPrecision info for renderBufferOdd.
        scene_rdl2::grid_util::CoarsePassPrecision dummyCoarsePassPrecision;
        scene_rdl2::grid_util::FinePassPrecision dummyFinePassPrecision;
        fb.setupRenderBufferOdd(nullptr);
        scene_rdl2::grid_util::PackTiles::
            decode(true, // renderBufferOdd
                   data,
                   dataSize,
                   true, // storeNumSampleData
                   workActivePixels,
                   fb.getRenderBufferOddTiled(), // normalized color
                   fb.getRenderBufferOddNumSampleBufferTiled(),
                   dummyCoarsePassPrecision,
                   dummyFinePassPrecision);
        if (!workActivePixels.isSameSize(fb.getActivePixelsRenderBufferOdd())) {
            // resolution changed, we should pick current workActivePixels.
            fb.getActivePixelsRenderBufferOdd().copy(workActivePixels);
        } else {
            // update activePixels info by OR bitmask operation
            (void)fb.getActivePixelsRenderBufferOdd().orOp(workActivePixels);
        }
        mHasRenderBufferOdd = true;

    } else if (dataType == scene_rdl2::grid_util::PackTiles::DataType::PIXELINFO) {
        //
        // pixelInfo
        //
        fb.setupPixelInfo(nullptr, name);
        scene_rdl2::grid_util::PackTiles::
            decodePixelInfo(data, dataSize, workActivePixels,
                            fb.getPixelInfoBufferTiled(),
                            fb.getPixelInfoCoarsePassPrecision(),
                            fb.getPixelInfoFinePassPrecision());
        if (!workActivePixels.isSameSize(fb.getActivePixelsPixelInfo())) {
            // resolution changed, we should pick current workActivePixels.
            fb.getActivePixelsPixelInfo().copy(workActivePixels);
        } else {
            // update activePixels info by OR bitmask operation
            (void)fb.getActivePixelsPixelInfo().orOp(workActivePixels);
        }
        mHasPixelInfo = true;
        
    } else if (dataType == scene_rdl2::grid_util::PackTiles::DataType::HEATMAP_WITH_NUMSAMPLE) {
        //
        // heatMap
        //
        fb.setupHeatMap(nullptr, name);
        scene_rdl2::grid_util::PackTiles::
            decodeHeatMap(data,
                          dataSize,
                          true, // storeNumSampleData
                          workActivePixels,
                          fb.getHeatMapSecBufferTiled(),
                          fb.getHeatMapNumSampleBufferTiled());
        if (!workActivePixels.isSameSize(fb.getActivePixelsHeatMap())) {
            // resolution changed, we should pick current workActivePixels.
            fb.getActivePixelsHeatMap().copy(workActivePixels);
        } else {
            // update activePixels info by OR bitmask operation
            (void)fb.getActivePixelsHeatMap().orOp(workActivePixels);
        }
        mHasHeatMap = true;
 
    } else if (dataType == scene_rdl2::grid_util::PackTiles::DataType::WEIGHT) {
        //
        // weight buffer
        //
        fb.setupWeightBuffer(nullptr, name);
        scene_rdl2::grid_util::PackTiles::
            decodeWeightBuffer(data,
                               dataSize,
                               workActivePixels,
                               fb.getWeightBufferTiled(),
                               fb.getWeightBufferCoarsePassPrecision(),
                               fb.getWeightBufferFinePassPrecision());
        if (!workActivePixels.isSameSize(fb.getActivePixelsWeightBuffer())) {
            // resolution changed, we should pick current workActivePixels.
            fb.getActivePixelsWeightBuffer().copy(workActivePixels);
        } else {
            // update activePixels info by OR bitmask operation
            (void)fb.getActivePixelsWeightBuffer().orOp(workActivePixels);
        }

    } else if (dataType == scene_rdl2::grid_util::PackTiles::DataType::REFERENCE) {
        //
        // renderOutput reference AOVs (Beauty, Alpha, HeatMap, Weight, BeautyAux, AlphaAux)
        //
        FbAovShPtr fbAov = fb.getAov(name);
        scene_rdl2::grid_util::PackTiles::
            decodeRenderOutputReference(data,
                                        dataSize,
                                        fbAov); // update fbAov internal info
        mHasRenderOutput = true;

    } else if (dataType != scene_rdl2::grid_util::PackTiles::DataType::UNDEF) {
        //
        // renderOutput AOVs
        //
        FbAovShPtr fbAov = fb.getAov(name);
        auto oldFmt = fbAov->getFormat();
        scene_rdl2::grid_util::PackTiles::
            decodeRenderOutput(data,
                               dataSize,
                               true, // storeNumSampleData
                               workActivePixels,
                               fbAov); // done fbAov memory setup if needed internally
        if (oldFmt != fbAov->getFormat() || !workActivePixels.isSameSize(fbAov->getActivePixels())) {
            // resolution/format changed, we should pick current workActivePixels
            fbAov->getActivePixels().copy(workActivePixels);
        } else {
            // update activePixels information by OR bitmask operation
            (void)fbAov->getActivePixels().orOp(workActivePixels);
        }
        mHasRenderOutput = true;
    }
}

} // namespace mcrt_dataio

