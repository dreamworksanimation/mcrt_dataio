// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ClientReceiverDenoiser.h"

#ifdef MOONRAY_USE_CUDA
// This header must be included in .cc file for the link to succeed
#include <optix_function_table_definition.h>
#endif

#include <scene_rdl2/common/grid_util/Fb.h>

// Basically we should use multi-thread version.
// This single thread mode is used debugging and performance comparison reason mainly.
//#define SINGLE_THREAD

#ifndef SINGLE_THREAD
#include <tbb/parallel_for.h>
#include <thread>
#endif // end SINGLE_THREAD

// denoiseActionInterval logic debug messages
//#define DEBUG_MSG_DENOISE_ACTION_INTERVAL

#ifdef DEBUG_MSG_DENOISE_ACTION_INTERVAL
#include <iomanip>
#endif // end DEBUG_MSG_DENOISE_ACTION_INTERVAL

namespace mcrt_dataio {

void
ClientReceiverDenoiser::resetTimingInfo()
{
    mLatencyTracker.reset();
    mDenoiseTimeTracker.reset();
    mDenoiseMinInterval = 0.0f;
    mPrevEvalTimingResult = 0.0f;
}

bool
ClientReceiverDenoiser::denoiseBeauty(const DenoiseEngine engine,
                                      const float latencySec,
                                      const int width,
                                      const int height,
                                      const scene_rdl2::math::Viewport* roi,
                                      const SnapshotBuffCallBack& beautyInputCallBack,
                                      const SnapshotBuffCallBack& albedoInputCallBack,
                                      const SnapshotBuffCallBack& normalInputCallBack,
                                      const int outputNumChan,
                                      std::vector<float>& beautyOutput,
                                      bool& fallback)
{
    denoiseActionTimingTrackStart(latencySec);

    mErrorMsg.clear();

    fallback = false;

    if (!setupDenoiser(engine, width, height, roi, albedoInputCallBack, normalInputCallBack)) {
        fallback = true;
        return false;
    }
    if (!mDenoiseReady) {
        fallback = true;
        return true;
    }

    bool denoiseRun = false;
    if (denoiseActionIntervalTest()) {
        mDenoiser->denoise(inputBuff(beautyInputCallBack, mBeautyInput),
                           inputBuff(albedoInputCallBack, mAlbedoInput),
                           inputBuff(normalInputCallBack, mNormalInput),
                           outputBuff(mDenoisedResult, 4),
                           &mErrorMsg);
        
        if (!mErrorMsg.empty()) {
            fallback = true;
            return false;
        }

        denoiseRun = true;
    }

    outputBuff(beautyOutput, outputNumChan);
    copyDenoisedResultToOut(outputNumChan, beautyOutput);

    if (denoiseRun) {
        denoiseActionTimingUpdate();
    }

    return true;
}

bool
ClientReceiverDenoiser::denoiseBeauty888(const DenoiseEngine engine,
                                         const float latencySec,
                                         const int width,
                                         const int height,
                                         const scene_rdl2::math::Viewport* roi,
                                         const SnapshotBuffCallBack& beautyInputCallBack,
                                         const SnapshotBuffCallBack& albedoInputCallBack,
                                         const SnapshotBuffCallBack& normalInputCallBack,
                                         std::vector<unsigned char>& beautyOutput,
                                         bool isSrgb,
                                         bool& fallback)
{
    denoiseActionTimingTrackStart(latencySec);

    mErrorMsg.clear();

    fallback = false;

    if (!setupDenoiser(engine, width, height, roi, albedoInputCallBack, normalInputCallBack)) {
        fallback = true;
        return false;
    }
    if (!mDenoiseReady) {
        fallback = true;
        return true;
    }

    bool denoiseRun = false;
    if (denoiseActionIntervalTest()) {
        mDenoiser->denoise(inputBuff(beautyInputCallBack, mBeautyInput),
                           inputBuff(albedoInputCallBack, mAlbedoInput),
                           inputBuff(normalInputCallBack, mNormalInput),
                           outputBuff(mDenoisedResult, 4),
                           &mErrorMsg);
        if (!mErrorMsg.empty()) {
            fallback = true;
            return false;
        }

        denoiseRun = true;
    }

    if (beautyOutput.size() != mBeautyInput.size()) beautyOutput.resize(mBeautyInput.size());
    scene_rdl2::grid_util::Fb::conv888Beauty(mDenoisedResult, isSrgb, beautyOutput); // rgba -> rgb888

    if (denoiseRun) {
        denoiseActionTimingUpdate();
    }

    return true;
}
    
std::string
ClientReceiverDenoiser::showStatus() const
{
    using scene_rdl2::str_util::secStr;

    std::ostringstream ostr;
    ostr << "ClientReceiverDenoiser status {\n"
         << "  sKeepMaxItems:" << sKeepMaxItems << '\n'
         << "  mLatencyTracker.getAvg():" << secStr(mLatencyTracker.getAvg()) << '\n'
         << "  mDenoiseTimeTracker.getAvg():" << secStr(mDenoiseTimeTracker.getAvg()) << '\n'
         << "  mDenoiseMinInterval:" << secStr(mDenoiseMinInterval)
         << " (current minimum interval of denoise action)\n"
         << "  mPrevEvalTimingResult:" << mPrevEvalTimingResult << " (current cost function result)\n"
         << "}";
    return ostr.str();
}

bool
ClientReceiverDenoiser::setupDenoiser(const DenoiseEngine engine,
                                      const int width,
                                      const int height,
                                      const scene_rdl2::math::Viewport* roi,
                                      const SnapshotBuffCallBack& albedoInputCallBack,
                                      const SnapshotBuffCallBack& normalInputCallBack)
{
    int denoiseWidth = (!roi) ? width : roi->width();
    int denoiseHeight = (!roi) ? height : roi->height();

    bool useAlbedo = albedoInputCallBack ? true : false;
    bool useNormals = normalInputCallBack ? true : false;

    if (!mDenoiser ||
        engine != mDenoiseEngine || 
        denoiseWidth != mDenoiseWidth || denoiseHeight != mDenoiseHeight ||
        useAlbedo != mDenoiseUseAlbedo || useNormals != mDenoiseUseNormals) {

        moonray::denoiser::DenoiserMode denoiserMode = moonray::denoiser::OPTIX;
        switch (engine) {
        case DenoiseEngine::OPTIX              : denoiserMode = moonray::denoiser::OPTIX;              break;
        case DenoiseEngine::OPEN_IMAGE_DENOISE : denoiserMode = moonray::denoiser::OPEN_IMAGE_DENOISE; break;
        default : break;
        }

        mDenoiseEngine = engine;
        mDenoiseWidth = denoiseWidth;
        mDenoiseHeight = denoiseHeight;
        mDenoiseUseAlbedo = useAlbedo;
        mDenoiseUseNormals = useNormals;

        mDenoiser = std::make_unique<moonray::denoiser::Denoiser>(denoiserMode,
                                                                  denoiseWidth, denoiseHeight,
                                                                  useAlbedo, useNormals, &mErrorMsg);
        if (!mErrorMsg.empty()) {
            mErrorMsg += " : Fall back to disable denoiser";
            mDenoiseReady = false;
            return false;
        }
        mDenoiseReady = true;
    }

    return true;
}

//------------------------------------------------------------------------------------------

const float*
ClientReceiverDenoiser::inputBuff(const SnapshotBuffCallBack& callBack, std::vector<float>& buff) const
{
    if (!callBack) {
        return nullptr;
    }
    callBack(buff);
    return buff.data();
}

float*
ClientReceiverDenoiser::outputBuff(std::vector<float>& buff, int numChan) const
{
    size_t totalSize = mDenoiser->imageWidth() * mDenoiser->imageHeight() * numChan;
    if (buff.size() != totalSize) {
        buff.resize(totalSize);
    }
    return &buff[0];
}

void
ClientReceiverDenoiser::copyDenoisedResultToOut(const int outputNumChan, std::vector<float>& out) const
{
    size_t pixTotal = mDenoiser->imageWidth() * mDenoiser->imageHeight();
    size_t copyPixByte = ((outputNumChan < 4) ? outputNumChan : 4) * sizeof(float);
#   ifdef SINGLE_THREAD
    for (size_t i = 0 ; i < pixTotal; ++i) {
        std::memcpy(&out[i * outputNumChan], &mDenoisedResult[i * 4], copyPixByte);
    }
#   else // else SINGLE_THREAD
    size_t taskSize = std::max(pixTotal / (std::thread::hardware_concurrency() * 10), 1UL);
    tbb::blocked_range<size_t> range(0, pixTotal, taskSize);
    tbb::parallel_for(range, [&](const tbb::blocked_range<size_t> &r) {
            for (size_t i = r.begin(); i < r.end(); ++i) {
                std::memcpy(&out[i * outputNumChan], &mDenoisedResult[i * 4], copyPixByte);
            }
        });
#   endif // end !SINGLE_THREAD    
}

void
ClientReceiverDenoiser::denoiseActionTimingTrackStart(const float latencySec)
{
    mDenoiseAction.start();
    mLatencyTracker.set(latencySec);
}

bool
ClientReceiverDenoiser::denoiseActionIntervalTest()
//
// This function automatically adjusts the denoise action interval in order to minimize the cost function.
//    
{
    if (mDenoiseTimeTracker.isEmpty()) {
        return true; // very 1st time to check => do denoise
    }

    float denoiseTimeAvg = mDenoiseTimeTracker.getAvg();
    float actionInterval = mDenoiseActionInterval.end();
    if (actionInterval <= denoiseTimeAvg) {
        return false; // denoise action call is shorter interval than denoise action cost itself.
    }

    if (mDenoiseMinInterval < denoiseTimeAvg) {
        mDenoiseMinInterval = denoiseTimeAvg;
    }

    auto clipDeltaVal = [&](const float v, const float scale) -> float {
        float limit = mDenoiseMinInterval * scale;
        if (v > limit) {
            return limit;
        }
        return v;
    };

#   ifdef DEBUG_MSG_DENOISE_ACTION_INTERVAL
    auto showInfo = [&](const std::string& msg,
                        const float denoiseTimeAvg,
                        const float currLatency,
                        const float currVal,
                        const float delta) -> std::string {
        auto msShow = [](const float sec) -> std::string {
            std::ostringstream ostr;
            ostr << std::setw(6) << std::fixed << std::setprecision(2) << (sec * 1000.0f);
            return ostr.str();
        };
        std::ostringstream ostr;
        ostr
        << msg
        << " denoise:" << msShow(denoiseTimeAvg) << "ms"
        << " currLatency:" << msShow(currLatency) << "ms"
        << " currVal:" << std::setw(6) << std::fixed << std::setprecision(4) << currVal
        << " delta:" << std::setw(6) << std::fixed << std::setprecision(4) << delta
        << " minInterval:" << msShow(mDenoiseMinInterval) << "ms";
        return ostr.str();
    };
#   endif // end DEBUG_MSG_DENOISE_ACTION_INTERVAL

    if (mDenoiseMinInterval < actionInterval) {
        auto costFunc = [](const float latencySec, const float denoiseMinIntervalSec) -> float {
            static constexpr float weight = 0.9f; // Heuristically defined weight based on the several scenes.
            return latencySec + weight * denoiseMinIntervalSec;
        };
        float currLatency = mLatencyTracker.getAvg();
        float currVal = costFunc(currLatency, mDenoiseMinInterval);

        float delta = 0.0f;
        if (mPrevEvalTimingResult > 0.0f) {
            if (mPrevEvalTimingResult < currVal) {
                // getting worse => try increase mDenoiseMinInterval
                delta = currVal - mPrevEvalTimingResult;
                delta = clipDeltaVal(delta, 0.5f);
                mDenoiseMinInterval += delta;

                // We set the max interval as 20% longer than the latency average.
                // This 20% is heuristically defined based on several tests.
                float intervalMax = mLatencyTracker.getAvg() * 1.2f;
                mDenoiseMinInterval = std::min(mDenoiseMinInterval, intervalMax);
#               ifdef DEBUG_MSG_DENOISE_ACTION_INTERVAL
                std::cerr << showInfo("+", denoiseTimeAvg, currLatency, currVal, delta) << '\n';
#               endif // end DEBUG_MSG_DENOISE_ACTION_INTERVAL
            } else if (currVal < mPrevEvalTimingResult) {
                // getting better => try decrease mDenoiseMinInterval
                delta = mPrevEvalTimingResult - currVal;
                delta = clipDeltaVal(delta, 0.5f);
                mDenoiseMinInterval -= delta;
                mDenoiseMinInterval = std::max(mDenoiseMinInterval, denoiseTimeAvg);
#               ifdef DEBUG_MSG_DENOISE_ACTION_INTERVAL
                std::cerr << showInfo("-", denoiseTimeAvg, currLatency, currVal, delta) << '\n';
#               endif // end DEBUG_MSG_DENOISE_ACTION_INTERVAL
            }
        }
        mPrevEvalTimingResult = currVal;

        return true;
    }

    return false;
}

void
ClientReceiverDenoiser::denoiseActionTimingUpdate()
{
    mDenoiseTimeTracker.set(mDenoiseAction.end());
    mDenoiseActionInterval.start();
}

} // namespace mcrt_dataio
