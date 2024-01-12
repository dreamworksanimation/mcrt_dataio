// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "ClientReceiverFb.h"

#include "ClientReceiverConsoleDriver.h"
#include "ClientReceiverDenoiser.h"
#include "ClientReceiverStats.h"
#include "TelemetryDisplay.h"
#include "TimingAnalysis.h"
#include "TimingRecorderHydra.h"

#include <mcrt_dataio/engine/merger/GlobalNodeInfo.h>
#include <mcrt_dataio/share/codec/InfoRec.h>
#include <mcrt_dataio/share/util/FpsTracker.h>
#include <mcrt_dataio/share/util/MiscUtil.h>
#include <mcrt_dataio/share/util/SysUsage.h>

#include <scene_rdl2/common/grid_util/PackTiles.h>
#include <scene_rdl2/common/grid_util/PackTilesPassPrecision.h>
#include <scene_rdl2/common/rec_time/RecTime.h>
#include <scene_rdl2/render/util/StrUtil.h>

#include <algorithm> // std::max()
#include <cstdlib> // getenv()
#include <iomanip>
#include <json/json.h>
#include <json/writer.h>
#include <limits>
#include <tbb/parallel_for.h>

// This directive is used only for debug purpose.
// If this directive is active, all get*Rgb888() functions are switched to
// debug mode and internally using full float API (like get*()) functions in order to
// verify full float API by 8bit API.
// This directive ** SHOULD BE COMMENTED OUT ** for release code.
//#define VERIFY_FLOAT_API_BY_UC

namespace mcrt_dataio {

class ClientReceiverFb::Impl
{
public:
    using CallBackStartedCondition = ClientReceiverFb::CallBackStartedCondition;
    using CallBackGenericComment = ClientReceiverFb::CallBackGenericComment;
    using CallBackSendMessage = std::function<bool(const arras4::api::MessageContentConstPtr msg)>;
    using DenoiseEngine = ClientReceiverFb::DenoiseEngine;
    using DenoiseMode = ClientReceiverFb::DenoiseMode;
    using SenderMachineId = ClientReceiverFb::SenderMachineId;
    using Arg = scene_rdl2::grid_util::Arg;
    using Parser = scene_rdl2::grid_util::Parser;

    Impl(bool initialTelemetryOverlayCondition);
    ~Impl() {}

    /// This class is Non-copyable
    Impl &operator = (const Impl&) = delete;
    Impl(const Impl&) = delete;

    void setClientMessage(const std::string& msg);
    void clearClientMessage() { mClientMessage.clear(); }

    //------------------------------

    bool decodeProgressiveFrame(const mcrt::ProgressiveFrame& message,
                                const bool doParallel,
                                const CallBackStartedCondition& callBackFuncAtStartedCondition,
                                const CallBackGenericComment& callBackFuncForGenericComment);

    //------------------------------

    int getReceivedImageSenderMachineId() const { return mRecvImgSenderMachineId; }

    size_t getViewId() const { return mViewId; }
    uint32_t getFrameId() const { return mFrameId; }
    mcrt::BaseFrame::Status getStatus() const { return mStatus; }
    ClientReceiverFb::BackendStat getBackendStat() const;
    float getRenderPrepProgress() const { return mRenderPrepProgress; } // return fraction
    float getProgress() const { return mProgress; }

    bool isCoarsePass() const { return (mCoarsePassStatus == 0)? true: false; }

    const std::string& getDenoiserAlbedoInputName() const { return mDenoiserAlbedoInputName; }
    const std::string& getDenoiserNormalInputName() const { return mDenoiserNormalInputName; }

    uint64_t getSnapshotStartTime() const { return mSnapshotStartTime; }
    float getElapsedSecFromStart(); // return sec

    uint64_t getRecvMsgSize() const { return mRecvMsgSize; } // return last message size as byte

    unsigned getWidth() const { return mRezedViewport.width(); }
    unsigned getHeight() const { return mRezedViewport.height(); }

    const scene_rdl2::math::Viewport& getRezedViewport() const { return mRezedViewport; } // closed viewport
    bool getRoiViewportStatus() const { return mRoiViewportStatus; }
    const scene_rdl2::math::Viewport& getRoiViewport() const { return mRoiViewport; } // closed viewport

    bool getPixelInfoStatus() const { return mFb.getPixelInfoStatus(); }
    const std::string& getPixelInfoName() const { return mFb.getPixelInfoName(); }
    int getPixelInfoNumChan() const { return 1; }

    bool getHeatMapStatus() const { return mFb.getHeatMapStatus(); }
    const std::string& getHeatMapName() const { return mFb.getHeatMapName(); }
    int getHeatMapNumChan() const { return 1; }

    bool getWeightBufferStatus() const { return mFb.getWeightBufferStatus(); }
    const std::string& getWeightBufferName() const { return mFb.getWeightBufferName(); }
    int getWeightBufferNumChan() const { return 1; }

    bool getRenderBufferOddStatus() const { return mFb.getRenderBufferOddStatus(); }
    int getRenderBufferOddNumChan() const { return 4; }

    unsigned getTotalRenderOutput() const { return mFb.getTotalRenderOutput(); }
    const std::string& getRenderOutputName(const unsigned id) const;
    int getRenderOutputNumChan(const unsigned id) const;
    int getRenderOutputNumChan(const std::string& aovName) const;
    bool getRenderOutputClosestFilter(const unsigned id) const;
    bool getRenderOutputClosestFilter(const std::string& aovName) const;

    //------------------------------

    void setDenoiseEngine(DenoiseEngine engine) { mDenoiseEngine = engine; }
    DenoiseEngine getDenoiseEngine() const { return mDenoiseEngine; }
    void setBeautyDenoiseMode(DenoiseMode mode) { mBeautyDenoiseMode = mode; }
    DenoiseMode getBeautyDenoiseMode() const { return mBeautyDenoiseMode; }
    const std::string& getErrorMsg() const { return mErrorMsg; }

    //------------------------------

    bool getBeautyRgb888(std::vector<unsigned char>& rgbFrame, const bool top2bottom, const bool isSrgb);
    void getBeautyRgb888NoDenoise(std::vector<unsigned char>& rgbFrame,
                                  const bool top2bottom, const bool isSrgb);

    bool getPixelInfoRgb888(std::vector<unsigned char>& rgbFrame, const bool top2bottom, const bool isSrgb);
    bool getHeatMapRgb888(std::vector<unsigned char>& rgbFrame, const bool top2bottom, const bool isSrgb);
    bool getWeightBufferRgb888(std::vector<unsigned char>& rgbFrame, const bool top2bottom, const bool isSrgb);
    bool getBeautyAuxRgb888(std::vector<unsigned char>& rgbFrame, const bool top2bottom, const bool isSrgb);

    bool getRenderOutputRgb888(const unsigned id,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottom,
                               const bool isSrgb,
                               const bool closestFilterDepthOutput);
    void getRenderOutputRgb888NoDenoise(const unsigned id,
                                        std::vector<unsigned char>& rgbFrame,
                                        const bool top2bottom,
                                        const bool isSrgb,
                                        const bool closestFilterDepthOutput);
    bool getRenderOutputRgb888(const std::string& aovName,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottom,
                               const bool isSrgb,
                               const bool closestFilterDepthOutput);
    void getRenderOutputRgb888NoDenoise(const std::string& aovName,
                                        std::vector<unsigned char>& rgbFrame,
                                        const bool top2bottom,
                                        const bool isSrgb,
                                        const bool closestFilterDepthOutput);

    //------------------------------

    bool getBeauty(std::vector<float>& rgba, const bool top2bottom); // 4 channels/pixel
    void getBeautyNoDenoise(std::vector<float>& rgba, const bool top2bottom); // 4 channels/pixel

    bool getPixelInfo(std::vector<float>& data, const bool top2bottom); // 1 channel/pixel
    bool getHeatMap(std::vector<float>& data, const bool top2bottom);   // 1 channel/pixel
    bool getWeightBuffer(std::vector<float>& data, const bool top2bottom); // 1 channel/pixel
    bool getBeautyOdd(std::vector<float>& rgba, const bool top2bottom); // 4 channels/pixel
    bool getBeautyOddNoDenoise(std::vector<float>& rgba, const bool top2bottom); // 4 channels/pixel

    int  getRenderOutput(const unsigned id,
                         std::vector<float>& data,
                         const bool top2bottom,
                         const bool closestFilterDepthOutput);
    int  getRenderOutputNoDenoise(const unsigned id,
                                  std::vector<float>& data,
                                  const bool top2bottom,
                                  const bool closestFilterDepthOutput);
    int  getRenderOutput(const std::string& aovName,
                         std::vector<float>& data,
                         const bool top2bottom,
                         const bool closestFilterDepthOutput);
    int  getRenderOutputNoDenoise(const std::string& aovName,
                                  std::vector<float>& data,
                                  const bool top2bottom,
                                  const bool closestFilterDepthOutput);

    // always store data into float4 pixel
    int getRenderOutputF4(const unsigned id,
                          std::vector<float>& data,
                          const bool top2bottom,
                          const bool closestFilterDepthOutput);
    int getRenderOutputF4(const std::string& aovName,
                          std::vector<float>& data,
                          const bool top2bottom,
                          const bool closestFilterDepthOutput);

    //------------------------------

    scene_rdl2::math::Vec4f getPixBeauty(const int sx, const int sy) const;
    float getPixPixelInfo(const int sx, const int sy) const;
    float getPixHeatMap(const int sx, const int sy) const;
    float getPixWeightBuffer(const int sx, const int sy) const;
    scene_rdl2::math::Vec4f getPixBeautyOdd(const int sx, const int sy) const;

    int getPixRenderOutput(const unsigned id, const int sx, const int sy,
                           std::vector<float>& out) const;
    int getPixRenderOutput(const std::string& aovName, const int sx, const int sy,
                           std::vector<float>& out) const;    

    std::string showPix(const int sx, const int sy, const std::string& aovName) const;

    //------------------------------

    const scene_rdl2::grid_util::LatencyLog& getLatencyLog() const { return mLatencyLog; }

    const scene_rdl2::grid_util::LatencyLogUpstream& getLatencyLogUpstream() const
    {
        return mLatencyLogUpstream;
    }

    //------------------------------

    void setInfoRecInterval(const float sec) { mInfoRecInterval = sec; }
    void setInfoRecDisplayInterval(const float sec) { mInfoRecDisplayInterval = sec; }
    void setInfoRecFileName(const std::string& fileName) {
        mInfoRecFileName = fileName;
        std::cerr << ">> ClientReceiverFb.cc infoRec"
                  << " interval:" << mInfoRecInterval << "sec"
                  << " display:" << mInfoRecDisplayInterval << "sec"
                  << " file:" << mInfoRecFileName << std::endl;
    }

    void updateStatsMsgInterval();
    void updateStatsProgressiveFrame();
    bool getStats(const float intervalSec, std::string& outMsg);

    float getRecvImageDataFps() { return mRecvImageDataFps.getFps(); }
    
    unsigned getFbActivityCounter() { return mFbActivityCounter; }

    //------------------------------

    void consoleEnable(ClientReceiverFb* fbReceiver,
                       const unsigned short port,
                       const CallBackSendMessage& sendMessage);
    ClientReceiverConsoleDriver& consoleDriver() { return mConsoleDriver; }

    Parser& getParser() { return mParser; }

    void setTimingRecorderHydra(std::shared_ptr<TimingRecorderHydra> ptr);

    //------------------------------

    void setTelemetryOverlayReso(unsigned width, unsigned height);
    void setTelemetryOverlayActive(bool sw);
    bool getTelemetryOverlayActive() const;
    std::vector<std::string> getAllTelemetryPanelName();
    void setTelemetryInitialPanel(const std::string& panelName);
    void switchTelemetryPanelByName(const std::string& panelName);
    void switchTelemetryPanelToNext();
    void switchTelemetryPanelToPrev();
    void switchTelemetryPanelToParent();
    void switchTelemetryPanelToChild();

private:
    std::string mClientMessage;

    // last received image data's sender machineId
    int mRecvImgSenderMachineId {static_cast<int>(SenderMachineId::UNKNOWN)}; // 0orPositive:mcrt, or enum value of SenderMachineId

    size_t mViewId {0};
    uint32_t mLastFrameId {!0};
    uint32_t mFrameId {0};
    mcrt::BaseFrame::Status mStatus {mcrt::BaseFrame::FINISHED};
    float mRenderPrepProgress {0.0f};
    float mProgress {-1.0f};
    unsigned mFbActivityCounter {0};
    unsigned mDecodeProgressiveFrameCounter {0};

    int mCoarsePassStatus {0}; // 0:coarsePass 1:nonCoarsePass 2:unknown
    std::string mDenoiserAlbedoInputName;
    std::string mDenoiserNormalInputName;
    uint64_t mSnapshotStartTime {0}; // time of did snapshot at mcrt computation
    float mCurrentLatencySec {0.0f};

    scene_rdl2::math::Viewport mRezedViewport;

    bool mRoiViewportStatus {false};
    scene_rdl2::math::Viewport mRoiViewport;

    bool mResetFbWithColorMode {false};
    scene_rdl2::grid_util::Fb mFb;

    DenoiseEngine mDenoiseEngine {DenoiseEngine::OPTIX};
    DenoiseMode mBeautyDenoiseMode {DenoiseMode::DISABLE};
    std::string mErrorMsg;
    ClientReceiverDenoiser mDenoiser;

    scene_rdl2::grid_util::LatencyLog mLatencyLog;
    scene_rdl2::grid_util::LatencyLogUpstream mLatencyLogUpstream;

    unsigned mTelemetryOverlayResoWidth {640};
    unsigned mTelemetryOverlayResoHeight {360};
    telemetry::Display mTelemetryDisplay;

    SysUsage mSysUsage; // system info of client host

    //------------------------------

    scene_rdl2::rec_time::RecTime mElapsedTimeFromStart; // elapsed time information from image = STARTED

    uint64_t mRecvMsgSize {0};      // last message's size

    //------------------------------

    ClientReceiverStats mStats;

