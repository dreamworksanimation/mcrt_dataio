// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ClientReceiverFb.h"

#include <mcrt_denoise/denoiser/Denoiser.h>
#include <scene_rdl2/common/grid_util/FloatValueTracker.h>
#include <scene_rdl2/common/math/Viewport.h>
#include <scene_rdl2/common/rec_time/RecTime.h>

#include <functional>
#include <vector>

namespace mcrt_dataio {

class ClientReceiverDenoiser
{
public:
    using DenoiseEngine = ClientReceiverFb::DenoiseEngine;
    using SnapshotBuffCallBack = std::function<void(std::vector<float>& buff)>;

    ClientReceiverDenoiser()
        : mDenoiseEngine(ClientReceiverFb::DenoiseEngine::OPTIX)
        , mDenoiseReady(true)
        , mDenoiseWidth(0)
        , mDenoiseHeight(0)
        , mDenoiseUseAlbedo(false)
        , mDenoiseUseNormals(false)
        , mLatencyTracker(sKeepMaxItems)
        , mDenoiseTimeTracker(sKeepMaxItems)
        , mDenoiseMinInterval(0.0f)
        , mPrevEvalTimingResult(0.0f)
    {}
        
    void resetTimingInfo();

    bool denoiseBeauty(const DenoiseEngine engine,
                       const float latencySec,
                       const int width,
                       const int height,
                       const scene_rdl2::math::Viewport* roi,
                       const SnapshotBuffCallBack& beautyInputSnapshot,
                       const SnapshotBuffCallBack& albedoInputSnapshot,
                       const SnapshotBuffCallBack& normalInputSnapshot,
                       const int outputNumChan,
                       std::vector<float>& beautyOutput,
                       bool& fallback);

    bool denoiseBeauty888(const DenoiseEngine engine,
                          const float latencySec,
                          const int width,
                          const int height,
                          const scene_rdl2::math::Viewport* roi,
                          const SnapshotBuffCallBack& beautyInputSnapshot,
                          const SnapshotBuffCallBack& albedoInputSnapshot,
                          const SnapshotBuffCallBack& normalInputSnapshot,
                          std::vector<unsigned char>& beautyOutput, // rgb : apply gamma22 or sRGB
                          bool isSrgb, // true=sRGB, false=gamma22
                          bool& fallback);

    const std::string& getErrorMsg() const { return mErrorMsg; }

    std::string showStatus() const;

protected:
    bool setupDenoiser(const DenoiseEngine engine,
                       const int width, const int height, const scene_rdl2::math::Viewport* roi,
                       const SnapshotBuffCallBack& albedoInputCallBack,
                       const SnapshotBuffCallBack& normalInputCallBack);

    const float* inputBuff(const SnapshotBuffCallBack& callBack, std::vector<float>& buff) const;
    float* outputBuff(std::vector<float>& buff, int numChan) const;
    void copyDenoisedResultToOut(const int outputNumChan, std::vector<float>& beautyOutput) const;

    void denoiseActionTimingTrackStart(const float latencySec);
    bool denoiseActionIntervalTest();
    void denoiseActionTimingUpdate();

    //------------------------------

    DenoiseEngine mDenoiseEngine;

    std::vector<float> mBeautyInput;
    std::vector<float> mAlbedoInput;
    std::vector<float> mNormalInput;
    std::vector<float> mDenoisedResult; // We always keep the denoise action result.

    bool mDenoiseReady;
    int mDenoiseWidth;
    int mDenoiseHeight;
    bool mDenoiseUseAlbedo;
    bool mDenoiseUseNormals;
    std::unique_ptr<moonray::denoiser::Denoiser> mDenoiser;

    std::string mErrorMsg;

    scene_rdl2::rec_time::RecTime mDenoiseActionInterval;
    scene_rdl2::rec_time::RecTime mDenoiseAction;

    static constexpr int sKeepMaxItems = 10;
    scene_rdl2::grid_util::FloatValueTracker mLatencyTracker;
    scene_rdl2::grid_util::FloatValueTracker mDenoiseTimeTracker;

    float mDenoiseMinInterval; // sec
    float mPrevEvalTimingResult;
};

} // namespace mcrt_dataio