    uint32_t mLastSyncId {0xffffffff};
    scene_rdl2::rec_time::RecTime mLastGetStatsTime; // for getStats()
    float mLastProgress {0.0f};

    //------------------------------

    GlobalNodeInfo mGlobalNodeInfo { /* decodeOnly = */ true,
                                     /* valueKeepDurationSec = */ 5.0f,
                                     /* msgSendHandler = */ nullptr};

    bool mClockDeltaRun {false};

    float mInfoRecInterval {0.0f}; // sec
    float mInfoRecDisplayInterval {10.0f}; // sec
    InfoRecMaster mInfoRecMaster;
    scene_rdl2::rec_time::RecTime mDispInfoRec;
    std::string mInfoRecFileName {"./run_"};
    scene_rdl2::rec_time::RecTime mLastInfoRecOut;

    FpsTracker mRecvImageDataFps {3.0f};

    //------------------------------

    ClientReceiverConsoleDriver mConsoleDriver;

    Parser mParser;

    bool mRenderPrepDetailedProgressDump {false}; // for debug
    int mRenderPrepDetailedProgressDumpMode {0}; // 0:fraction 1:full-dump

    // last syncId of renderPrepDetailedProgress dump
    unsigned mRenderPrepDetailedProgressShowLastSyncId {std::numeric_limits<unsigned>::max()};

    unsigned mRenderPrepDetailedProgressShowCompleteCount {0}; // for renderPrepDetailedProgress dump logic
    size_t mShowMcrtTotal {0}; // for debug

    std::shared_ptr<TimingRecorderHydra> mTimingRecorderHydra;
    TimingAnalysis mTimingAnalysis {mGlobalNodeInfo};

    //------------------------------

    void initErrorMsg() { mErrorMsg.clear(); }
    void addErrorMsg(const std::string& msg);

    void updateCpuMemUsage();
    void updateNetIO();

    bool decodeProgressiveFrameBuff(const mcrt::BaseFrame::DataBuffer& buffer);
    void decodeAuxInfo(const mcrt::BaseFrame::DataBuffer& buffer);
    void afterDecode(const CallBackGenericComment& callBackFuncForGenericComment);
    void processGenericComment(const CallBackGenericComment& callBackFuncForGenericComment);

    void infoRecUpdate();
    void infoRecUpdateDataAll();
    void infoRecUpdateGlobal();
    void infoRecUpdateClient(InfoRecMaster::InfoRecItemShPtr recItem);
    void infoRecUpdateMerge(InfoRecMaster::InfoRecItemShPtr recItem);
    void infoRecUpdateAllNodes(InfoRecMaster::InfoRecItemShPtr recItem);

    uint64_t convertTimeBackend2Client(const uint64_t backendTimeUSec);
    std::string showProgress() const;
    void renderPrepDetailedProgress();

    static bool denoiseAlbedoInputCheck(const DenoiseMode mode, const std::string& inputAovName);
    static bool denoiseNormalInputCheck(const DenoiseMode mode, const std::string& inputAovName);
    bool runDenoise888(std::vector<unsigned char>& rgbFrame,
                       const bool top2bottom,
                       const bool isSrgb,
                       const std::function<void(std::vector<float>& buff)>& setInputCallBack,
                       bool& fallback);
    bool runDenoise(const int outputNumChan,
                    std::vector<float>& rgba,
                    const bool top2bottom,
                    const std::function<void(std::vector<float>& buff)>& setInputCallBack,
                    bool& fallback);

    void setupTelemetryDisplayInfo(telemetry::DisplayInfo& displayInfo);

    void parserConfigure();
    std::string showDenoiseInfo() const;
    std::string showRenderPrepProgress() const;
    std::string showViewportInfo() const;

    void telemetryResetTest(); // for the telemetry testing purposes
}; // ClientReceiverFb::Impl

//------------------------------------------------------------------------------------------

ClientReceiverFb::Impl::Impl(bool initialTelemetryOverlayCondition)
{
    parserConfigure();

    if (initialTelemetryOverlayCondition) {
        mProgress = 0.0f;
        setTelemetryOverlayActive(true);
    } else {
        setTelemetryOverlayActive(false);
    }

    mGlobalNodeInfo.setClientHostName(MiscUtil::getHostName());
    mGlobalNodeInfo.setClientCpuTotal(SysUsage::getCpuTotal());
    mGlobalNodeInfo.setClientMemTotal(SysUsage::getMemTotal());
    mSysUsage.updateNetIO();
}

void
ClientReceiverFb::Impl::setClientMessage(const std::string& msg)
{
    mClientMessage = msg;
    updateCpuMemUsage();
    updateNetIO();
}

bool
ClientReceiverFb::Impl::decodeProgressiveFrame(const mcrt::ProgressiveFrame &message,
                                               const bool doParallel,
                                               const CallBackStartedCondition& callBackFuncAtStartedCondition,
                                               const CallBackGenericComment& callBackFuncForGenericComment)
{
    if (mDecodeProgressiveFrameCounter == 0) {
        // very first progressiveFrame message decoding
        mElapsedTimeFromStart.start();  // initialize frame start time
    }
    mDecodeProgressiveFrameCounter++; // never reset during sessions.

    updateCpuMemUsage();
    updateNetIO();

    if (message.mHeader.mProgress < 0.0f) {
        // Special case, this message only contains auxInfo data (no image information).
        for (const mcrt::BaseFrame::DataBuffer &buffer: message.mBuffers) {
            mRecvMsgSize += buffer.mDataLength;

            if (!decodeProgressiveFrameBuff(buffer)) {
                return false;
            }
        }
        afterDecode(callBackFuncForGenericComment);

        if (mFrameId > 0) {
            unsigned int currSyncId = mGlobalNodeInfo.getNewestBackEndSyncId();
            if (mFrameId < currSyncId) {
                // This is not a very first render and 
                // already started new syncId at the back-end. So we reset progress value
                // (We should not reset fb here. Reset fb creates bad effects for interactive
                // camera updates like black frame flicker for example.)
                //
                // Whenever you send any restart related message to the back-end, the back-end engine sends
                // back renderPrep start stats with new syncId to the client when back-end starts renderPrep
                // execution.
                // This start renderPrep stats arrives to the client 2 ways.
                //   1) before receiving the first image from the back-end. Usually this happens if
                //      renderPrep is heavy. or
                //   2) arrive together within the very first image.
                // This code checks situation 1. and set progress to 0.0 if we find some of the back-end
                // engines start a new frame condition. This is useful when renderPrep requires a pretty
                // long time. We can reset progress before the initial image arrives.
                // In some cases, after reseting the mProgress to 0.0, there is a possibility to receive
                // an old syncId image due to asynchronous rendering logic under multi-machine configuration.
                // If so, the received old image sets current progress value to old progress value even though
                // we already know the new frame is starting somewhere in the back-end engine. This is an edge
                // case and might not be common. Still resetting to 0.0 here would be a better idea (even if
                // there is some weird situation). Because wrong old progress value by old image is immediately
                // overwritten by next renderPrep stats or new image.
                // Situation 2 would be processed later code.
                mProgress = 0.0f;
            }
        } else {
            if (mTelemetryDisplay.getActive()) {
                if (mStatus == mcrt::BaseFrame::FINISHED) {
                    // We set progress as 0.0 when telemetry overlay is enable.
                    // If so, client app try to display image data.
                    mProgress = 0.0f;
                }
            }
        }
        return true;
    }

    // Store received message's sender machineId for debugging purpose
    mRecvImgSenderMachineId = message.mMachineId;

    // progress value is always correct If the progressiveFrame message includes image data. And we should
    // update mProgress here. mStatus is only updated with image data. (progressiveFrame without image does
    // not include proper mStatus information)
    mProgress = message.mHeader.mProgress;
    mFbActivityCounter++;

    mRecvImageDataFps.set();    // update recvImageDataFps condition

    mViewId = message.mHeader.mViewId;
    mFrameId = message.mHeader.mFrameId; // syncId of this image
    if (mLastFrameId != mFrameId) {
        mElapsedTimeFromStart.start();  // initialize frame start time
    }
    mStatus = message.mHeader.mStatus;

    mCoarsePassStatus = message.mCoarsePassStatus;
    if (mStatus == mcrt::BaseFrame::STARTED) {
        // We only update Albedo/Normal input information at frame START timing.
        // Because there is no possibility to change this information during rendering.
        mDenoiserAlbedoInputName = message.mDenoiserAlbedoInputName;
        mDenoiserNormalInputName = message.mDenoiserNormalInputName;
    }
    mSnapshotStartTime = message.mSnapshotStartTime;
    {
        uint64_t startTimeAdjusted = convertTimeBackend2Client(mSnapshotStartTime);
        mCurrentLatencySec = scene_rdl2::grid_util::LatencyItem::getLatencySec(startTimeAdjusted);
    }

    mRezedViewport =
        scene_rdl2::math::
        Viewport(message.getRezedViewport().minX(),
                 message.getRezedViewport().minY(),
                 message.getRezedViewport().maxX(),
                 message.getRezedViewport().maxY());

    if (message.hasViewport()) {
        mRoiViewportStatus = true;
        mRoiViewport =
            scene_rdl2::math::
            Viewport(message.getViewport().minX(),
                     message.getViewport().minY(),
                     message.getViewport().maxX(),
                     message.getViewport().maxY());
    } else {
        mRoiViewportStatus = false;
    }

    //------------------------------

    if (mRezedViewport != mFb.getRezedViewport()) {
        mFb.init(mRezedViewport);
    }

    mcrt::BaseFrame::Status currStatus = message.getStatus();
    if (currStatus == mcrt::BaseFrame::STARTED) {
        if (mFrameId != mLastFrameId) {
            if (mResetFbWithColorMode) {
                mFb.reset();
            } else {
                mFb.resetExceptColor();
            }
            callBackFuncAtStartedCondition();
        }
    }

    mLastFrameId = mFrameId;

    //
    // decode buffer data from message
    //
    mRecvMsgSize = 0;
    if (!doParallel) {
        for (const mcrt::BaseFrame::DataBuffer &buffer: message.mBuffers) {
            mRecvMsgSize += buffer.mDataLength;

            if (!decodeProgressiveFrameBuff(buffer)) {
                return false;
            }
        }

    } else {
        std::vector<const mcrt::BaseFrame::DataBuffer*> bufferArray;
        for (const auto &buffer: message.mBuffers) {
            mRecvMsgSize += buffer.mDataLength;
            bufferArray.push_back(&buffer);
        }
        if (bufferArray.size()) {
            tbb::blocked_range<size_t> range(0, bufferArray.size());
            bool error = false;
            tbb::parallel_for(range, [&](const tbb::blocked_range<size_t> &r) {
                    for (size_t id = r.begin(); id < r.end(); ++id) {
                        if (!decodeProgressiveFrameBuff(*bufferArray[id])) {
                            error = true;
                            return;
                        }
                    }
                });
            if (error) return false;
        }
    }
    afterDecode(callBackFuncForGenericComment);
    return true;
}

ClientReceiverFb::BackendStat
ClientReceiverFb::Impl::getBackendStat() const
{
    switch (mGlobalNodeInfo.getNodeStat()) {
    case GlobalNodeInfo::NodeStat::IDLE :
        return ClientReceiverFb::BackendStat::IDLE;
    case GlobalNodeInfo::NodeStat::RENDER_PREP_RUN :
        return ClientReceiverFb::BackendStat::RENDER_PREP_RUN;
    case GlobalNodeInfo::NodeStat::RENDER_PREP_CANCEL :
        return ClientReceiverFb::BackendStat::RENDER_PREP_CANCEL;
    case GlobalNodeInfo::NodeStat::MCRT :
        return ClientReceiverFb::BackendStat::MCRT;
    default :
        return ClientReceiverFb::BackendStat::UNKNOWN;
    }
}

float
ClientReceiverFb::Impl::getElapsedSecFromStart()
{
    if (mElapsedTimeFromStart.isInit()) {
        // This API and decodeProgressiveFrame() should be called from the same thread.
        // This is a safety logic for the case of calling this API before executing
        // decodeProgressiveFrame().
        mElapsedTimeFromStart.start();
    }
    return mElapsedTimeFromStart.end(); // return sec
}

const std::string &
ClientReceiverFb::Impl::getRenderOutputName(const unsigned id) const
{
    static const std::string nullStr("");

    scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
    if (!mFb.getAov2(id, fbAov)) {
        return nullStr;
    }
    return fbAov->getAovName();
}

int
ClientReceiverFb::Impl::getRenderOutputNumChan(const unsigned id) const
{
    scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
    if (!mFb.getAov2(id, fbAov)) {
        return 0;
    }
    return fbAov->getNumChan();
}

int    
ClientReceiverFb::Impl::getRenderOutputNumChan(const std::string& aovName) const
{
    scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
    if (!mFb.getAov2(aovName, fbAov)) {
        return 0;
    }
    return fbAov->getNumChan();
}

bool
ClientReceiverFb::Impl::getRenderOutputClosestFilter(const unsigned id) const
{
    scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
    if (!mFb.getAov2(id, fbAov)) {
        return false;
    }
    return fbAov->getClosestFilterStatus();
}

bool
ClientReceiverFb::Impl::getRenderOutputClosestFilter(const std::string& aovName) const
{
    scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
    if (!mFb.getAov2(aovName, fbAov)) {
        return false;
    }
    return fbAov->getClosestFilterStatus();
}

bool
ClientReceiverFb::Impl::getBeautyRgb888(std::vector<unsigned char>& rgbFrame,
                                        const bool top2bottom,
                                        const bool isSrgb)
{
    bool telemetryOverlayWithPrevArchive = false;
    if (getProgress() == 0.0f) {
        // This situation only be happened under telemetryOverlay active condition.
        // Not received image data yet for this frame. We only update telemetryOverlay info
        telemetryOverlayWithPrevArchive = true;
        if (mFrameId == 0) {
            // This is before receiving any images
            telemetry::DisplayInfo info;
            setupTelemetryDisplayInfo(info);
            mTelemetryDisplay.bakeOverlayRgb888(rgbFrame, top2bottom, info, telemetryOverlayWithPrevArchive);
            return true;
        }
    }

    initErrorMsg();

    bool result = true;
    if (mBeautyDenoiseMode == DenoiseMode::DISABLE) {
        getBeautyRgb888NoDenoise(rgbFrame, top2bottom, isSrgb);
    } else {
        bool fallback;
#       ifdef VERIFY_FLOAT_API_BY_UC
        std::vector<float> work;
        result = runDenoise(4, work,
                            top2bottom,
                            [&, top2bottom](std::vector<float>& buff) {
                                getBeautyNoDenoise(buff, top2bottom);
                            },
                            fallback);
        if (fallback) getBeautyNoDenoise(work, top2bottom);
        mFb.conv888Beauty(work, isSrgb, rgbFrame);
#       else // else VERIFY_FLOAT_API_BY_UC        
        result = runDenoise888(rgbFrame, top2bottom, isSrgb,
                               [&, top2bottom](std::vector<float>& buff) {
                                   getBeautyNoDenoise(buff, top2bottom);
                               },
                               fallback);
        if (fallback) getBeautyRgb888NoDenoise(rgbFrame, top2bottom, isSrgb);
#       endif // end !VERIFY_FLOAT_API_BY_UC
    }

    if (mTelemetryDisplay.getActive()) {
        telemetry::DisplayInfo info;
        setupTelemetryDisplayInfo(info);
        mTelemetryDisplay.bakeOverlayRgb888(rgbFrame, top2bottom, info, telemetryOverlayWithPrevArchive);
    }
    
    return result;
}

void
ClientReceiverFb::Impl::getBeautyRgb888NoDenoise(std::vector<unsigned char>& rgbFrame,
                                                 const bool top2bottom,
                                                 const bool isSrgb)
{
    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderBuffer(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderBuffer();
        }
    }

#   ifdef VERIFY_FLOAT_API_BY_UC
    std::vector<float> work;
    // We test untileBeauty() here instead of untileBeautyRGB() because untileBeautyRGB() is
    // easily tested by getRenderOutputRgb888() with proper "beauty" AOV definition
    mFb.untileBeauty(top2bottom,  ((mRoiViewportStatus)? &mRoiViewport: nullptr), work);
    mFb.conv888Beauty(work, isSrgb, rgbFrame);
#   else  // else VERIFY_FLOAT_API_BY_UC
    mFb.untileBeauty(isSrgb, top2bottom,
                     ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                     rgbFrame);
#   endif // end !VERIFY_FLOAT_API_BY_UC
}

bool
ClientReceiverFb::Impl::getPixelInfoRgb888(std::vector<unsigned char>& rgbFrame,
                                           const bool top2bottom,
                                           const bool isSrgb)
{
    initErrorMsg();

    if (!mFb.getPixelInfoStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolatePixelInfo(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                     mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolatePixelInfo();
        }
    }

#   ifdef VERIFY_FLOAT_API_BY_UC
    std::vector<float> work;
    mFb.untilePixelInfo(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), work);
    mFb.conv888PixelInfo(work, isSrgb, rgbFrame);
#   else  // else VERIFY_FLOAT_API_BY_UC
    mFb.untilePixelInfo(isSrgb, top2bottom,
                        ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                        rgbFrame);
#   endif // end !VERIFY_FLOAT_API_BY_UC

    return true;
}

bool
ClientReceiverFb::Impl::getHeatMapRgb888(std::vector<unsigned char>& rgbFrame,
                                         const bool top2bottom,
                                         const bool isSrgb)
{
    initErrorMsg();

    if (!mFb.getHeatMapStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateHeatMap(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                   mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateHeatMap();
        }
    }

#   ifdef VERIFY_FLOAT_API_BY_UC
    std::vector<float> work;
    mFb.untileHeatMap(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), work);
    mFb.conv888HeatMap(work, isSrgb, rgbFrame);
#   else  // else VERIFY_FLOAT_API_BY_UC
    mFb.untileHeatMap(isSrgb, top2bottom,
                      ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                      rgbFrame);
#   endif  // end !VERIFY_FLOAT_API_BY_UC

    return true;
}

bool
ClientReceiverFb::Impl::getWeightBufferRgb888(std::vector<unsigned char>& rgbFrame,
                                              const bool top2bottom,
                                              const bool isSrgb)
{
    initErrorMsg();

    if (!mFb.getWeightBufferStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateWeightBuffer(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateWeightBuffer();
        }
    }

#   ifdef VERIFY_FLOAT_API_BY_UC
    std::vector<float> work;
    mFb.untileWeightBuffer(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), work);
    mFb.conv888WeightBuffer(work, isSrgb, rgbFrame);
#   else // else VERIFY_FLOAT_API_BY_UC
    mFb.untileWeightBuffer(isSrgb, top2bottom,
                           ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                           rgbFrame);
#   endif // end !VERIFY_FLOAT_API_BY_UC

    return true;
}

bool
ClientReceiverFb::Impl::getBeautyAuxRgb888(std::vector<unsigned char>& rgbFrame,
                                           const bool top2bottom,
                                           const bool isSrgb)
{
    initErrorMsg();

    if (!mFb.getRenderBufferOddStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderBufferOdd(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                           mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderBufferOdd();
        }
    }

#   ifdef VERIFY_FLOAT_API_BY_UC
    std::vector<float> work;
    // We test untileBeautyOdd() here instead of untileBeautyAux() because untileBeautyAux() is
    // easily tested by getRenderOutputRgb888() with proper "beautyAUX" AOV definition
    mFb.untileBeautyOdd(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), work);
    mFb.conv888BeautyOdd(work, isSrgb, rgbFrame);
#   else // else VERIFY_FLOAT_API_BY_UC
    mFb.untileBeautyAux(isSrgb, top2bottom,
                        ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                        rgbFrame);
#   endif // end !VERIFY_FLOAT_API_BY_UC

    return true;
}

bool
ClientReceiverFb::Impl::getRenderOutputRgb888(const unsigned id,
                                              std::vector<unsigned char>& rgbFrame,
                                              const bool top2bottom,
                                              const bool isSrgb,
                                              const bool closestFilterDepthOutput)
{
    initErrorMsg();

    if (mFb.getTotalRenderOutput() <= id) return false; // no AOV

    bool denoise = false;
    if (mBeautyDenoiseMode != DenoiseMode::DISABLE &&
        mFb.isBeautyRelatedAov(id) &&
        !closestFilterDepthOutput) {
        denoise = true;
    }
        
    bool result = true;
    if (!denoise) {
        getRenderOutputRgb888NoDenoise(id, rgbFrame, top2bottom, isSrgb, closestFilterDepthOutput);
    } else {
        // beauty related AOV with denoise
        bool fallback;
#       ifdef VERIFY_FLOAT_API_BY_UC
        std::vector<float> work;
        result = runDenoise(3, work,
                            top2bottom,
                            [&, top2bottom](std::vector<float>& buff) {
                                getRenderOutputF4(id, buff, top2bottom, closestFilterDepthOutput);
                            },
                            fallback);
        if (fallback) getRenderOutputNoDenoise(id, work, top2bottom, closestFilterDepthOutput);
        mFb.conv888BeautyRGB(work, isSrgb, rgbFrame);
#       else // else VERIFY_FLOAT_API_BY_UC        
        result = runDenoise888(rgbFrame, top2bottom, isSrgb,
                               [&, id, top2bottom, closestFilterDepthOutput](std::vector<float>& buff) {
                                   getRenderOutputF4(id, buff, top2bottom, closestFilterDepthOutput); 
                               },
                               fallback);
        if (fallback) {
            getRenderOutputRgb888NoDenoise(id, rgbFrame, top2bottom, isSrgb, closestFilterDepthOutput);
        }
#       endif // end !VERIFY_FLOAT_API_BY_UC
    }
    return result;
}

void
ClientReceiverFb::Impl::getRenderOutputRgb888NoDenoise(const unsigned id,
                                                       std::vector<unsigned char>& rgbFrame,
                                                       const bool top2bottom,
                                                       const bool isSrgb,
                                                       const bool closestFilterDepthOutput)
{
    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderOutput(id,
                                        mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderOutput(id);
        }
    }

#   ifdef VERIFY_FLOAT_API_BY_UC
    std::vector<float> work;
    mFb.untileRenderOutput(id,
                           top2bottom,
                           ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                           closestFilterDepthOutput,
                           work);
    mFb.conv888RenderOutput(id, work, isSrgb, closestFilterDepthOutput, rgbFrame);
#   else // else VERIFY_FLOAT_API_BY_UC
    mFb.untileRenderOutput(id, isSrgb,
                           top2bottom,
                           ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                           closestFilterDepthOutput,
                           rgbFrame);
#   endif // end !VERIFY_FLOAT_API_BY_UC
}

bool
ClientReceiverFb::Impl::getRenderOutputRgb888(const std::string& aovName,
                                              std::vector<unsigned char>& rgbFrame,
                                              const bool top2bottom,
                                              const bool isSrgb,
                                              const bool closestFilterDepthOutput)
{
    initErrorMsg();

    bool denoise = false;
    if (mBeautyDenoiseMode != DenoiseMode::DISABLE &&
        mFb.isBeautyRelatedAov(aovName) &&
        !closestFilterDepthOutput) {
        denoise = true;
    }

    bool result = true;
    if (!denoise) {
        getRenderOutputRgb888NoDenoise(aovName, rgbFrame, top2bottom, isSrgb, closestFilterDepthOutput);
    } else {
        // beauty related AOV with denoise
        bool fallback;
#       ifdef VERIFY_FLOAT_API_BY_UC
        std::vector<float> work;
        result = runDenoise(3, work,
                            top2bottom,
                            [&, top2bottom](std::vector<float>& buff) {
                                getRenderOutputF4(aovName, buff, top2bottom, closestFilterDepthOutput);
                            },
                            fallback);
        if (fallback) getRenderOutputNoDenoise(aovName, work, top2bottom, closestFilterDepthOutput);
        mFb.conv888BeautyRGB(work, isSrgb, rgbFrame);
#       else // else VERIFY_FLOAT_API_BY_UC
        result = runDenoise888(rgbFrame, top2bottom, isSrgb,
                               [&, aovName, top2bottom, closestFilterDepthOutput](std::vector<float>& buff) {
                                   getRenderOutputF4(aovName, buff, top2bottom, closestFilterDepthOutput); 
                               },
                               fallback);
        if (fallback) {
            getRenderOutputRgb888NoDenoise(aovName, rgbFrame, top2bottom, isSrgb, closestFilterDepthOutput);
        }
#       endif // end !VERIFY_FLOAT_API_BY_UC
    }
    return result;
}

void
ClientReceiverFb::Impl::getRenderOutputRgb888NoDenoise(const std::string& aovName,
                                                       std::vector<unsigned char>& rgbFrame,
                                                       const bool top2bottom,
                                                       const bool isSrgb,
                                                       const bool closestFilterDepthOutput)
{
    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderOutput(aovName,
                                        mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderOutput(aovName);
        }
    }

#   ifdef VERIFY_FLOAT_API_BY_UC
    std::vector<float> work;
    mFb.untileRenderOutput(aovName,
                           top2bottom,
                           ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                           closestFilterDepthOutput,
                           work);
    mFb.conv888RenderOutput(aovName, work, isSrgb, closestFilterDepthOutput, rgbFrame);
#   else // else VERIFY_FLOAT_API_BY_UC
    mFb.untileRenderOutput(aovName, isSrgb,
                           top2bottom,
                           ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                           closestFilterDepthOutput,
                           rgbFrame);
#   endif // end !VERIFY_FLOAT_API_BY_UC
}
    
//------------------------------------------------------------------------------

bool
ClientReceiverFb::Impl::getBeauty(std::vector<float>& rgba,
                                  const bool top2bottom)
{
    initErrorMsg();

    bool result = true;
    if (mBeautyDenoiseMode == DenoiseMode::DISABLE) {
        getBeautyNoDenoise(rgba, top2bottom);
    } else {
        bool fallback;
        result = runDenoise(4, rgba,
                            top2bottom,
                            [&, top2bottom](std::vector<float>& buff) {
                                getBeautyNoDenoise(buff, top2bottom);
                            },
                            fallback);
        if (fallback) getBeautyNoDenoise(rgba, top2bottom);
    }
    return result;
}

void
ClientReceiverFb::Impl::getBeautyNoDenoise(std::vector<float>& rgba,
                                           const bool top2bottom)
{
    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderBuffer(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderBuffer();
        }
    }

    mFb.untileBeauty(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), rgba);
}

bool
ClientReceiverFb::Impl::getPixelInfo(std::vector<float>& data,
                                     const bool top2bottom)
{
    initErrorMsg();

    if (!mFb.getPixelInfoStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolatePixelInfo(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                     mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolatePixelInfo();
        }
    }

    mFb.untilePixelInfo(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), data);

    return true;
}

bool    
ClientReceiverFb::Impl::getHeatMap(std::vector<float>& data,
                                   const bool top2bottom)
{
    initErrorMsg();

    if (!mFb.getHeatMapStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateHeatMap(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                   mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateHeatMap();
        }
    }

    mFb.untileHeatMap(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), data);

    return true;
}

bool
ClientReceiverFb::Impl::getWeightBuffer(std::vector<float>& data,
                                        const bool top2bottom)
{
    initErrorMsg();

    if (!mFb.getWeightBufferStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateWeightBuffer(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateWeightBuffer();
        }
    }

    mFb.untileWeightBuffer(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), data);

    return true;
}

bool
ClientReceiverFb::Impl::getBeautyOdd(std::vector<float>& rgba,
                                     const bool top2bottom)
{
    initErrorMsg();

    if (!mFb.getRenderBufferOddStatus()) return false;

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderBufferOdd(mRoiViewport.mMinX, mRoiViewport.mMinY,
                                           mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderBufferOdd();
        }
    }

    mFb.untileBeautyOdd(top2bottom, ((mRoiViewportStatus)? &mRoiViewport: nullptr), rgba);

    return true;
}

int    
ClientReceiverFb::Impl::getRenderOutput(const unsigned id,
                                        std::vector<float>& data,
                                        const bool top2bottom,
                                        const bool closestFilterDepthOutput)
{
    initErrorMsg();

    if (mFb.getTotalRenderOutput() <= id) return 0; // no AOV

    bool denoise = false;
    if (mBeautyDenoiseMode != DenoiseMode::DISABLE &&
        mFb.isBeautyRelatedAov(id) &&
        !closestFilterDepthOutput) {
        denoise = true;
    }

    if (!denoise) {
        return getRenderOutputNoDenoise(id, data, top2bottom, closestFilterDepthOutput);
    } else {
        // beauty related AOV (BEAUTY or BEAUTY_AUX) with denoise. channel total is 3 in this case.
        bool fallback;
        bool result = runDenoise(3, data,
                                 top2bottom,
                                 [&, top2bottom](std::vector<float>& buff) {
                                     getRenderOutputF4(id, buff, top2bottom, closestFilterDepthOutput);
                                 },
                                 fallback);
        if (fallback) getRenderOutputNoDenoise(id, data, top2bottom, closestFilterDepthOutput);
        return result ? 3 : -1;        // -1 is error
    }
}

int    
ClientReceiverFb::Impl::getRenderOutputNoDenoise(const unsigned id,
                                                 std::vector<float>& data,
                                                 const bool top2bottom,
                                                 const bool closestFilterDepthOutput)
{
    if (mFb.getTotalRenderOutput() <= id) return 0; // no AOV

    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderOutput(id,
                                        mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderOutput(id);
        }
    }

    return mFb.untileRenderOutput(id,
                                  top2bottom,
                                  ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                                  closestFilterDepthOutput,
                                  data);
}

int    
ClientReceiverFb::Impl::getRenderOutput(const std::string& aovName,
                                        std::vector<float>& data,
                                        const bool top2bottom,
                                        const bool closestFilterDepthOutput)
{
    initErrorMsg();

    bool denoise = false;
    if (mBeautyDenoiseMode != DenoiseMode::DISABLE &&
        mFb.isBeautyRelatedAov(aovName) &&
        !closestFilterDepthOutput) {
        denoise = true;
    }

    if (!denoise) {
        return getRenderOutputNoDenoise(aovName, data, top2bottom, closestFilterDepthOutput);
    } else {
        // beauty related AOV (BEAUTY or BEAUTY_AUX) with denoise. channel total is 3 in this case.
        bool fallback;
        bool result = runDenoise(3, data,
                                 top2bottom,
                                 [&, top2bottom](std::vector<float>& buff) {
                                     getRenderOutputF4(aovName, buff, top2bottom, closestFilterDepthOutput);
                                 },
                                 fallback);
        if (fallback) getRenderOutputNoDenoise(aovName, data, top2bottom, closestFilterDepthOutput);
        return result ? 3 : -1; // -1 is error
    }
}

int    
ClientReceiverFb::Impl::getRenderOutputNoDenoise(const std::string& aovName,
                                                 std::vector<float>& data,
                                                 const bool top2bottom,
                                                 const bool closestFilterDepthOutput)
{
    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderOutput(aovName,
                                        mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderOutput(aovName);
        }
    }

    return mFb.untileRenderOutput(aovName,
                                  top2bottom,
                                  ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                                  closestFilterDepthOutput,
                                  data);
}

int    
ClientReceiverFb::Impl::getRenderOutputF4(const unsigned id,
                                          std::vector<float>& data,
                                          const bool top2bottom,
                                          const bool closestFilterDepthOutput)
{
    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderOutput(id,
                                        mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderOutput(id);
        }
    }

    return mFb.untileRenderOutputF4(id,
                                    top2bottom,
                                    ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                                    closestFilterDepthOutput,
                                    data);
}

int    
ClientReceiverFb::Impl::getRenderOutputF4(const std::string& aovName,
                                          std::vector<float>& data,
                                          const bool top2bottom,
                                          const bool closestFilterDepthOutput)
{
    if (mCoarsePassStatus != 1) {
        // need extrapolation
        if (mRoiViewportStatus) {
            mFb.extrapolateRenderOutput(aovName,
                                        mRoiViewport.mMinX, mRoiViewport.mMinY,
                                        mRoiViewport.mMaxX, mRoiViewport.mMaxY);
        } else {
            mFb.extrapolateRenderOutput(aovName);
        }
    }

    return mFb.untileRenderOutputF4(aovName,
                                    top2bottom,
                                    ((mRoiViewportStatus)? &mRoiViewport: nullptr),
                                    closestFilterDepthOutput,
                                    data);
}

scene_rdl2::math::Vec4f
ClientReceiverFb::Impl::getPixBeauty(const int sx, const int sy) const
{
    return mFb.getPixRenderBuffer(sx, sy);
}

float
ClientReceiverFb::Impl::getPixPixelInfo(const int sx, const int sy) const
{
    return mFb.getPixPixelInfo(sx, sy);
}

float
ClientReceiverFb::Impl::getPixHeatMap(const int sx, const int sy) const
{
    return mFb.getPixHeatMap(sx, sy);
}

float
ClientReceiverFb::Impl::getPixWeightBuffer(const int sx, const int sy) const
{
    return mFb.getPixWeightBuffer(sx, sy);
}

scene_rdl2::math::Vec4f
ClientReceiverFb::Impl::getPixBeautyOdd(const int sx, const int sy) const
{
    return mFb.getPixRenderBufferOdd(sx, sy);
}

int
ClientReceiverFb::Impl::getPixRenderOutput(const unsigned id,
                                           const int sx,
                                           const int sy,
                                           std::vector<float>& out) const
{
    scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
    if (!mFb.getAov2(id, fbAov)) {
        return 0; // no expected data
    }
    return fbAov->getPix(sx, sy, out);
}

int
ClientReceiverFb::Impl::getPixRenderOutput(const std::string& aovName,
                                           const int sx,
                                           const int sy,
                                           std::vector<float>& out) const
{
    scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
    if (!mFb.getAov2(aovName, fbAov)) {
        return 0; // no expected data
    }

    scene_rdl2::math::Vec4f rgba;
    switch (fbAov->getReferenceType()) {
    case scene_rdl2::grid_util::FbReferenceType::UNDEF :
        return fbAov->getPix(sx, sy, out);
    case scene_rdl2::grid_util::FbReferenceType::BEAUTY :
        rgba = getPixBeauty(sx, sy);
        out.resize(3);
        out[0] = rgba[0];
        out[1] = rgba[1];
        out[2] = rgba[2];
        return static_cast<int>(out.size());
    case scene_rdl2::grid_util::FbReferenceType::ALPHA :
        rgba = getPixBeauty(sx, sy);
        out.resize(1);
        out[0] = rgba[3];
        return static_cast<int>(out.size());
    case scene_rdl2::grid_util::FbReferenceType::HEAT_MAP :
        out.resize(1);
        out[0] = getPixHeatMap(sx, sy);
        return static_cast<int>(out.size());
    case scene_rdl2::grid_util::FbReferenceType::WEIGHT :
        out.resize(1);
        out[0] = getPixWeightBuffer(sx, sy);
        return static_cast<int>(out.size());
    case scene_rdl2::grid_util::FbReferenceType::BEAUTY_AUX :
        rgba = getPixBeautyOdd(sx, sy);
        out.resize(3);
        out[0] = rgba[0];
        out[1] = rgba[1];
        out[2] = rgba[2];
        return static_cast<int>(out.size());
    case scene_rdl2::grid_util::FbReferenceType::ALPHA_AUX :
        rgba = getPixBeautyOdd(sx, sy);
        out.resize(1);
        out[0] = rgba[3];
        return static_cast<int>(out.size());
    default : break;
    }

    return 0;
}

std::string
ClientReceiverFb::Impl::showPix(const int sx, const int sy, const std::string& aovName) const
//
// Return pixel value detailed information by string
//
{
    auto showHead = [](const std::string& msg, const int sx, const int sy) {
        std::ostringstream ostr;
        ostr << msg << " (sx:" << std::setw(4) << sx << ", sy:" << std::setw(4) << sy << ")";
        return ostr.str();
    };
    auto showVal = [](const float v) {
        union { float f; unsigned ui; } uni;
        auto showAsF = [](const float v) {
            std::ostringstream ostr;
            ostr << "float(" << std::setw(20) << std::fixed << std::setprecision(9) << v << ")";
            return ostr.str();
        };
        auto showAsBitImage = [&](const float v) {
            uni.f = v;
            std::ostringstream ostr;
            ostr << "bitImage(";
            for (int i = 31; i >= 0; --i) {
                ostr << ((uni.ui >> i) & 0x1);
                if (i == 16) ostr << ' ';
                else if (i != 0 && i % 4 == 0) ostr << '-';
            }
            ostr << ")";
            return ostr.str();
        };
        auto showAsUInt = [&](const float v) {
            uni.f = v;
            std::ostringstream ostr;
            ostr << "unsigned(" << std::setw(10) << uni.ui << ")";
            return ostr.str();
        };
        std::ostringstream ostr;
        // We show value by 3 different format
        ostr << showAsF(v) << " : " << showAsBitImage(v) << " : " << showAsUInt(v);
        return ostr.str();
    };
    auto showRgba = [&](const std::string& msg, const int sx, const int sy,
                        const scene_rdl2::math::Vec4f& rgba) {
        std::ostringstream ostr;
        ostr
        << showHead(msg, sx, sy) << " {\n"
        << scene_rdl2::str_util::addIndent(showVal(rgba[0])) << '\n'
        << scene_rdl2::str_util::addIndent(showVal(rgba[1])) << '\n'
        << scene_rdl2::str_util::addIndent(showVal(rgba[2])) << '\n'
        << scene_rdl2::str_util::addIndent(showVal(rgba[3])) << '\n'
        << "}";
        return ostr.str();
    };
    auto showFVec = [&](const std::string& msg, const int sx, const int sy,
                        const std::vector<float> v) {
        std::ostringstream ostr;
        ostr << showHead(msg, sx, sy) << " {\n";
        for (size_t i = 0; i < v.size(); ++i) {
            ostr << scene_rdl2::str_util::addIndent(showVal(v[i])) << '\n';
        }
        ostr << "}";
        return ostr.str();
    };
    auto showF = [&](const std::string& msg, const int sx, const int sy,
                     const float v) {
         std::vector<float> vec{ v };
         return showFVec(msg, sx, sy, vec);
    };

    //------------------------------

    if (getProgress() < 0.0) {
        return "image data has not been received yet";
    }

    std::ostringstream ostr;
    if (aovName == "*Beauty") {
        ostr << showRgba("Beauty", sx, sy, getPixBeauty(sx, sy));
    } else if (aovName == "*PixelInfo") {
        if (getPixelInfoStatus()) {
            ostr << showF("PixelInfo", sx, sy, getPixPixelInfo(sx, sy));
        } else {
            ostr << "there is no PixelInfo";
        }
    } else if (aovName == "*HeatMap") {
        ostr << showF("HeatMap", sx, sy, getPixHeatMap(sx, sy));
    } else if (aovName == "*Weight") {
        ostr << showF("Weight", sx, sy, getPixWeightBuffer(sx, sy));
    } else if (aovName == "*BeautyOdd") {
        ostr << showRgba("BeautyOdd", sx, sy, getPixBeautyOdd(sx, sy));
    } else {
        std::vector<float> vec;
        if (getPixRenderOutput(aovName, sx, sy, vec) > 0) {
            scene_rdl2::grid_util::Fb::FbAovShPtr fbAov;
            mFb.getAov2(aovName, fbAov);
            ostr << "getPixRenderOutput {\n"
                 << scene_rdl2::str_util::addIndent(fbAov->showInfo()) << '\n'
                 << scene_rdl2::str_util::addIndent(showFVec("pixValue", sx, sy, vec)) << '\n'
                 << "}";
        } else {
            ostr << showHead("unknown aov name:" + aovName, sx, sy);
        }
    }
    return ostr.str();
}

void
ClientReceiverFb::Impl::updateStatsMsgInterval()
{
    mStats.updateMsgInterval();
}

void
ClientReceiverFb::Impl::updateStatsProgressiveFrame()
{
    mStats.updateLatency(mCurrentLatencySec);
    mStats.updateRecvMsgSize(mRecvMsgSize);
}

bool
ClientReceiverFb::Impl::getStats(const float intervalSec, std::string& outMsg)
{
    uint32_t syncId = mFrameId;
    if (syncId != mLastSyncId) {
        std::ostringstream ostr;
        ostr << ">>> 1st latency:" << mCurrentLatencySec * 1000.0f << " ms syncId:" << syncId;
        outMsg = ostr.str();
        mLastSyncId = syncId;

        // Just in case
        if (mElapsedTimeFromStart.isInit()) {
            mElapsedTimeFromStart.start();
        }
    } else if (mLastGetStatsTime.end() > intervalSec) {
        std::ostringstream ostr;
        ostr << showProgress() << "% " << mStats.show(getElapsedSecFromStart());
        outMsg = ostr.str();
    } else {
        if (mLastProgress < 1.0f && getProgress() >= 1.0f) {
            std::ostringstream ostr;
            ostr << ">100%< " << mStats.show(getElapsedSecFromStart());
            outMsg = ostr.str();
            mLastProgress = getProgress();
            return true;
        }

        mLastProgress = getProgress();
        return false;           // no need display statistical info
    }

    mStats.reset();
    mLastGetStatsTime.start();
    mLastProgress = getProgress();
    return true;
}

void    
ClientReceiverFb::Impl::consoleEnable(ClientReceiverFb* fbReceiver,
                                      const unsigned short port,
                                      const CallBackSendMessage& sendMessage)
{
    mConsoleDriver.set(sendMessage, fbReceiver);
    mConsoleDriver.initialize(port);
}

void
ClientReceiverFb::Impl::setTimingRecorderHydra(std::shared_ptr<TimingRecorderHydra> ptr)
{
    mTimingRecorderHydra = ptr;
    mTimingAnalysis.setTimingRecorderHydra(ptr);
}

void
ClientReceiverFb::Impl::setTelemetryOverlayReso(unsigned width, unsigned height)
{
    /* useful debug message
    std::cerr << ">> ClientReceiverFb.cc setTelemetryOverlayReso"
              << " old(w:" << mTelemetryOverlayResoWidth << ", h:" << mTelemetryOverlayResoHeight << ") ->"
              << " new(w:" << width << ", h:" << height << ")\n";
    */
    mTelemetryOverlayResoWidth = width;
    mTelemetryOverlayResoHeight = height;

    //
    // We need to initialize internal Fb if the internal Fb is smaller than the telemetry overlay resolution.
    // This is required regarding client message telemetry display action.
    //
    unsigned telemetryMaxX = mTelemetryOverlayResoWidth - 1;
    unsigned telemetryMaxY = mTelemetryOverlayResoHeight - 1;
    scene_rdl2::math::Viewport currFbViewport = mFb.getRezedViewport();
    if (currFbViewport.mMaxX < telemetryMaxX || currFbViewport.mMaxY < telemetryMaxY) {
        unsigned maxX = std::max(static_cast<unsigned>(currFbViewport.mMaxX), telemetryMaxX);
        unsigned maxY = std::max(static_cast<unsigned>(currFbViewport.mMaxY), telemetryMaxY);
        scene_rdl2::math::Viewport newViewport(0, 0, maxX, maxY);
        mFb.init(newViewport);
    }
}

void
ClientReceiverFb::Impl::setTelemetryOverlayActive(bool sw)
{
    mTelemetryDisplay.setActive(sw);
}

bool
ClientReceiverFb::Impl::getTelemetryOverlayActive() const
{
    return mTelemetryDisplay.getActive();
}

std::vector<std::string>
ClientReceiverFb::Impl::getAllTelemetryPanelName()
{
    return mTelemetryDisplay.getAllPanelName();
}

void
ClientReceiverFb::Impl::setTelemetryInitialPanel(const std::string& panelName)
{
    mTelemetryDisplay.setTelemetryInitialPanel(panelName);
}

void
ClientReceiverFb::Impl::switchTelemetryPanelByName(const std::string& panelName)
{
    mTelemetryDisplay.switchPanelByName(panelName);
}

void
ClientReceiverFb::Impl::switchTelemetryPanelToNext()
{
    mTelemetryDisplay.switchPanelToNext();
}

void
ClientReceiverFb::Impl::switchTelemetryPanelToPrev()
{
    mTelemetryDisplay.switchPanelToPrev();
}

void
ClientReceiverFb::Impl::switchTelemetryPanelToParent()
{
    mTelemetryDisplay.switchPanelToParent();
}

void
ClientReceiverFb::Impl::switchTelemetryPanelToChild()
{
    mTelemetryDisplay.switchPanelToChild();
}

//------------------------------------------------------------------------------

void
ClientReceiverFb::Impl::addErrorMsg(const std::string& msg)
{
    if (!mErrorMsg.empty()) mErrorMsg += '\n';
    mErrorMsg += msg;
}

void
ClientReceiverFb::Impl::updateCpuMemUsage()
{
    if (mSysUsage.isCpuUsageReady()) {
        mGlobalNodeInfo.setClientCpuUsage(mSysUsage.getCpuUsage());
        mGlobalNodeInfo.setClientMemUsage(mSysUsage.getMemUsage());
    }
}

void
ClientReceiverFb::Impl::updateNetIO()
{
    if (mSysUsage.updateNetIO()) {
        mGlobalNodeInfo.setClientNetRecvBps(mSysUsage.getNetRecv());
        mGlobalNodeInfo.setClientNetSendBps(mSysUsage.getNetSend());
    }
}

bool
ClientReceiverFb::Impl::decodeProgressiveFrameBuff(const mcrt::BaseFrame::DataBuffer& buffer)
{
    if (!buffer.mDataLength) return true; // empty data somehow -> skip

    if (!std::strcmp(buffer.mName, "latencyLog")) {
        mLatencyLog.decode(buffer.mData.get(), buffer.mDataLength);

        /* useful debug dump
        static int iii = 0;
        iii++;
        if (iii > 24) {
            std::cerr << ">> DecoderComponent.cc latencyLog {" << std::endl;
            std::cerr << mLatencyLog->show("  ") << std::endl;
            std::cerr << '}' << std::endl;
            iii = 0;
        }
        */
        return true;

    } else if (!std::strcmp(buffer.mName, "latencyLogUpstream")) {
        mLatencyLogUpstream.decode(buffer.mData.get(), buffer.mDataLength);

        /* useful debug dump
        static int iii = 0;
        iii++;
        if (iii > 24) {
            std::cerr << ">> DecoderComponent.cc latencyLogUpstream {" << std::endl;
            std::cerr << mLatencyLogUpstream->show("  ") << std::endl;
            std::cerr << '}' << std::endl;
            iii = 0;
        }
        */
        return true;
    }

    if (!std::strcmp(buffer.mName, "auxInfo")) {
        decodeAuxInfo(buffer);
        return true;
    }

    //
    // PackTile codec data
    //
    scene_rdl2::fb_util::ActivePixels workActivePixels;
    scene_rdl2::grid_util::PackTiles::DataType dataType =
        scene_rdl2::grid_util::PackTiles::
        decodeDataType(static_cast<const void*>(buffer.mData.get()), buffer.mDataLength);
    bool activeDecodeAction {false};

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::BEAUTY) {
        // "beauty" buffer as RGBA mode (not include numSample)
        if (!scene_rdl2::grid_util::PackTiles::
            decode(false,
                   static_cast<const void*>(buffer.mData.get()),
                   buffer.mDataLength,
                   workActivePixels,
                   mFb.getRenderBufferTiled(), // RGBA : float * 4
                   mFb.getRenderBufferCoarsePassPrecision(),
                   mFb.getRenderBufferFinePassPrecision(),
                   activeDecodeAction)) {
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixels().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::BEAUTY_WITH_NUMSAMPLE) {
        // "beauty" buffer as RGBA mode  with "numSample" buffer
        scene_rdl2::grid_util::Fb::NumSampleBuffer dummyBuffer;
        if (!scene_rdl2::grid_util::PackTiles::
            decode(false, // renderBufferOdd condition
                   static_cast<const void*>(buffer.mData.get()),
                   buffer.mDataLength,
                   false, // storeNumSampleData condition
                   workActivePixels,
                   mFb.getRenderBufferTiled(), // RGBA : float * 4 : normalized
                   dummyBuffer, // numSampleBufferTiled
                   mFb.getRenderBufferCoarsePassPrecision(),
                   mFb.getRenderBufferFinePassPrecision(),
                   activeDecodeAction)) {
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixels().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::PIXELINFO) {
        mFb.setupPixelInfo(nullptr, buffer.mName);
        if (!scene_rdl2::grid_util::PackTiles::
            decodePixelInfo(static_cast<const void*>(buffer.mData.get()),
                            buffer.mDataLength,
                            workActivePixels,
                            mFb.getPixelInfoBufferTiled(), // Depth : float
                            mFb.getPixelInfoCoarsePassPrecision(),
                            mFb.getPixelInfoFinePassPrecision(),
                            activeDecodeAction)) {
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixelsPixelInfo().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::HEATMAP) {
        mFb.setupHeatMap(nullptr, buffer.mName);
        if (!scene_rdl2::grid_util::PackTiles::
            decodeHeatMap(static_cast<const void*>(buffer.mData.get()),
                          buffer.mDataLength,
                          workActivePixels,
                          mFb.getHeatMapSecBufferTiled(),
                          activeDecodeAction)) { // Sec : float
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixelsHeatMap().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::HEATMAP_WITH_NUMSAMPLE) {
        mFb.setupHeatMap(nullptr, buffer.mName);
        scene_rdl2::grid_util::Fb::NumSampleBuffer dummyBuffer;
        if (!scene_rdl2::grid_util::PackTiles::
            decodeHeatMap(static_cast<const void*>(buffer.mData.get()),
                          buffer.mDataLength,
                          false, // storeNumSampleData condition
                          workActivePixels,
                          mFb.getHeatMapSecBufferTiled(), // Sec : float
                          dummyBuffer,
                          activeDecodeAction)) { // heatMapNumSampleBufTiled
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixelsHeatMap().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::WEIGHT) {
        mFb.setupWeightBuffer(nullptr, buffer.mName);
        if (!scene_rdl2::grid_util::PackTiles::
            decodeWeightBuffer(static_cast<const void*>(buffer.mData.get()),
                               buffer.mDataLength,
                               workActivePixels,
                               mFb.getWeightBufferTiled(), // Weight : float
                               mFb.getWeightBufferCoarsePassPrecision(),
                               mFb.getWeightBufferFinePassPrecision(),
                               activeDecodeAction)) {
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixelsWeightBuffer().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::BEAUTYODD) {
        // Actually we don't have {coarse, fine}PassPrecision info for renderBufferOdd.
        scene_rdl2::grid_util::CoarsePassPrecision dummyCoarsePassPrecision;
        scene_rdl2::grid_util::FinePassPrecision dummyFinePassPrecision;
        mFb.setupRenderBufferOdd(nullptr);
        if (!scene_rdl2::grid_util::PackTiles::
            decode(true, // renderBufferOdd condition
                   static_cast<const void*>(buffer.mData.get()),
                   buffer.mDataLength,
                   workActivePixels,
                   mFb.getRenderBufferOddTiled(), // RGBA : float * 4
                   dummyCoarsePassPrecision,
                   dummyFinePassPrecision,
                   activeDecodeAction)) {
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixelsRenderBufferOdd().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::BEAUTYODD_WITH_NUMSAMPLE) {
        // Actually we don't have {coarse, fine}PassPrecision info for renderBufferOdd.
        scene_rdl2::grid_util::CoarsePassPrecision dummyCoarsePassPrecision;
        scene_rdl2::grid_util::FinePassPrecision dummyFinePassPrecision;
        scene_rdl2::grid_util::Fb::NumSampleBuffer dummyBuffer;
        mFb.setupRenderBufferOdd(nullptr);
        if (!scene_rdl2::grid_util::PackTiles::
            decode(true, // renderBufferOdd condition
                   static_cast<const void*>(buffer.mData.get()),
                   buffer.mDataLength,
                   false, // storeNumSampleData condition
                   workActivePixels,
                   mFb.getRenderBufferOddTiled(), // RGBA : float * 4
                   dummyBuffer, // numSampleBufferTiled
                   dummyCoarsePassPrecision,
                   dummyFinePassPrecision,
                   activeDecodeAction)) {
            return false;
        }
        if (activeDecodeAction) {
            if (!mFb.getActivePixelsRenderBufferOdd().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    if (dataType == scene_rdl2::grid_util::PackTiles::DataType::REFERENCE) {
        // RenderOutput AOV reference type (Beauty, Alpha, HeatMap, Weight)
        scene_rdl2::grid_util::Fb::FbAovShPtr fbAov = mFb.getAov(buffer.mName); // MTsafe
        if (!scene_rdl2::grid_util::PackTiles::
            decodeRenderOutputReference(static_cast<const void*>(buffer.mData.get()),
                                        buffer.mDataLength,
                                        fbAov)) { // done fbAov memory setup if needed
            return false;
        }
        return true;
    }

    if (dataType != scene_rdl2::grid_util::PackTiles::DataType::UNDEF) {
        // RenderOutput AOV.
        scene_rdl2::grid_util::Fb::FbAovShPtr fbAov = mFb.getAov(buffer.mName); // MTsafe
        if (!scene_rdl2::grid_util::PackTiles::
            decodeRenderOutput(static_cast<const void*>(buffer.mData.get()),
                               buffer.mDataLength,
                               false, // storeNumSampleData
                               workActivePixels,
                               fbAov,
                               activeDecodeAction)) { // done fbAov memory setup if needed
            return false;
        }
        if (activeDecodeAction) {
            // update activePixels info by OR bitmask operation
            if (!fbAov->getActivePixels().orOp(workActivePixels)) {
                return false;
            }
        }
        return true;
    }

    return true;
}

void
ClientReceiverFb::Impl::decodeAuxInfo(const mcrt::BaseFrame::DataBuffer& buffer)
{
    scene_rdl2::rdl2::ValueContainerDeq cDeq(buffer.mData.get(), buffer.mDataLength);
    std::vector<std::string> infoDataArray = cDeq.deqStringVector();
    for (size_t i = 0; i < infoDataArray.size(); ++i) {
        if (!mGlobalNodeInfo.decode(infoDataArray[i])) {
            std::cerr << ">> ClientReceiverFb.cc decodeAuxInfo() mGlobalNodeInfo.decode() failed\n"
                      << "infoDataArray[i:" << i << "]"
                      << "(size:" << infoDataArray[i].size() << ")"
                      << ">" << infoDataArray[i] << "<\n";
        }
    }

    if (!mGlobalNodeInfo.getMergeHostName().empty()) {
        if (!mClockDeltaRun) {
            if (!mGlobalNodeInfo.clockDeltaClientMainAgainstMerge()) {
                std::cerr << ">> ClientReceiverFb.cc decodeAuxInfo()"
                          << " clockDeltaClientAgainstMerge failed" << std::endl;
            }
            mClockDeltaRun = true;
        }
    }
    
    // useful debug message
    // std::cerr << ">> ClientReceiverFb.cc " << mGlobalNodeInfo.show() << '\n'
}

void
ClientReceiverFb::Impl::afterDecode(const CallBackGenericComment& callBackFuncForGenericComment)
{
    // This is relatively light weight and no serious impact to the decode performance.
    mRenderPrepProgress = mGlobalNodeInfo.getRenderPrepProgress();

    processGenericComment(callBackFuncForGenericComment);
    infoRecUpdate();

    if (mRenderPrepDetailedProgressDump) {
        renderPrepDetailedProgress(); // for debug
    }

    { // for debug
        if (mGlobalNodeInfo.getMcrtTotal() > mShowMcrtTotal) {
            std::cerr << mGlobalNodeInfo.showAllHostsName() << '\n';
            mShowMcrtTotal = mGlobalNodeInfo.getMcrtTotal();
        }
    }    
}

void
ClientReceiverFb::Impl::processGenericComment(const CallBackGenericComment& callBackFuncForGenericComment)
//
// If we have genericComment recently received, pass them into callBack function.
//    
{
    std::string genericComment = mGlobalNodeInfo.deqGenericComment();
    if (!genericComment.empty()) {
        if (callBackFuncForGenericComment) {
            callBackFuncForGenericComment(genericComment);
        }

        mConsoleDriver.showString(genericComment + '\n'); // send string to TlSvr client if TlSvr is active.
    }
}

void
ClientReceiverFb::Impl::infoRecUpdate()
{
    auto notStartAllYet = [&](GlobalNodeInfo& nodeInfo) {
        if (!mInfoRecMaster.getItemTotal()) return true; // very 1st time
        if (!nodeInfo.isMcrtAllStart()) {
            return true; // we have non active mcrt engine
        }
        return false; // all mcrt is active
    };
    auto justOnStart = [&](GlobalNodeInfo& nodeInfo) {
        // Check about start timing of rendering
        if (!mInfoRecMaster.getItemTotal()) return true; // very 1st time
        float currProgress = nodeInfo.getMergeProgress();
        float prevProgress = mInfoRecMaster.getLastRecItem()->getMergeProgress();
        if (currProgress < prevProgress) {
            return true; // probably rerender started.
        }
        return false;
    };
    auto justOnComplete = [&](GlobalNodeInfo& nodeInfo) {
        // Check about render complete timing
        if (!mInfoRecMaster.getItemTotal()) return false;
        float currProgress = nodeInfo.getMergeProgress();
        float prevProgress = mInfoRecMaster.getLastRecItem()->getMergeProgress();
        if (currProgress >= 1.0f && prevProgress < 1.0f) return true;
        return false;
    };
    auto justOnStopAll = [&](GlobalNodeInfo& nodeInfo) {
        // Check about final active mcrt engine stop timing
        if (!nodeInfo.isMcrtAllStop()) {
            return false; // we have active mcrt engine
        }
        if (!mInfoRecMaster.getItemTotal()) {
            return false; // no previous condition and assume it as stop. So this is not run to stop.
        }
        if (mInfoRecMaster.getLastRecItem()->isMcrtAllStop()) {
            return false; // Previous condition is all stop, So this is also not run to stop situation.
        }
        return true;
    };

    if (mInfoRecInterval <= 0.0f) {
        return; // disable recInfo mode, skip
    }

    bool notStartAllYetFlag = notStartAllYet(mGlobalNodeInfo);
    bool justOnStopAllFlag = justOnStopAll(mGlobalNodeInfo);
    bool justOnStartFlag = justOnStart(mGlobalNodeInfo);
    bool justOnCompleteFlag = justOnComplete(mGlobalNodeInfo); 
    if (justOnStopAllFlag) std::cerr << "STOP-ALL" << std::endl;

    float interval = mInfoRecInterval;
    if (notStartAllYetFlag) {
        interval = 0.5f; // sec
    }
    
    if (!justOnStartFlag && !justOnCompleteFlag && !justOnStopAllFlag &&
        !mInfoRecMaster.intervalCheck(interval)) {
        return; // short interval or not just on start/complete/stop timing => skip
    }
    
    //------------------------------

    infoRecUpdateDataAll();

    //------------------------------

    float progress = mGlobalNodeInfo.getMergeProgress();

    // bool always = true; // for debug
    bool always = false;
    if (always ||
        (notStartAllYetFlag && progress < 1.0) ||
        justOnStopAllFlag || justOnStartFlag || justOnCompleteFlag ||
        mDispInfoRec.end() > mInfoRecDisplayInterval) {
        std::ostringstream ostr;
        // select some of the info for runtime display
        ostr << ">> ClientReceiverFb.cc recItemTotal:" << mInfoRecMaster.getItemTotal() << '\n'
             << mInfoRecMaster.getLastRecItem()->showTable("cpu") << '\n'
             << mInfoRecMaster.getLastRecItem()->showTable("snp") << '\n'
             << mInfoRecMaster.getLastRecItem()->showTable("snd") << '\n'
             << mInfoRecMaster.getLastRecItem()->showTable("rnd") << '\n'
             << mInfoRecMaster.getLastRecItem()->showTable("rps");
        std::cerr << ostr.str() << std::endl;
        mDispInfoRec.start();
    }

    scene_rdl2::rec_time::RecTime recTime;

    if (!justOnCompleteFlag && !justOnStopAllFlag) {
        if (mLastInfoRecOut.isInit()) {
            mLastInfoRecOut.start();
        } else {
            if (mLastInfoRecOut.end() > 60.0f) { // every 60 sec
                std::cerr << "== InfoRec temp SAVE ==" << std::endl;
                recTime.start();
                mInfoRecMaster.save(mInfoRecFileName, ".iRec-A");
                std::cerr << "== InfoRec SAVE temp complete:" << recTime.end() << " sec ==" << std::endl;
                mLastInfoRecOut.start();
            }
        }
    }

    if (justOnCompleteFlag) {
        std::cerr << "== InfoRec SAVE ==" << std::endl;
        recTime.start();
        mInfoRecMaster.save(mInfoRecFileName, ".iRec-C");
        std::cerr << "== InfoRec SAVE complete:" << recTime.end() << " sec ==" << std::endl;
        mLastInfoRecOut.start();
    }

    if (justOnStopAllFlag) {
        std::cerr << "== InfoRec Final SAVE ==" << std::endl;
        recTime.start();
        mInfoRecMaster.save(mInfoRecFileName, ".iRec-F");
        mInfoRecMaster.clearItems();
        std::cerr << "== InfoRec Final SAVE complete:" << recTime.end() << " sec ==" << std::endl;
        mLastInfoRecOut.start();
    }
}

void
ClientReceiverFb::Impl::infoRecUpdateDataAll()
{
    infoRecUpdateGlobal();

    InfoRecMaster::InfoRecItemShPtr recItem = mInfoRecMaster.newRecItem();
    infoRecUpdateClient(recItem);
    infoRecUpdateMerge(recItem);
    infoRecUpdateAllNodes(recItem);
}

void
ClientReceiverFb::Impl::infoRecUpdateGlobal()
{
    InfoRecGlobal &recGlobal = mInfoRecMaster.getGlobal();

    if (!recGlobal.isDispatchSet()) {
        recGlobal.setDispatch(mGlobalNodeInfo.getDispatchHostName(),
                              0, // cpuTotal : unknown (We don't have info)
                              0); // memTotal : unknown (We don't have info)
    }

    if (!recGlobal.isMergeSet()) {
        recGlobal.setMerge(mGlobalNodeInfo.getMergeHostName(),
                           mGlobalNodeInfo.getMergeCpuTotal(),
                           mGlobalNodeInfo.getMergeMemTotal());
    }
}

void
ClientReceiverFb::Impl::infoRecUpdateClient(InfoRecMaster::InfoRecItemShPtr recItem)
{
    recItem->setClient(mCurrentLatencySec,
                       mGlobalNodeInfo.getClientClockTimeShift());
}

void
ClientReceiverFb::Impl::infoRecUpdateMerge(InfoRecMaster::InfoRecItemShPtr recItem)
{
    recItem->setMerge(mGlobalNodeInfo.getMergeCpuUsage(),
                      mGlobalNodeInfo.getMergeMemUsage(),
                      mGlobalNodeInfo.getMergeRecvBps(),
                      mGlobalNodeInfo.getMergeSendBps(),
                      mGlobalNodeInfo.getMergeProgress());
    if (mGlobalNodeInfo.getMergeFeedbackActive()) {
        recItem->setMergeFeedbackOn(mGlobalNodeInfo.getMergeFeedbackInterval(), // sec
                                    mGlobalNodeInfo.getMergeEvalFeedbackTime(), // millisec
                                    mGlobalNodeInfo.getMergeSendFeedbackFps(),  // fps
                                    mGlobalNodeInfo.getMergeSendFeedbackBps()); // Byte/Sec
    } else {
        recItem->setMergeFeedbackOff();
    }
}

void
ClientReceiverFb::Impl::infoRecUpdateAllNodes(InfoRecMaster::InfoRecItemShPtr recItem)
{
    InfoRecGlobal& recGlobal = mInfoRecMaster.getGlobal();
    mGlobalNodeInfo.crawlAllMcrtNodeInfo([&](GlobalNodeInfo::McrtNodeInfoShPtr mcrtNodeInfo) {
            int mId = mcrtNodeInfo->getMachineId();
            if (mId >= 0) {
                // Just in case, we only accept mId = 0 or positive.
                // Actually, mId < 0 is a user error and mostly this situation happens under a single mcrt
                // configuration when user forgot to setup machineId inside sessiondef configuration file.
                if (!recGlobal.isMcrtSet(mId)) {
                    recGlobal.setMcrt(mId,
                                      mcrtNodeInfo->getHostName(),
                                      mcrtNodeInfo->getCpuTotal(),
                                      mcrtNodeInfo->getMemTotal());
                }
                recItem->setMcrt(mId,
                                 mcrtNodeInfo->getCpuUsage(),
                                 mcrtNodeInfo->getMemUsage(),
                                 mcrtNodeInfo->getSnapshotToSend(),
                                 mcrtNodeInfo->getSendBps(),
                                 mcrtNodeInfo->getRenderActive(),
                                 static_cast<int>(mcrtNodeInfo->getRenderPrepStats().stage()),
                                 mcrtNodeInfo->getProgress(),
                                 mcrtNodeInfo->getClockTimeShift());
                if (mcrtNodeInfo->getFeedbackActive()) {
                    recItem->setMcrtFeedbackOn(mId,
                                               mcrtNodeInfo->getFeedbackInterval(), // sec
                                               mcrtNodeInfo->getRecvFeedbackFps(),  // fps
                                               mcrtNodeInfo->getRecvFeedbackBps(),  // Byte/Sec
                                               mcrtNodeInfo->getEvalFeedbackTime(), // millisec
                                               mcrtNodeInfo->getFeedbackLatency()); // millisec
                } else {
                    recItem->setMcrtFeedbackOff(mId);
                }
            }
            return true;
        });
}

uint64_t
ClientReceiverFb::Impl::convertTimeBackend2Client(const uint64_t backendTimeUSec)
{
    float clockOffsetMs = mGlobalNodeInfo.getClientClockTimeShift(); // millisec
    return backendTimeUSec + (uint64_t)(clockOffsetMs * 1000.0f);
}

std::string
ClientReceiverFb::Impl::showProgress() const
{
    float v = getProgress() * 100.0f;
    std::ostringstream ostr;
    ostr << std::setw(5) << std::fixed << std::setprecision(2) << v;
    return ostr.str();
}

void
ClientReceiverFb::Impl::renderPrepDetailedProgress()
// This is for the debugging purpose function
{
    unsigned oldestSyncId = mGlobalNodeInfo.getOldestBackEndSyncId();
    if (mRenderPrepDetailedProgressShowLastSyncId != oldestSyncId) {
        mRenderPrepDetailedProgressShowLastSyncId = oldestSyncId;
        mRenderPrepDetailedProgressShowCompleteCount = 0; // reset counter
    }

    if (mGlobalNodeInfo.isMcrtAllRenderPrepCompletedOrCanceled()) {
        mRenderPrepDetailedProgressShowCompleteCount++;
        if (mRenderPrepDetailedProgressShowCompleteCount > 1) {
            return;
        }
        // We want to show very first all completed condition status.
    }

    if (mRenderPrepDetailedProgressDumpMode == 0) {
        std::cerr << getRenderPrepProgress() << '\n';
    } else {
        std::cerr << mGlobalNodeInfo.showRenderPrepStatus() << '\n';
    }
}

bool
ClientReceiverFb::Impl::denoiseAlbedoInputCheck(const DenoiseMode mode, const std::string& inputAovName)
{
    return ((mode == DenoiseMode::ENABLE_W_ALBEDO || mode == DenoiseMode::ENABLE_W_ALBEDO_NORMAL) &&
            !inputAovName.empty());
}

bool
ClientReceiverFb::Impl::denoiseNormalInputCheck(const DenoiseMode mode, const std::string& inputAovName)
{
    return ((mode == DenoiseMode::ENABLE_W_NORMAL || mode == DenoiseMode::ENABLE_W_ALBEDO_NORMAL) &&
            !inputAovName.empty());
}

bool
ClientReceiverFb::Impl::runDenoise888(std::vector<unsigned char>& rgbFrame,
                                      const bool top2bottom,
                                      const bool isSrgb,
                                      const std::function<void(std::vector<float>& buff)>& setInputCallBack,
                                      bool& fallback)
{
    if (mStatus == mcrt::BaseFrame::STARTED) {
        // We skip the denoise operation for 1st image of the frame in order to keep good interactively.
        fallback = true;
        mDenoiser.resetTimingInfo();
        return true;
    }

    static std::function<void(std::vector<float>& buff)> nullCallBack {nullptr};

    if (!mDenoiser.
        denoiseBeauty888(mDenoiseEngine,
                         mCurrentLatencySec,
                         mFb.getWidth(),
                         mFb.getHeight(),
                         ((mRoiViewportStatus) ? &mRoiViewport : nullptr),
                         setInputCallBack,
                         (!denoiseAlbedoInputCheck(mBeautyDenoiseMode, mDenoiserAlbedoInputName) ?
                          nullCallBack :
                          [&, top2bottom](std::vector<float>& buff) {
                             getRenderOutputF4(mDenoiserAlbedoInputName, buff, top2bottom, false);
                         }),
                         (!denoiseNormalInputCheck(mBeautyDenoiseMode, mDenoiserNormalInputName) ?
                          nullCallBack :
                          [&, top2bottom](std::vector<float>& buff) {
                             getRenderOutputF4(mDenoiserNormalInputName, buff, top2bottom, false);
                         }),
                         rgbFrame,
                         isSrgb,
                         fallback)) {
        addErrorMsg(mDenoiser.getErrorMsg());
        return false;
    }
    return true;
}

bool
ClientReceiverFb::Impl::runDenoise(const int outputNumChan,
                                   std::vector<float>& rgba,
                                   const bool top2bottom,
                                   const std::function<void(std::vector<float>& buff)>& setInputCallBack,
                                   bool& fallback)
{
    if (mStatus == mcrt::BaseFrame::STARTED) {
        // We skip the denoise operation for 1st image of the frame in order to keep good interactively.
        fallback = true;
        mDenoiser.resetTimingInfo();
        return true;
    }

    static std::function<void(std::vector<float>& buff)> nullCallBack {nullptr};
    
    if (!mDenoiser.
        denoiseBeauty(mDenoiseEngine,
                      mCurrentLatencySec,
                      mFb.getWidth(),
                      mFb.getHeight(),
                      ((mRoiViewportStatus) ? &mRoiViewport : nullptr),
                      setInputCallBack,
                      (!denoiseAlbedoInputCheck(mBeautyDenoiseMode, mDenoiserAlbedoInputName) ?
                       nullCallBack :
                       [&, top2bottom](std::vector<float>& buff) {
                          getRenderOutputF4(mDenoiserAlbedoInputName, buff, top2bottom, false);
                      }),
                      (!denoiseNormalInputCheck(mBeautyDenoiseMode, mDenoiserNormalInputName) ?
                       nullCallBack :
                       [&, top2bottom](std::vector<float>& buff) {
                          getRenderOutputF4(mDenoiserNormalInputName, buff, top2bottom, false);
                      }),
                      outputNumChan,
                      rgba,
                      fallback)) {
        addErrorMsg(mDenoiser.getErrorMsg());
        return false;
    }
    return true;
}

void
ClientReceiverFb::Impl::setupTelemetryDisplayInfo(telemetry::DisplayInfo& displayInfo)
{
    //
    // client message
    //
    displayInfo.mClientMessage = &mClientMessage;

    //
    // Image resolution and telemetryOverlay resolution
    //
    displayInfo.mOverlayWidth = mTelemetryOverlayResoWidth;
    displayInfo.mOverlayHeight = mTelemetryOverlayResoHeight;
    if (mProgress < 0.0f) {
        if (mFrameId > 0) {
            // This is not a very first render. So we return the previous image resolution.
            // This avoids unexpected display data buffer resize inside telemetryOverlay logic.
            displayInfo.mImageWidth = getWidth();
            displayInfo.mImageHeight = getHeight();
        } else {
            // We don't receive image resolution info yet.
            displayInfo.mImageWidth = 0;
            displayInfo.mImageHeight = 0;
        }
    } else {
        if (mFrameId == 0 && mProgress == 0.0f) {
            // This is a situation of the very first time rendering before receiving 1st progressiveFrame
            // image data under telemetryOverlay activated condition. We don't know the image resolution
            // yet but we know the telemetryOverlay resolution and it is the same as the image resolution
            // at this version. We should use this info for the image display buffer allocation. This
            // solution is safe under image resolution is not changed inside one session, however, we have
            // to reconsider changing the resolution operation. Probably display buffer size might not be
            // properly reallocated when the resolution changed. This is a future task.
            displayInfo.mImageWidth = mTelemetryOverlayResoWidth;
            displayInfo.mImageHeight = mTelemetryOverlayResoHeight;
        } else {
            displayInfo.mImageWidth = getWidth();
            displayInfo.mImageHeight = getHeight();
        }
    }

    //
    // general info
    //
    displayInfo.mViewId = getViewId();
    displayInfo.mFrameId = getFrameId();
    displayInfo.mElapsedSecFromStart = getElapsedSecFromStart(); // sec
    displayInfo.mStatus = getStatus();
    displayInfo.mRenderPrepProgress = getRenderPrepProgress();
    displayInfo.mProgress = getProgress();
    displayInfo.mFbActivityCounter = getFbActivityCounter();
    displayInfo.mDecodeProgressiveFrameCounter = mDecodeProgressiveFrameCounter;
    displayInfo.mIsCoarsePass = isCoarsePass();
    displayInfo.mCurrentLatencySec = mCurrentLatencySec;
    displayInfo.mReceiveImageDataFps = getRecvImageDataFps();

    displayInfo.mGlobalNodeInfo = &mGlobalNodeInfo;
}

void
ClientReceiverFb::Impl::parserConfigure()
{
    using scene_rdl2::str_util::boolStr;

    mParser.description("ClientReceiverFb command");

    mParser.opt("globalNodeInfo", "...command...", "globalNodeInfo command",
                [&](Arg& arg) { return mGlobalNodeInfo.getParser().main(arg.childArg()); });
    mParser.opt("renderPrepProgress", "", "show current renderPrep progress value",
                [&](Arg& arg) { return arg.msg(showRenderPrepProgress() + '\n'); });
    mParser.opt("renderPrepDetailedDump", "<bool>", "renderPrep stage detailed information dump",
                [&](Arg& arg) -> bool { mRenderPrepDetailedProgressDump = (arg++).as<bool>(0); return true; });
    mParser.opt("renderPrepDetailedDumpMode", "<mode>", "0:fraction 1:fullDump",
                [&](Arg& arg) { mRenderPrepDetailedProgressDumpMode = (arg++).as<int>(0); return true; });
    mParser.opt("denoiseInfo", "", "dump denoise information",
                [&](Arg& arg) { return arg.msg(showDenoiseInfo() + '\n'); });
    mParser.opt("denoiseEngine", "<optix|openImageDenoise|show>", "select denoise engine or show current",
                [&](Arg& arg) {
                    std::string engine = (arg++)();
                    if (engine == "optix") mDenoiseEngine = DenoiseEngine::OPTIX;
                    else if (engine == "openImageDenoise") mDenoiseEngine = DenoiseEngine::OPEN_IMAGE_DENOISE;
                    else if (engine != "show") return arg.fmtMsg("unknown engineType:%s\n", engine.c_str());
                    return arg.msg(ClientReceiverFb::showDenoiseEngine(mDenoiseEngine) + '\n');
                });
    mParser.opt("denoiseMode", "<0|1|2|3|4>", "0:off 1:on 2:on+albedo 3:on+normal 4:on+albedo+normal",
                [&](Arg& arg) {
                    switch (arg++.as<int>(0)) {
                    case 0 : mBeautyDenoiseMode = DenoiseMode::DISABLE; break;
                    case 1 : mBeautyDenoiseMode = DenoiseMode::ENABLE; break;
                    case 2 : mBeautyDenoiseMode = DenoiseMode::ENABLE_W_ALBEDO; break;
                    case 3 : mBeautyDenoiseMode = DenoiseMode::ENABLE_W_NORMAL; break;
                    case 4 : mBeautyDenoiseMode = DenoiseMode::ENABLE_W_ALBEDO_NORMAL; break;
                    default : break;
                    }
                    return arg.msg(ClientReceiverFb::showDenoiseMode(mBeautyDenoiseMode) + '\n');
                });
    mParser.opt("resetFbWithColMode", "<on|off|show>", "set or show fb reset w/ col mode",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else mResetFbWithColorMode = (arg++).as<bool>(0);
                    return arg.fmtMsg("resetFbWithColMode %s\n", boolStr(mResetFbWithColorMode).c_str());
                });
    mParser.opt("backendStat", "", "show backend computation status",
                [&](Arg& arg) { return arg.msg(ClientReceiverFb::showBackendStat(getBackendStat()) + '\n'); });
    mParser.opt("timingAnalysis", "...command...", "timingAnalysis command",
                [&](Arg& arg) { return mTimingAnalysis.getParser().main(arg.childArg()); });
    mParser.opt("viewportInfo", "", "dump viewport information",
                [&](Arg& arg) { return arg.msg(showViewportInfo() + '\n'); }); 
    mParser.opt("telemetry", "...command...", "telemetry display command",
                [&](Arg& arg) { return mTelemetryDisplay.getParser().main(arg.childArg()); });
    mParser.opt("fb", "...command...", "fb command",
                [&](Arg& arg) { return mFb.getParser().main(arg.childArg()); });
    mParser.opt("telemetryResetTest", "", "reset telemetry related info for simulation of proc start time",
                [&](Arg& arg) { telemetryResetTest(); return arg.msg("testReset done\n"); });
}

std::string
ClientReceiverFb::Impl::showDenoiseInfo() const
{
    std::ostringstream ostr;
    ostr << "denoise info {\n"
         << "  mDenoiseEngine:" << ClientReceiverFb::showDenoiseEngine(mDenoiseEngine) << '\n'
         << "  mBeautyDenoiseMode:" << ClientReceiverFb::showDenoiseMode(mBeautyDenoiseMode) << '\n'
         << "  mDenoiserAlbedoInputName:" << mDenoiserAlbedoInputName << '\n'
         << "  mDenoiserNormalInputName:" << mDenoiserNormalInputName << '\n';
    if (mBeautyDenoiseMode == DenoiseMode::DISABLE) {
        ostr << "  denoiser status info empty\n";
    } else {
        ostr << scene_rdl2::str_util::addIndent(mDenoiser.showStatus()) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
ClientReceiverFb::Impl::showRenderPrepProgress() const
{
    std::ostringstream ostr;
    ostr << "renderPrepProgress:"
         << std::setw(10) << std::fixed << std::setprecision(5) << getRenderPrepProgress();
    return ostr.str();
}

std::string
ClientReceiverFb::Impl::showViewportInfo() const
{
    auto showViewport = [](const scene_rdl2::math::Viewport& viewport) -> std::string {
        std::ostringstream ostr;
        ostr
        << "(" << viewport.mMinX << ',' << viewport.mMinY << ")-"
        << "(" << viewport.mMaxX << ',' << viewport.mMaxY << ")";
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "viewportInfo {\n"
         << "  mRezedViewport:" << showViewport(mRezedViewport) << '\n'
         << "  mRoiViewportStatus:" << scene_rdl2::str_util::boolStr(mRoiViewportStatus) << '\n';
    if (mRoiViewportStatus) {
        ostr << "  mRoiViewport:" << showViewport(mRoiViewport) << '\n';
    }
    ostr << "  mFb {\n"
         << "    getWidth():" << mFb.getWidth() << '\n'
         << "    getHeight():" << mFb.getHeight() << '\n'
         << "    getAlignedWidth():" << mFb.getAlignedWidth() << '\n'
         << "    getAlignedHeight():" << mFb.getAlignedHeight() << '\n'
         << "    getNumTilesX():" << mFb.getNumTilesX() << '\n'
         << "    getNumTilesY():" << mFb.getNumTilesY() << '\n'
         << "    getTotalTiles():" << mFb.getTotalTiles() << '\n'
         << "  }\n"
         << "}";
    return ostr.str();
}

void
ClientReceiverFb::Impl::telemetryResetTest()
//
// This function is used to reset telemetry-related parameters in order to simulate process
// boot time conditions for telemetry overlay testing.
//    
{
    mProgress = -1.0;
    mStatus = mcrt::BaseFrame::FINISHED;
}

//==========================================================================================
//==========================================================================================

ClientReceiverFb::ClientReceiverFb(bool initialTelemetryOverlayCondition)
{
    mImpl.reset(new Impl(initialTelemetryOverlayCondition));
}

ClientReceiverFb::~ClientReceiverFb()
{
}

void
ClientReceiverFb::setClientMessage(const std::string& msg)
{
    mImpl->setClientMessage(msg);
}

void
ClientReceiverFb::clearClientMessage()
{
    mImpl->clearClientMessage();
}

bool
ClientReceiverFb::decodeProgressiveFrame(const mcrt::ProgressiveFrame& message,
                                         const bool doParallel,
                                         const CallBackStartedCondition& callBackFuncAtStartedCondition,
                                         const CallBackGenericComment& callBackFuncForGenericComment)
{
    return mImpl->decodeProgressiveFrame(message, doParallel,
                                         callBackFuncAtStartedCondition, callBackFuncForGenericComment);
}
 
size_t
ClientReceiverFb::getViewId() const
{
    return mImpl->getViewId();
}

uint32_t
ClientReceiverFb::getFrameId() const
{
    return mImpl->getFrameId();
}

mcrt::BaseFrame::Status
ClientReceiverFb::getStatus() const
{
    return mImpl->getStatus();
}

ClientReceiverFb::BackendStat
ClientReceiverFb::getBackendStat() const
{
    return mImpl->getBackendStat();
}

float
ClientReceiverFb::getRenderPrepProgress() const
{
    return mImpl->getRenderPrepProgress();
}

float
ClientReceiverFb::getProgress() const
{
    return mImpl->getProgress();
}

bool
ClientReceiverFb::isCoarsePass() const
{
    return mImpl->isCoarsePass();
}

uint64_t
ClientReceiverFb::getSnapshotStartTime() const
{
    return mImpl->getSnapshotStartTime();
}

float
ClientReceiverFb::getElapsedSecFromStart()
{
    return mImpl->getElapsedSecFromStart();
}

uint64_t
ClientReceiverFb::getRecvMsgSize() const
{
    return mImpl->getRecvMsgSize(); // return last message size as byte
}

unsigned
ClientReceiverFb::getWidth() const
{
    return mImpl->getWidth();
}

unsigned
ClientReceiverFb::getHeight() const
{
    return mImpl->getHeight();
}

const scene_rdl2::math::Viewport&
ClientReceiverFb::getRezedViewport() const
{
    return mImpl->getRezedViewport(); // closed viewport
}

bool
ClientReceiverFb::getRoiViewportStatus() const
{
    return mImpl->getRoiViewportStatus();
}

const scene_rdl2::math::Viewport&
ClientReceiverFb::getRoiViewport() const
{
    return mImpl->getRoiViewport(); // closed viewport
}

bool
ClientReceiverFb::getPixelInfoStatus() const
{
    return mImpl->getPixelInfoStatus();
}

const std::string&
ClientReceiverFb::getPixelInfoName() const
{
    return mImpl->getPixelInfoName();
}

int
ClientReceiverFb::getPixelInfoNumChan() const
{
    return mImpl->getPixelInfoNumChan();
}

bool
ClientReceiverFb::getHeatMapStatus() const
{
    return mImpl->getHeatMapStatus();
}

const std::string&
ClientReceiverFb::getHeatMapName() const
{
    return mImpl->getHeatMapName();
}

int
ClientReceiverFb::getHeatMapNumChan() const
{
    return mImpl->getHeatMapNumChan();
}

bool
ClientReceiverFb::getWeightBufferStatus() const
{
    return mImpl->getWeightBufferStatus();
}

const std::string&
ClientReceiverFb::getWeightBufferName() const
{
    return mImpl->getWeightBufferName();
}

int
ClientReceiverFb::getWeightBufferNumChan() const
{
    return mImpl->getWeightBufferNumChan();
}

bool
ClientReceiverFb::getRenderBufferOddStatus() const
{
    return mImpl->getRenderBufferOddStatus();
}

int
ClientReceiverFb::getRenderBufferOddNumChan() const
{
    return mImpl->getRenderBufferOddNumChan();
}

unsigned
ClientReceiverFb::getTotalRenderOutput() const
{
    return mImpl->getTotalRenderOutput();
}

const std::string&
ClientReceiverFb::getRenderOutputName(const unsigned id) const
{
    return mImpl->getRenderOutputName(id);
}

int
ClientReceiverFb::getRenderOutputNumChan(const unsigned id) const
{
    return mImpl->getRenderOutputNumChan(id);
}

int
ClientReceiverFb::getRenderOutputNumChan(const std::string& aovName) const
{
    return mImpl->getRenderOutputNumChan(aovName);
}

bool
ClientReceiverFb::getRenderOutputClosestFilter(const unsigned id) const
{
    return mImpl->getRenderOutputClosestFilter(id);
}

bool
ClientReceiverFb::getRenderOutputClosestFilter(const std::string& aovName) const
{
    return mImpl->getRenderOutputClosestFilter(aovName);
}

//------------------------------------------------------------------------------------------

void
ClientReceiverFb::setDenoiseEngine(DenoiseEngine engine)
{
    mImpl->setDenoiseEngine(engine);
}

ClientReceiverFb::DenoiseEngine
ClientReceiverFb::getDenoiseEngine() const
{
    return mImpl->getDenoiseEngine();
}

void
ClientReceiverFb::setBeautyDenoiseMode(DenoiseMode mode)
{
    mImpl->setBeautyDenoiseMode(mode);
}

ClientReceiverFb::DenoiseMode
ClientReceiverFb::getBeautyDenoiseMode() const
{
    return mImpl->getBeautyDenoiseMode();
}

const std::string&
ClientReceiverFb::getErrorMsg() const
{
    return mImpl->getErrorMsg();
}

//------------------------------------------------------------------------------------------

bool
ClientReceiverFb::getBeautyRgb888(std::vector<unsigned char>& rgbFrame,
                                  const bool top2botton,
                                  const bool isSrgb)
{
    return mImpl->getBeautyRgb888(rgbFrame, top2botton, isSrgb);
}

bool
ClientReceiverFb::getPixelInfoRgb888(std::vector<unsigned char>& rgbFrame,
                                     const bool top2bottom,
                                     const bool isSrgb)
{
    return mImpl->getPixelInfoRgb888(rgbFrame, top2bottom, isSrgb);
}

bool
ClientReceiverFb::getHeatMapRgb888(std::vector<unsigned char>& rgbFrame,
                                   const bool top2bottom,
                                   const bool isSrgb)
{
    return mImpl->getHeatMapRgb888(rgbFrame, top2bottom, isSrgb);
}

bool
ClientReceiverFb::getWeightBufferRgb888(std::vector<unsigned char>& rgbFrame,
                                        const bool top2bottom,
                                        const bool isSrgb)
{
    return mImpl->getWeightBufferRgb888(rgbFrame, top2bottom, isSrgb);
}

bool
ClientReceiverFb::getBeautyAuxRgb888(std::vector<unsigned char>& rgbFrame,
                                     const bool top2bottom,
                                     const bool isSrgb)
{
    return mImpl->getBeautyAuxRgb888(rgbFrame, top2bottom, isSrgb);
}

bool
ClientReceiverFb::getRenderOutputRgb888(const unsigned id,
                                        std::vector<unsigned char>& rgbFrame,
                                        const bool top2bottom,
                                        const bool isSrgb,
                                        const bool closestFilterDepthOutput)
{
    return mImpl->getRenderOutputRgb888(id, rgbFrame, top2bottom, isSrgb, closestFilterDepthOutput);
}

bool
ClientReceiverFb::getRenderOutputRgb888(const std::string& aovName,
                                        std::vector<unsigned char>& rgbFrame,
                                        const bool top2bottom,
                                        const bool isSrgb,
                                        const bool closestFilterDepthOutput)
{
    return mImpl->getRenderOutputRgb888(aovName, rgbFrame, top2bottom, isSrgb, closestFilterDepthOutput);
}

bool
ClientReceiverFb::getBeauty(std::vector<float>& rgba,
                            const bool top2bottom)
// 4 channels per pixel
{
    return mImpl->getBeauty(rgba, top2bottom);
}
    
bool
ClientReceiverFb::getPixelInfo(std::vector<float>& data, const bool top2bottom)
// 1 channel per pixel
{
    return mImpl->getPixelInfo(data, top2bottom);
}

bool
ClientReceiverFb::getHeatMap(std::vector<float>& data, const bool top2bottom)
// 1 channel per pixel
{
    return mImpl->getHeatMap(data, top2bottom);
}

bool
ClientReceiverFb::getWeightBuffer(std::vector<float>& data, const bool top2bottom)
// 1 channel per pixel
{
    return mImpl->getWeightBuffer(data, top2bottom);
}

bool
ClientReceiverFb::getBeautyOdd(std::vector<float>& rgba, const bool top2bottom)
// 4 channels per pixel
{
    return mImpl->getBeautyOdd(rgba, top2bottom);
}

int
ClientReceiverFb::getRenderOutput(const unsigned id,
                                  std::vector<float>& data,
                                  const bool top2bottom,
                                  const bool closestFilterDepthOutput)
{
    return mImpl->getRenderOutput(id, data, top2bottom, closestFilterDepthOutput);
}

int
ClientReceiverFb::getRenderOutput(const std::string& aovName,
                                  std::vector<float>& data,
                                  const bool top2bottom,
                                  const bool closestFilterDepthOutput)
{
    return mImpl->getRenderOutput(aovName, data, top2bottom, closestFilterDepthOutput);
}

scene_rdl2::math::Vec4f
ClientReceiverFb::getPixBeauty(const int sx, const int sy) const
{
    return mImpl->getPixBeauty(sx, sy);
}

float
ClientReceiverFb::getPixPixelInfo(const int sx, const int sy) const
{
    return mImpl->getPixPixelInfo(sx, sy);
}

float
ClientReceiverFb::getPixHeatMap(const int sx, const int sy) const
{
    return mImpl->getPixHeatMap(sx, sy);
}

float
ClientReceiverFb::getPixWeightBuffer(const int sx, const int sy) const
{
    return mImpl->getPixWeightBuffer(sx, sy);
}

scene_rdl2::math::Vec4f
ClientReceiverFb::getPixBeautyOdd(const int sx, const int sy) const
{
    return mImpl->getPixBeautyOdd(sx, sy);
}

int
ClientReceiverFb::getPixRenderOutput(const unsigned id,
                                     const int sx,
                                     const int sy,
                                     std::vector<float>& out) const
{
    return mImpl->getPixRenderOutput(id, sx, sy, out);
}

int
ClientReceiverFb::getPixRenderOutput(const std::string& aovName,
                                     const int sx,
                                     const int sy,
                                     std::vector<float>& out) const
{
    return mImpl->getPixRenderOutput(aovName, sx, sy, out);
}

std::string
ClientReceiverFb::showPix(const int sx, const int sy, const std::string& aovName) const
{
    return mImpl->showPix(sx, sy, aovName);
}

const scene_rdl2::grid_util::LatencyLog&
ClientReceiverFb::getLatencyLog() const
{
    return mImpl->getLatencyLog();
}

const scene_rdl2::grid_util::LatencyLogUpstream&
ClientReceiverFb::getLatencyLogUpstream() const
{
    return mImpl->getLatencyLogUpstream();
}

void
ClientReceiverFb::setInfoRecInterval(const float sec)
{
    mImpl->setInfoRecInterval(sec);
}

void
ClientReceiverFb::setInfoRecDisplayInterval(const float sec)
{
    mImpl->setInfoRecDisplayInterval(sec);
}

void
ClientReceiverFb::setInfoRecFileName(const std::string& fileName)
{
    mImpl->setInfoRecFileName(fileName);
}

void
ClientReceiverFb::updateStatsMsgInterval()
{
    mImpl->updateStatsMsgInterval();
}

void
ClientReceiverFb::updateStatsProgressiveFrame()
{
    mImpl->updateStatsProgressiveFrame();
}

bool
ClientReceiverFb::getStats(const float intervalSec, std::string& outMsg)
{
    return mImpl->getStats(intervalSec, outMsg);
}

float
ClientReceiverFb::getRecvImageDataFps()
{
    return mImpl->getRecvImageDataFps();
}

unsigned
ClientReceiverFb::getFbActivityCounter()
{
    return mImpl->getFbActivityCounter();
}

void
ClientReceiverFb::consoleAutoSetup(const CallBackSendMessage& sendMessage)
{
    const char* env = std::getenv("CLIENTRECEIVER_CONSOLE");
    if (!env) return;

    unsigned short port = static_cast<unsigned short>(std::stoi(env));
    consoleEnable(port, sendMessage);

    std::cerr << "ClientReceiverConsole enable port:" << port << '\n';
}
    
void
ClientReceiverFb::consoleEnable(const unsigned short port,
                                const CallBackSendMessage& sendMessage)
{
    mImpl->consoleEnable(this, port, sendMessage);
}

ClientReceiverConsoleDriver&
ClientReceiverFb::consoleDriver()
{
    return mImpl->consoleDriver();
}

ClientReceiverFb::Parser&
ClientReceiverFb::getParser()
{
    return mImpl->getParser();
}

// static function
std::string
ClientReceiverFb::showDenoiseEngine(const DenoiseEngine& engine)
{
    switch (engine) {
    case DenoiseEngine::OPTIX : return "OPTIX";
    case DenoiseEngine::OPEN_IMAGE_DENOISE : return "OPEN_IMAGE_DENOISE";
    default : break;
    }
    return "?";
}

// static function
std::string
ClientReceiverFb::showDenoiseMode(const DenoiseMode& mode)
{
    switch (mode) {
    case DenoiseMode::DISABLE : return "DISABLE";
    case DenoiseMode::ENABLE : return "ENABLE";
    case DenoiseMode::ENABLE_W_ALBEDO : return "ENABLE_W_ALBEDO";
    case DenoiseMode::ENABLE_W_NORMAL : return "ENABLE_W_NORMAL";
    case DenoiseMode::ENABLE_W_ALBEDO_NORMAL : return "ENABLE_W_ALBEDO_NORMAL";
    default : break;
    }
    return "?";
}

// static function
std::string
ClientReceiverFb::showBackendStat(const BackendStat& stat)
{
    switch (stat) {
    case BackendStat::IDLE : return "IDLE";
    case BackendStat::RENDER_PREP_RUN : return "RENDER_PREP_RUN";
    case BackendStat::RENDER_PREP_CANCEL : return "RENDER_PREP_CANCEL";
    case BackendStat::MCRT : return "MCRT";
    case BackendStat::UNKNOWN : return "UNKNOWN";
    default : break;
    }
    return "?";
}

void
ClientReceiverFb::setTimingRecorderHydra(std::shared_ptr<TimingRecorderHydra> ptr)
{
    return mImpl->setTimingRecorderHydra(ptr);
}

int
ClientReceiverFb::getReceivedImageSenderMachineId() const
{
    return mImpl->getReceivedImageSenderMachineId();
}

// static function
std::string
ClientReceiverFb::showSenderMachineId(int machineId)
{
    if (machineId == static_cast<int>(SenderMachineId::DISPATCH)) {
        return "machineId:DISPATCH";
    } else if (machineId == static_cast<int>(SenderMachineId::MERGE)) {
        return "machineId:MERGE";
    } else if (machineId == static_cast<int>(SenderMachineId::UNKNOWN)) {
        return "machineId::UNKNOWN";
    } else {
        return std::string("machineId:") + std::to_string(machineId);
    }
}

void
ClientReceiverFb::setTelemetryOverlayReso(unsigned width, unsigned height)
{
    mImpl->setTelemetryOverlayReso(width, height);
}

void
ClientReceiverFb::setTelemetryOverlayActive(bool sw)
{
    mImpl->setTelemetryOverlayActive(sw);
}

bool
ClientReceiverFb::getTelemetryOverlayActive() const
{
    return mImpl->getTelemetryOverlayActive();
}

std::vector<std::string>
ClientReceiverFb::getAllTelemetryPanelName()
{
    return mImpl->getAllTelemetryPanelName();
}

void
ClientReceiverFb::setTelemetryInitialPanel(const std::string& panelName)
{
    mImpl->setTelemetryInitialPanel(panelName);
}

void
ClientReceiverFb::switchTelemetryPanelByName(const std::string& panelName)
{
    mImpl->switchTelemetryPanelByName(panelName);
}

void
ClientReceiverFb::switchTelemetryPanelToNext()
{
    mImpl->switchTelemetryPanelToNext();
}

void
ClientReceiverFb::switchTelemetryPanelToPrev()
{
    mImpl->switchTelemetryPanelToPrev();
}

void
ClientReceiverFb::switchTelemetryPanelToParent()
{
    mImpl->switchTelemetryPanelToParent();
}

void
ClientReceiverFb::switchTelemetryPanelToChild()
{
    mImpl->switchTelemetryPanelToChild();
}

} // namespace mcrt_dataio
