// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file ClientReceiverFb.h
///
/// @brief -- ProgressiveFrame message decoder for frontend client --
///
/// This file includes ClientReceiverFb class and it is used by frontend client.<br>
/// This ClientReceiverFb decodes ProgressiveFrame message.
/// And ClientReceiverFb keeps entire images internally and they are properly updated according to
/// the multiple ProgressiveFrame messages.<br>
/// You can retrieve current image result from ClientReceiverFb whenever you want.<br>
///
#pragma once

#include <mcrt_messages/ProgressiveFrame.h>
#include <scene_rdl2/common/grid_util/Fb.h>
#include <scene_rdl2/common/grid_util/LatencyLog.h>
#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/math/Viewport.h>
#include <scene_rdl2/common/rec_time/RecTime.h>

namespace mcrt_dataio {

class ClientReceiverConsoleDriver;
class TimingRecorderHydra;

class ClientReceiverFb
{
public:
    using CallBackStartedCondition = std::function<void()>;
    using CallBackGenericComment = std::function<void(const std::string &comment)>;
    using CallBackSendMessage = std::function<bool(const arras4::api::MessageContentConstPtr msg)>;
    using Parser = scene_rdl2::grid_util::Parser;

    enum class DenoiseEngine : int {
        OPTIX,
        OPEN_IMAGE_DENOISE    
    };

    enum class DenoiseMode : int {
        DISABLE, // disable denoise
        ENABLE, // enable denoise but does not use any additional input
        ENABLE_W_ALBEDO, // enable denoise with ALBEDO input if available
        ENABLE_W_NORMAL, // enable denoise with NORMAL input if available
        ENABLE_W_ALBEDO_NORMAL // enable denoise with ALBEDO and NORMAL input if available
    };

    enum class BackendStat : int {
        IDLE, // idle mode
        RENDER_PREP_RUN, // executing renderPrep phase
        RENDER_PREP_CANCEL, // middle of canceling renderPrep
        MCRT, // executing MCRT phase
        UNKNOWN
    };

    enum class SenderMachineId : int {
        DISPATCH = -1,
        MERGE = -2,
        UNKNOWN = -3
    };

    ClientReceiverFb(bool initialTelemetryOverlayCondition = false);
    ~ClientReceiverFb();

    /// This class is Non-copyable
    ClientReceiverFb& operator = (const ClientReceiverFb&) = delete;
    ClientReceiverFb(const ClientReceiverFb&) = delete;

    /// @brief Set client message for telemetry overlay
    /// @param msg Client message itself
    ///
    /// @detail
    /// You can set client messages for telemetry overlay. Some telemetry panels can show this message.
    /// You can update client messages multiple times at any time. This message is only used for telemetry
    /// overlay so far.
    void setClientMessage(const std::string& msg);

    /// @brief Clear client message for telemetry overlay
    ///
    /// @detail
    /// Clear current client message for telemetry overlay
    void clearClientMessage();

    //------------------------------

    /// @brief decode ProgressiveFrame message and update internal fb data accordingly.
    ///
    /// @param message message which you would like to decode
    /// @param doParallel decode by multi-threads (=true) or single-thread (=false)
    /// @param callbackFuncAtStartedCondition callback function pointer which calls when received message has "STARTED" condition
    /// @return decode succeeded (true) or error (false)
    ///
    /// @detail
    /// This function decodes input message and update internal framebuffer data accordingly.
    /// You have to call this function always you received ProgressiveFrame message by received order.
    /// If you don't decode some of the ProgressiveFrame message or wrong order of messages,
    /// internal framebuffer data is not updated properly and image might be broken. 
    /// If you get this situation somehow, only solution to solve and back to normal condision is
    /// "re-rendering from scratch".
    ///
    bool decodeProgressiveFrame(const mcrt::ProgressiveFrame& message,
                                const bool doParallel,
                                const CallBackStartedCondition& callBackFuncAtStartedCondition,
                                const CallBackGenericComment& callBackForGenericComment = nullptr);

    //------------------------------

    /// @brief Get View id of current image
    /// @return View id (= ID of the view, for supporting multiple views) of current image
    ///
    /// @detail
    /// progmcrt computation does not support View id yet. Always returns 0.
    size_t getViewId() const;

    /// @brief Get frame id of current image
    /// @return Frame id (= Unique ID per session) of current image
    ///
    /// @detail
    /// This API returns frame id (a.k.a sync id) of current image.<br>
    /// However current implementation is still experimental. Just use for debugging so far.
    uint32_t getFrameId() const;

    /// @brief Get current image's status
    /// @return image status
    ///
    /// @detail
    /// This API returns current image status as following condition.
    /// <table>
    ///   <tr>
    ///     <th>returned status</th>
    ///     <th>description</th>
    ///   </tr>
    ///   <tr>
    ///     <td>mcrt::BaseFrame::STARTED</td>
    ///     <td>The first frame of a progressive render</td>
    ///   </tr>
    ///   <tr>
    ///     <td>mcrt::BaseFrame::RENDERING</td>
    ///     <td>On going status of a progressive refining frame</td>
    ///   </tr>
    ///   <tr>
    ///     <td>mcrt::BaseFrame::FINISHED</td>
    ///     <td>The last frame of a progressive render</td>
    ///   </tr>
    ///   <tr>
    ///     <td>mcrt::BaseFrame::CANCELLED</td>
    ///     <td>Render was cancelled. Image may be incomplete</td>
    ///   </tr>
    ///   <tr>
    ///     <td>mcrt::BaseFrame::ERROR</td>
    ///     <td>Some error condition</td>
    ///   </tr>
    /// </table>
    mcrt::BaseFrame::Status getStatus() const;

    /// @brief
    /// @return backend computation running status
    ///
    /// @detail
    /// Return backend computation's running status.
    /// <table>
    ///   <tr>
    ///     <th>returned status</th>
    ///     <th>description</th>
    ///   </tr>
    ///   <tr>
    ///      <td>BackendStat::IDLE</td>
    ///      <td>idling and ready to process the next message</td>
    ///   </tr>
    ///   <tr>
    ///      <td>BackendStat::RENDER_PREP_RUN</td>
    ///      <td>backend computation is executing renderPrep stage</td>
    ///   </tr>
    ///   <tr>
    ///      <td>BackendStat::RENDER_PREP_CANCEL</td>
    ///      <td>backend computation is middle of canceling renderPrep stage</td>
    ///   </tr>
    ///   <tr>
    ///      <td>BackendStat::MCRT</td>
    ///      <td>backend computation is executing MCRT stage</td>
    ///   </tr>
    /// </table>
    BackendStat getBackendStat() const;

    /// @brief Get current image's renderPrep stage progress fraction value.
    /// @return RenderPrep stage progress fraction as 0.0 ~ 1.0.
    ///
    /// @detail
    /// Return renderPrep stage progress fraction value as 0.0 ~ 1.0. 0.0 is renderPrep is just starting now
    /// and 1.0 indicates completed. Progress fraction is computed based on the renderPrep internal task steps
    /// and fraction value is not reflected actual execution timing. 
    /// Basically, progress fraction value is simply increased.
    /// However, in some specific conditions, the progress fraction might decrease in the middle. But finally
    /// will reach 1.0.
    /// Under multi-machine configuration, MCRT stage might be able to start before renderPrep progress
    /// fraction reached to 1.0. 
    /// And, you might be able to see the rendering images even renderPrep progress value is less than 1.0.
    /// This is not a bug due to the charactaristics of a multi-machine environment.
    /// We are not syncing all the back-end engines and might be happend some mcrt computation send back
    /// images earlier than others. In this case, another back-end engine is still in the renderPrep stage
    /// and overall renderPrep progress fraction might not be reached to 1.0 yet.
    float getRenderPrepProgress() const;

    /// @brief Get current image's progress fraction value.
    /// @return Frame progress fraction as 0.0 ~ 1.0. 0.0 is just start render now and 1.0 is image completed.
    float getProgress() const;

    /// @brief Make sure current image is coarse pass or not.
    /// @return True is image is coarse pass. False is not.
    bool isCoarsePass() const;

    /// @brief Get time of snapshot was executed on MCRT computation
    /// @return Microseconds since the Epoch time (1970-01-01 00:00:00 +0000 (UTC))
    uint64_t getSnapshotStartTime() const;

    /// @brief Get elapsed time from snapshot was executed on MCRT computation to current
    /// @return Second
    /// @detail
    /// This API and decodeProgressiveFrame() should be called from same thread.
    float getElapsedSecFromStart(); // return sec

    /// @brief Return last received ProgressiveFrame message size
    /// @return Byte
    uint64_t getRecvMsgSize() const; // return last message size as byte

    /// @brief Get rezed viewport width
    /// @return Width pixel size
    unsigned getWidth() const;

    /// @brief Get rezed viewport height
    /// @return Height pixel size
    unsigned getHeight() const;

    /// @brief Get rezed viewport
    /// @return Rezed viewport as closed viewport
    const scene_rdl2::math::Viewport& getRezedViewport() const; // closed viewport

    /// @brief Get ROI viewport condition.
    /// @return Ture when ROI viewport is set otherwise false
    bool getRoiViewportStatus() const;

    /// @brief Get ROI viewport if it's set. If there is no ROI viewport, return useless info.
    /// @return ROI viewport as closed viewport if ROI viewport is setup.
    const scene_rdl2::math::Viewport& getRoiViewport() const; // closed viewport

    /// @brief Get PixelInfo data condition
    /// @return If image includes PixelInfo data, return true. Otherwise false.
    bool getPixelInfoStatus() const;

    /// @brief Get PixelInfo data AOV buffer name if image has PixelInfo data.
    /// @return PixelInfo data's AOV buffer name if image has PixelInfo data.
    ///         If image does not have PixelInfo data, just return useless info.
    const std::string& getPixelInfoName() const;

    /// @brief Get PixelInfo data's total channel number
    /// @return PixelInfo data's total channel number.
    int getPixelInfoNumChan() const;

    /// @brief Get HeatMap data condition
    /// @return If image includes HeatMap data, return true. Otherwise false.
    bool getHeatMapStatus() const;

    /// @brief Get HeatMap data AOV buffer name if image has HeatMap data.
    /// @return HeatMap data's AOV buffer name if image has HeatMap data.
    ///         If image does not have HeatMap data, just return useless info.
    const std::string& getHeatMapName() const;

    /// @brief Get HeatMap data's total channel number
    /// @return PixelInfo data's total channel number.
    int getHeatMapNumChan() const;

    /// @brief Get Weight buffer data condition
    /// @return If image includes Weight buffer data, return true. Otherwise false.
    bool getWeightBufferStatus() const;

    /// @brief Get Weight buffer data AOV buffer name if image has Weight buffer data.
    /// @return Weight buffer data's AOV buffer name if image has Weight buffer data.
    ///         If image does not have Weight buffer data, just return useless info.
    const std::string& getWeightBufferName() const;

    /// @brief Get Weight buffer data's total channel number
    /// @return Weight buffer data's total channel number.
    int getWeightBufferNumChan() const;

    /// @brief Get RenderBufferOdd data condition
    /// @return If image includes RenderBufferOdd data, return true. Otherwise false.
    bool getRenderBufferOddStatus() const;

    /// @brief Get RenderBufferOdd data's total channel number
    /// @return RenderBufferOdd data's total channel number
    int getRenderBufferOddNumChan() const;

    /// @brief Get total number of RenderOutput (AOV) of this image.
    /// @return Total number of RenderOutput (AOV) buffer of this image.
    unsigned getTotalRenderOutput() const;

    /// @brief Get AOV buffer name
    /// @param id AOV index (< total number of RenderOutput).
    /// @return AOV buffer name
    const std::string& getRenderOutputName(const unsigned id) const;

    /// @brief Get AOV buffer's channel total
    /// @param id AOV index (< total number of RenderOutput).
    /// @return AOV buffer's channel total
    int getRenderOutputNumChan(const unsigned id) const;

    /// @brief Get AOV buffer's channel total
    /// @param aovName AOV buffer name
    /// @return AOV buffer's channel total
    int getRenderOutputNumChan(const std::string& aovName) const;

    /// @brief Get AOV buffer's closestFilter use status
    /// @param id AOV index (< total number of RenderOutput).
    /// @return AOV buffer's closestFilter use status
    bool getRenderOutputClosestFilter(const unsigned id) const;

    /// @brief Get AOV buffer's closestFilter use status
    /// @param aovName AOV buffer name
    /// @return AOV buffer's closestFilter use status
    bool getRenderOutputClosestFilter(const std::string& aovName) const;

    //------------------------------

    /// @brief Setup denoise engine mode for denoise operation
    /// @param engine Denoise engine mode
    void setDenoiseEngine(DenoiseEngine engine);

    /// @brief Get current denoise engine mode for denoise operation
    /// @return current denoise engine mode
    DenoiseEngine getDenoiseEngine() const;

    /// @brief Setup denoise mode for the beauty data.
    /// @param mode Denoise mode
    ///
    /// @detail
    /// Setup denoise mode for the beauty data. This denoiseMode effects to getBeautyRgb888() and getBeauty().
    /// Also affects to getRenderOutputRgb888() and getRenderOutput() if specified AOV target (id or aovName)
    /// is BEAUTY buffer.
    /// There are 5 different DenoiseMode so far.
    /// DenoiseMode::DISABLE : 
    /// - disable the denoise operation (this is the default)
    /// DenoiseMode::ENABLE : 
    /// - enable the denoise operation without any additional guided input
    /// DenoiseMode::ENABLE_W_ALBEDO : 
    /// - enable denoise operation with ALBEDO_INPUT if it is available. 
    /// - If your scene's RenderOutput definition does not have ALBEDO_INPUT,
    ///   this mode is the same as DenoiseMode::ENABLE
    /// DenoiseMode::ENABLE_W_NORMAL : 
    /// - enable denoise operation with NORMAL_INPUT if it is available.
    /// - If your scene's RenderOutput definition does not have NORMAL_INPUT, this mode is the same as
    ///   DenoiseMode::ENABLE
    /// DenoiseMode::ENABLE_W_ALBEDO_NORMAL :
    /// - enable denoise operation with ALBEDO_INPUT and NORMAL_INPUT if they are available.
    /// - if only available ALBEDO_INPUT, it is the same as DenoiseMode::ENABLE_W_ALBEDO.
    /// - if only available NORMAL_INPUT, it is the same as DenoiseMode::ENABLE_W_NORMAL.
    /// - if both ALBEDO and NORMAL are not available, this is the same as DenoiseMode::ENABLE.
    /// Please check scene_rdl2's RenderOutput denoiser_input command for how to specify ALBEDO_INPUT and
    /// NORMAL_INPUT inside rdla RenderOutput.
    void setBeautyDenoiseMode(DenoiseMode mode);

    /// @brief Get denoise mode for the beauty data
    /// @return current denoise mode for beauty data
    DenoiseMode getBeautyDenoiseMode() const;

    /// @brief Get error message
    ///
    /// @defail
    /// The get related APIs might leave error messages internally when they return an error status.
    /// For example, getBeauty() API returns false if the internal denoise operation failed and leaves
    /// the error messages. This function gets these error messages for the caller.
    /// The error message will clear at the next get* API call.
    const std::string& getErrorMsg() const;

    //------------------------------

    /// @brief Get current beauty buffer data as 8bit RGB 3 channels w/ gamma2.2 or sRGB conversion
    /// @param rgbFrame returned current beauty buffer which is gamma2.2 or sRGB conversion.
    /// @param top2bottom specify output data Y direction order.
    /// @param isSrgb specify sRGB 8bit quantize mode.
    /// @return Return true if properly get beauty information. 
    ///         Return false if an error happened inside the denoise operation but try to return data
    ///         by falling back to non-denoise mode.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgbFrame is automatically adjusted by image size and total size of rgbFrame is
    /// getWidth() * getHeight() * 3 byte <br>
    /// Internally this API does not do rgbFrame.clear() but do rgbFrame.resize() by proper image size
    /// everytime you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// rgbFrame's data format is R G B R G B ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    /// If you set isSrgb = false (this is default), 8bit quantized logic is using gamma 2.2 conversion.
    /// If you set isSrgb argument as true, 8bit quantized logic changed from gamma 2.2 to sRGB mode.
    bool getBeautyRgb888(std::vector<unsigned char>& rgbFrame,
                         const bool top2bottom = false,
                         const bool isSrgb = false);

    /// @brief Get current PixelInfo buffer data as 8bit monochrome color RGB 3 channels.
    /// @param rgbFrame returned current PixelInfo buffer which properly converted depth info to monochrome image.
    /// @param top2bottom specify output data Y direction order.
    /// @return return true if image has PixelInfo buffer. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgbFrame is automatically adjusted by image size and total size of rgbFrame is
    /// getWidth() * getHeight() * 3 byte <br>
    /// Internally this API does not do rgbFrame.clear() but do rgbFrame.resize() by proper image size
    /// everytime you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// PixelInfo data (i.e. pixel center depth data) is converted to monochrome color value.
    /// Most closest depth value inside entire image goes to while (1,1,1) and max depth value goes to
    /// black (0,0,0).
    /// And depth value between min and max is converted to somewhere inbetween white and black.<br>
    /// rgbFrame's data format is R G B R G B ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    bool getPixelInfoRgb888(std::vector<unsigned char>& rgbFrame,
                            const bool top2bottom = false,
                            const bool isSrgb = false);

    /// @brief Get current HeatMap buffer data as 8bit RGB 3 channels.
    /// @param rgbFrame returned current HeatMap buffer which properly converted to color image.
    /// @param top2bottom specify output data Y direction order.
    /// @return return true if image has HeatMap buffer. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgbFrame is automatically adjusted by image size and total size of rgbFrame is
    /// getWidth() * getHeight() * 3 byte <br>
    /// Internally this API does not do rgbFrame.clear() but do rgbFrame.resize() by proper image size
    /// everytime you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// HeatMap data (= each pixels computational cost data) is converted to color value.
    /// Most lightest computational cost pixel goes to blue(0,0,1) and heavyest cost pixel goes to
    /// red(1,0,0).
    /// And cost between lightest and heavyest is converted to somewhere between blue and red.<br>
    /// rgbFrame's data format is R G B R G B ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    bool getHeatMapRgb888(std::vector<unsigned char>& rgbFrame,
                          const bool top2bottom = false,
                          const bool isSrgb = false);

    /// @brief Get current Weight buffer data as 8bit RGB 3 channels.
    /// @param rgbFrame returned current Weight buffer which properly converted to color image.
    /// @param top2bottom specify output data Y direction order.
    /// @param isSrgb specify sRGB 8bit quantize mode.
    /// @return return true if image has Weight buffer. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgbFrame is automatically adjusted by image size and total size of rgbFrame is
    /// getWidth() * getHeight() * 3 byte <br>
    /// Internally this API does not do rgbFrame.clear() but do rgbFrame.resize() by proper image size
    /// everytime you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// Weight buffer data is converted to color value.
    /// Weight ZERO goes to black(0,0,0) and highest number of Weight goes to white(1,1,1).
    /// output rgbFrame is gamma 2.2 collected. If you set isSrgb as flase<br>
    /// If you set isSrgb argument as true, 8bit quantized logic changed from gamma 2.2 to sRGB mode.
    /// rgbFrame's data format is R G B R G B ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    bool getWeightBufferRgb888(std::vector<unsigned char>& rgbFrame,
                               const bool top2bottom = false,
                               const bool isSrgb = false);

    /// @brief Get current RenderBufferOdd (aka BeautyAux) data as 8bit RGB 3 channels.
    /// @param rgbFrame returned current RenderBufferOdd buffer which is gamma corrected.
    /// @param top2bottom specify output data Y direction order.
    /// @param isSrgb specify sRGB 8bit quantize mode.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgbFrame is automatically adjusted by image size and total size of rgbFrame is
    /// getWidth() * getHeight() * 3 byte <br>
    /// Internally this API does not do rgbFrame.clear() but do rgbFrame.resize() by proper image size
    /// everytime you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// rgbFrame's data format is R G B R G B ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    /// If you set isSrgb = false (this is default), 8bit quantized logic is using gamma 2.2 conversion.
    /// If you set isSrgb argument as true, 8bit quantized logic changed from gamma 2.2 to sRGB mode.
    bool getBeautyAuxRgb888(std::vector<unsigned char>& rgbFrame,
                            const bool top2bottom = false,
                            const bool isSrgb = false);

    /// @brief Get current RenderOutput(=AOV) buffer data as 8bit RGB 3 channels.
    /// @param id speficy AOV id (0 <= id < getTotalRenderOutput())
    /// @param rgbFrame returned current RenderOutput buffer which pointed by id.
    /// @param top2bottom specify output data Y direction order.
    /// @param isSrgb specify sRGB 8bit quantize mode.
    /// @param closestFilterDepthOutput convert depth info when thie AOV is using closestFilter instead data itself
    /// @return return true is image has apropriate buffer. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgbFrame is automatically adjusted by image size and total size of rgbFrame is
    /// getWidth() * getHeight() * 3 byte <br>
    /// Internally this API does not do rgbFrame.clear() but do rgbFrame.resize() by proper image size
    /// everytime you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// We have 7 different types of AOV buffer to 8bit conversion logic (might have more depend the versions).
    /// Conversion logic is automatically selected based on the type of data.
    /// Following list is how convert data to 8bit for display purpose.
    /// <table>
    ///   <tr>
    ///     <th>num of chan</th>
    ///     <th>buffer type</th>
    ///     <th>description</th>
    ///   </tr>
    ///   <tr>
    ///     <td>1</td>
    ///     <td>
    ///     Non-depth, non-heatmap and non-weight buffer<br>
    ///     (i.e. standard single float buffer, alpha, alphaAUX)<br>
    ///     </td>
    ///     <td>
    ///       Remap 0.0~1.0 to 0~255 with gamma 2.2 or sRGB correction depend on the isSrgb argument and finally converted to the monochrome color.<br>
    ///       More than 1.0 goes to 255 and negative value goes to 0.
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>1</td>
    ///     <td>
    ///       Depth related buffer<br>
    ///       Depth related AOV buffer is assumed when AOV name includes "depth" word.
    ///     </td>
    ///     <td>
    ///     Mi10nimum depth inside entire image goes to whie(1,1,1) and maximum depth goes to black(0,0,0).<br>
    ///     FL10T_MAX pixel value converted to black(0,0,0).<br>
    ///       Between min and max depth is converted somewhere inbetween white and black.<br>
    ///       So data converted monochrome color finally.<br>
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>1</td>
    ///     <td>heatmap buffer</td>
    ///     <td>
    ///     Remap heatmap min~max to blue(0,0,255)~red(255,0,0).<br>
    ///       heatmap min value means computationally most lightest pixel and max value means most expensive pixel<br>
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>1</td>
    ///     <td>weight buffer</td>
    ///     <td>Remap weight min~max to 0~255 with gamma 2.2 or sRGB correction depend on the isSrgb argument and converted to the monochrome color</td>
    ///   </tr>
    ///   <tr>
    ///     <td>2</td>
    ///     <td>double float buffer</td>
    ///     <td>
    ///       Each component values convert from 0.0~1.0 to 0~255 with gamma 2.2 or sRGB correction which defined by isSrgb argument.<br>
    ///       Negative value goes to 0 and more than 1.0 value goes to 255.<br>
    ///       1st component goes to R. 2nd component goes to G. B is always 0<br>
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>3</td>
    ///     <td>Non position related triple float buffer</td>
    ///     <td>
    ///       Each 3 component values convert from 0.0~1.0 to 0~255 with gamma 2.2 or sRGB correction which defined by isSrgb argument.<br>
    ///       Negative value converted to 0 and more than 1.0 value converted to 255.<br>
    ///     1st component value goes to R, 2nd value goes to G, 3rd value goes to B.<br>
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>3</td>
    ///     <td>
    ///       Position related buffer<br>
    ///       Position related AOV buffer is assumed when AOV name includes "position" word.
    ///     </td>
    ///     <td>
    ///     Compute bbox of whole pixels first and remap all pixel value (x,y,z) from black(0,0,0) to white(1,1,1).<br>
    ///       min bbox position goes to black and max bbox position goes to white<br>
    ///       Inf value are ignored to compute bbox<br>
    ///       Using gamma 2.2 or sRGB correction for 8bit quantization which defined by isSrgb argument.<br>
    ///                                                       1st component value goes to R, 2nd value goes to G, 3rd value goes to B.<br>
    ///     </td>
    ///   </tr>
    /// </table>
    /// rgbFrame's data format is R G B R G B ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    /// If you set isSrgb = false (this is default), 8bit quantized logic is using gamma 2.2 conversion.
    /// If you set isSrgb argument as true, 8bit quantized logic changed from gamma 2.2 to sRGB mode.
    /// If this AOV is using closestFilter and closestFilterDepthOutput is true, this function convert closestFilter's depth instead data itself.
    /// Depth value mapped from minDepth~maxDepth to 0~255.
    /// If this AOV is not using closestFilter with closestFilterDepthOutput set to true, we don't have closestFilter depth info.
    /// So closestFilterDepthOutput is automatically set false.
    bool getRenderOutputRgb888(const unsigned id,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottom = false,
                               const bool isSrgb = false,
                               const bool closestFilterDepthOutput = false);

    /// @brief Get current RenderOutput(=AOV) buffer data as 8bit RGB 3 channels.
    /// @param aovName specify AOV's buffer name.
    /// @param rgbFrame returned current RenderOutput buffer which pointed by aovName.
    /// @param top2bottom specify output data Y direction order.
    /// @param isSrgb specify sRGB 8bit quantize mode.
    /// @param closestFilterDepthOutput convert depth info when thie AOV is using closestFilter instead data itself
    /// @return return true is image has apropriate buffer. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgbFrame is automatically adjusted by image size and total size of rgbFrame is
    /// getWidth() * getHeight() * 3 byte <br>
    /// Internally this API does not do rgbFrame.clear() but do rgbFrame.resize() by proper image size
    /// everytime you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// We have 7 different types of AOV buffer to 8bit conversion logic (might have more depend the versions).
    /// Conversion logic is automatically selected based on the type of data.
    /// (see previous getRenderOutputRgb888() function's comments for more detail).
    /// rgbFrame's data format is R G B R G B ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    /// If you set isSrgb = false (this is default), 8bit quantized logic is using gamma 2.2 conversion.
    /// If you set isSrgb argument as true, 8bit quantized logic changed from gamma 2.2 to sRGB mode.
    /// If this AOV is using closestFilter and closestFilterDepthOutput is true, this function convert closestFilter's depth instead data itself.
    /// Depth value mapped from minDepth~maxDepth to 0~255.
    /// If this AOV is not using closestFilter with closestFilterDepthOutput set to true, we don't have closestFilter depth info.
    /// So closestFilterDepthOutput is automatically set false.
    bool getRenderOutputRgb888(const std::string& aovName,
                               std::vector<unsigned char>& rgbFrame,
                               const bool top2bottom = false,
                               const bool isSrgb = false,
                               const bool closestFilterDepthOutput = false);

    //------------------------------

    /// @brief Get current beauty buffer data as float (=32bit single float) RGBA 4 channels
    /// @param rgba returned current beauty buffer as original data.
    /// @param top2bottom specify output data Y direction order.
    /// @return Return true if properly get beauty information. 
    ///         Return false if an error happened inside the denoise operation but try to return data
    ///         by falling back to non-denoise mode.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgba is automatically adjusted by image size and total size of rgba is
    /// getWidth() * getHeight() * 4 * sizeof(float) byte <br>
    /// Internally this API does not do rgba.clear() but do rgba.resize() by proper image size everytime
    /// you call this API.<br>
    /// If current image condition is coarse pass, output rgba is properly extrapolated.<br>
    /// rgba's data format is R G B A R G B A ... from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, return rgba is flipped Y direction.
    bool getBeauty(std::vector<float>& rgba,
                   const bool top2bottom = false); // 4 channels per pixel

    /// @brief Get current PixelInfo buffer data as float (=32bit single float) 1 channel data.
    /// @param data returned current PixelInfo buffer.
    /// @param top2bottom specify output data Y direction order.
    /// @return return true if image has PixelInfo buffer. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// data is automatically adjusted by image size and total size of data is
    /// getWidth() * getHeight() * sizeof(float) byte <br>
    /// Internally this API does not do data.clear() but do data.resize() by proper image size everytime
    /// you call this API.<br>
    /// If current image condition is coarse pass, output data is properly extrapolated.<br>
    /// PixelInfo data is just copied into data without any conversion and data might includes +infinity
    /// value.<br>
    /// data's format is lined up from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, return data is flipped Y direction.
    bool getPixelInfo(std::vector<float>& data, const bool top2bottom = false); // 1 channel per pixel

    /// @brief Get current HeatMap buffer data as float (=32bit single float) 1 channel data.
    /// @param data returned current HeatMap buffer.
    /// @param top2bottom specify output data Y direction order.
    /// @return return true if image has HeatMap buffer. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// data is automatically adjusted by image size and total size of data is
    /// getWidth() * getHeight() * sizeof(float) byte <br>
    /// Internally this API does not do data.clear() but do data.resize() by proper image size everytime
    /// you call this API.<br>
    /// If current image condition is coarse pass, output data is properly extrapolated.<br>
    /// Returned data is each pixel's computational cost and unit is second.<br>
    /// data's data format is lined up from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, return data is flipped Y direction.
    bool getHeatMap(std::vector<float>& data, const bool top2bottom = false);   // 1 channel per pixel

    /// @brief Get current weight buffer data as float (=32bit single float) 1 channel data.
    /// @param data returned current weight buffer.
    /// @param top2bottom specify output data Y direction order.
    /// @return return true if image has Weight buffer data. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// data is automatically adjusted by image size and total size of data is
    /// getWidth() * getHeight() * sizeof(float) byte <br>
    /// Internally this API does not do data.clear() but do data.resize() by proper image size everytime
    /// you call this API.<br>
    /// If current image condition is coarse pass, output data is properly extrapolated.<br>
    /// data's format is lined up from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, return data is flipped Y direction.
    bool getWeightBuffer(std::vector<float>& data, const bool top2bottom = false); // 1 channel per pixel

    /// @brief Get current RenderBufferOdd data as float (=32bit single float) RGBA 4 channels
    /// @param rgba returned current RenderBufferOdd data buffer.
    /// @param top2bottom specify output data Y direction order.
    /// @return return true if image has RenderBufferOdd data. false if image does not.
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// rgba is automatically adjusted by image size and total size of data is
    /// getWidth() * getHeight() * sizeof(float) * 4 byte <br>
    /// internally this API does not do data.clear() but do data.resize() by proper image size everytime
    /// you call this API. <br>
    /// If current image condition is coarse pass, output data is properly extrapolated.<br>
    /// data's format is lined up from bottom scanline to top scanline and
    /// left pixel to right pixel for each scanline when you set top2bottom as false.<br>
    /// If you set top2bottom as true, return data is flipped Y direction.
    bool getBeautyOdd(std::vector<float>& rgba, const bool top2bottom = false); // 4 channels per pixel

    /// @brief Get current RenderOutput(=AOV) buffer data
    /// @param id specify AOV id (0 <= id < getTotalRenderOutput())
    /// @param data returned current RenderOutput buffer which pointed by id.
    /// @param top2bottom specify output data Y direction order.
    /// @param closestFilterDepthOutput convert depth info when thie AOV is using closestFilter instead data itself
    /// @return return channel total number. 0 implies no data. -1 is error and you can get error message by
    ///         getErrorMsg().
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// data is automatically adjusted by image size with channel number and total size of data is
    /// getWidth() * getHeight() * sizeof(float) * number-of-channels byte <br>
    /// Internally this API does not do data.clear() but do data.resize() by proper image size everytime
    /// you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// number-of-channels are changed based on AOV buffer type. We have 4 possibilities of AOV buffer
    /// configuration now. 1 float to 4 float buffers. In any case, AOV buffer channel total number is returned
    /// as return value of this API.<br>
    /// If this API implies no data. This means there is no apropriate data which associated to id,
    /// AOV data itself is simply just copies 0x0 into data without any conversion as 32bit single float.
    /// data's formats are as follows.
    /// <table>
    ///   <tr>
    ///     <th>channel configuration</th>
    ///     <th>description</th>
    ///   </tr>
    ///   <tr>
    ///     <td>1 float</td>
    ///     <td>
    ///       data's format is V ... <br>
    ///       from bottom scanline to top scanline and left pixel to right pixel for each scanline
    ///       when you set top2bottom as false.<br>
    ///       If you set top2bottom as true, return data is flipped Y direction.
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>2 floats</td>
    ///     <td>
    ///       data's format is Vx Vy Vx Vy ... <br>
    ///       from bottom scanline to top scanline and left pixel to right pixel for each scanline
    ///       when you set top2bottom as false.<br>
    ///       If you set top2bottom as true, return data is flipped Y direction.
    ///       If this aov uses closestFilter, last component (i.e. Vy) is closestFilter's depth value.
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>3 floats</td>
    ///     <td>
    ///       data's format is Vx Vy Vz Vx Vy Vz ... <br>
    ///       from bottom scanline to top scanline and left pixel to right pixel for each scanline
    ///       when you set top2bottom as false.<br>
    ///       If you set top2bottom as true, return data is flipped Y direction.
    ///       If this aov uses closestFilter, last component (i.e. Vz) is closestFilter's depth value.
    ///     </td>
    ///   </tr>
    ///   <tr>
    ///     <td>4 floats</td>
    ///     <td>
    ///       data's format is Vx Vy Vz Vw Vx Vy Vz Vw ... <br>
    ///       from bottom scanline to top scanline and left pixel to right pixel for each scanline
    ///       when you set top2bottom as false.<br>
    ///       If you set top2bottom as true, return data is flipped Y direction.<br>
    ///       Currently all of the float4 data AOV is related closestFilter and. this is
    ///       original_3_float_data + closestFilter_depth.
    ///     </td>
    ///   </tr>
    /// </table>
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    /// If this AOV is using closestFilter and closestFilterDepthOutput is true, this function convert closestFilter's depth instead data itself.
    /// In this case, output data only has single float numChan.
    /// If this AOV is not using closestFilter with closestFilterDepthOutput set to true, we don't have closestFilter depth info.
    /// So closestFilterDepthOutput is automatically set false.
    int  getRenderOutput(const unsigned id,
                         std::vector<float>& data,
                         const bool top2bottom = false,
                         const bool closestFilterDepthOutput = false);

    /// @brief Get current RenderOutput(=AOV) buffer data
    /// @param aovName specify AOV's buffer name.
    /// @param data returned current RenderOutput buffer which pointed by id.
    /// @param top2bottom specify output data Y direction order.
    /// @param closestFilterDepthOutput convert depth info when thie AOV is using closestFilter instead data itself
    /// @return return channel total number. 0 implies no data. -1 is error and you can get error message by
    ///         getErrorMsg().
    ///
    /// @detail
    /// This API is designed for display logic of frontend client. <br>
    /// data is automatically adjusted by image size with channel number and total size of data is
    /// getWidth() * getHeight() * sizeof(float) * number-of-channels byte <br>
    /// Internally this API does not do data.clear() but do data.resize() by proper image size everytime
    /// you call this API.<br>
    /// If current image condition is coarse pass, output rgbFrame is properly extrapolated.<br>
    /// number-of-channels are changed based on AOV buffer type. We have 4 possibilities of AOV buffer
    /// configuration now. 1 float to 4 float buffers. In any case, AOV buffer channel total number is returned
    /// as return value of this API.<br>
    /// If this API implies no data. This means there is no apropriate data which associated to aovName.
    /// AOV data itself is simply just copies 0x0 into data without any conversion as 32bit single float.
    /// data's formats are same as previous getRenderOutput() function.
    /// (See comments of previous getRenderOutput() function for more detail).
    /// If you set top2bottom as true, output rgbFrame is flipped Y direction.
    /// If this AOV is using closestFilter and closestFilterDepthOutput is true, this function convert closestFilter's depth instead data itself.
    /// In this case, output data only has single float numChan.
    /// If this AOV is not using closestFilter with closestFilterDepthOutput set to true, we don't have closestFilter depth info.
    /// So closestFilterDepthOutput is automatically set false.
    int  getRenderOutput(const std::string& aovName,
                         std::vector<float>& data,
                         const bool top2bottom = false,
                         const bool closestFilterDepthOutput = false);

    //------------------------------

    /// @brief Get pixel value of beauty buffer
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @return return pixel value of beauty buffer as scene_rdl2::math::Vec4f
    ///
    /// @detail
    /// This API returns pixel value of internal Beauty buffer and mainly is using for debugging purpose. 
    // Return value is Vec4f and [0]=R, [1]=G, [2]=B and [3]=A
    scene_rdl2::math::Vec4f getPixBeauty(const int sx, const int sy) const;

    /// @brief Get pixel value of pixelInfo (depth) buffer
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @return return pixel value of pixel info buffer as float
    ///
    /// @detail
    /// This API returns pixel value of internal pixel info buffer and mainly is using for debugging purpose. 
    float getPixPixelInfo(const int sx, const int sy) const;

    /// @brief Get pixel value of heatMap buffer
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @return return pixel value of heatMap info buffer as float
    ///
    /// @detail
    /// This API returns pixel value of internal heatMap buffer and mainly is using for debugging purpose. 
    float getPixHeatMap(const int sx, const int sy) const;

    /// @brief Get pixel value of weight buffer
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @return return pixel value of weight buffer as float
    ///
    /// @detail
    /// This API returns pixel value of internal weight buffer and mainly is using for debugging purpose. 
    float getPixWeightBuffer(const int sx, const int sy) const;

    /// @brief Get pixel value of beauty odd buffer
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @return return pixel value of beauty odd buffer as scene_rdl2::math::Vec4f
    ///
    /// @detail
    /// This API returns pixel value of internal Beauty odd buffer and mainly is using for debugging purpose. 
    // Return value is Vec4f and [0]=R, [1]=G, [2]=B and [3]=A
    scene_rdl2::math::Vec4f getPixBeautyOdd(const int sx, const int sy) const;

    /// @brief Get pixel value of renderOutput (AOV) buffer
    /// @param id specify renderOutput id
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @param out return pixel value
    /// @return return AOV's number of channels. return 0 if there is no expected data
    ///
    /// @detail
    /// This API uses renderOutput-id to identify the data instead of aovName.
    /// This API returns pixel value of internal renderOutput buffer and mainly is using for debugging purpose. 
    /// The return value is std::vector<float> and properly resized and set value by this function based on
    /// the internal definition.
    int getPixRenderOutput(const unsigned id, const int sx, const int sy,
                           std::vector<float>& out) const; // return numChan

    /// @brief Get pixel value of renderOutput (AOV) buffer
    /// @param aovName specify renderOutput AOV name
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @param out return pixel value
    /// @return return AOV's number of channels. return 0 if there is no expected data
    ///
    /// @detail
    /// This API uses aovName to identify the data instead of renderOutput-id.
    /// This API returns pixel value of internal renderOutput buffer and mainly is using for debugging purpose. 
    /// The return value is std::vector<float> and properly resized and set value by this function based on
    /// the internal definition.
    int getPixRenderOutput(const std::string& aovName, const int sx, const int sy,
                           std::vector<float>& out) const; // return numChan

    /// @brief Return pixel value detailed information by string
    /// @param sx pixel screen position x
    /// @param sy pixel screen position y
    /// @param aovName specify renderOutput AOV name or pre-defined internal buffer name
    /// @return return pixel detailed information as string
    ///
    /// @detail
    /// Return pixel value detailed information by string.
    /// This API returns details information about specified pixel as string.
    /// We have special aovName for special internal frame buffers besides regular AOV buffers as follows.
    /// <table>
    ///   <tr>
    ///     <th>aovName</th>
    ///     <th>description</th>
    ///   </tr>
    ///   <tr>
    ///     <td>*Beauty</td>
    ///     <td>beauty buffer, RGBA 4 floats</td>
    ///   </tr>
    ///   <tr>
    ///     <td>*HeatMap</td>
    ///     <td>heatMap data, single float</td>
    ///   </tr>
    ///   <tr>
    ///     <td>*Weight</td>
    ///     <td>weight data, single float</td>
    ///   </tr>
    ///   <tr>
    ///     <td>*BeautyOdd</td>
    ///     <td>beauty odd buffer, RGBA 4 floats</td>
    ///   </tr>
    /// </table>
    /// If you specify another name of above as aovName, that aovName is used as a regular AOV buffer name.
    std::string showPix(const int sx, const int sy, const std::string& aovName) const;

    //------------------------------

    /// @brief Get current LatencyLog information
    /// @return return reference to current LatencyLog
    ///
    /// @detail
    /// If upstream computation sends LatencyLog data to the client, this API provides to access these
    /// information.<br>
    /// If upstream computation does not send LatencyLog data, returned LatencyLog is default value and not
    /// updated.<br>
    /// LatencyLog is MCRT computation's internal timing log information.<br><br>
    /// There are 2 possible situations.
    /// <table>
    ///   <tr>
    ///     <th>situation</th>
    ///     <th>description</th>
    ///   </tr>
    ///   <tr>
    ///     <td>Single MCRT</td>
    ///     <td>LatencyLog is a timing log information of MCRT computation</td>
    ///   </tr>
    ///   <tr>
    ///     <td>Multi MCRT</td>
    ///     <td>LatencyLog is a timing log information of MCRT Merger computation</td>
    ///   </tr>
    /// </table>
    /// Typical information dump APIs for LatencyLog itself is grid_util::LatencyLog::show().<br>
    /// (See scene_rdl2/lib/common/grid_util/LatencyLog.h for more detail)<br>
    const scene_rdl2::grid_util::LatencyLog &getLatencyLog() const;

    /// @brief Get current LatencyLogUpstream information
    /// @return return reference to current LatencyLogUpstream
    ///
    /// @detail
    /// If multi MCRT configured situation and also upstream Merger computation sends LatencyLogUpstream
    /// information,
    /// this API provides to access these information.<br>
    /// If upstream computation is not MCRT merge and not send proper LatencyLogUpstream information, 
    /// returned LatencyLogUpstream information is default value.<br>
    /// This LatencyLogUpstream containes all MCRT computation's LatencyLog info which received by
    /// MCRT Merger computation which contributed to create current image.<br>
    /// Typical information dump APIs for LatencyLog itself is grid_util::LatencyLogUpstream::show().<br>
    /// (See scene_rdl2/lib/common/grid_util/LatencyLog.h for more detail)<br>
    const scene_rdl2::grid_util::LatencyLogUpstream& getLatencyLogUpstream() const;

    //------------------------------

    /// @brief Set infoRec info snapshot minimum interval. Snapshot is never executed less than this interval.
    /// @param sec specify infoRec snapshot interval by sec. All infoRec logic is disable when you set 0.0f
    ///
    /// @detail
    /// InfoRec is an information recording logic which is designed for debug and performance analyzing
    /// purposes. ClientReceiverFb saves all internal statistical information in the separate memory at
    /// run time. Then every 60sec this information is dumped to the disk.
    /// This API set up an infoRec information snapshot event interval by second and adds current
    /// information to the last of the infoRec data array.
    /// InfoRec information snapshot is executed by the internal of
    /// ClientReceiverFb::decodeProgressiveFrame() function and this snapshot action is only executed if
    /// duration between previous snapshot and current snapshot is more than user defined second.
    /// This sec value is only used for controlling minimum interval duration between previous infoRec
    /// snapshot and current snapshot and not guaranteed the exact interval timing of snapshot action.
    /// If you set an extremely short interval or execute an extremely long session, the internal infoRec
    /// data array size gets bigger and might be a problem.
    void setInfoRecInterval(const float sec); // 0.0 = disable infoRec logic

    /// @brief Set minimum interval of showing infoRec information 
    /// @param sec specify interval of infoRec information dump to the std::cerr 
    ///
    /// @detail
    /// Current infoRec information is shown to the std::cerr by user defined interval. This information
    /// dump to std::cerr is useful to understand all system statistical information by some interval at
    /// client side.
    /// This sec value is only used for controlling minimum interval duration between previous information
    /// display and current display and not guaranteed the exact interval timing of information display
    /// action.
    void setInfoRecDisplayInterval(const float sec);

    /// @brief Set filename of infoRec dump
    /// @param fileName Specify infoRec dump filename.
    ///
    /// @detail
    /// InfoRec logic creates files which include all the statistical information from beginning to that
    /// moment by every 60sec. Timestamp strings and one of extensions (iRec-A, iRec-C or iRec-D) are
    /// added to the created filename. So It is easily understood when the file was created. Usually long
    /// client session creates lots of files.
    /// Later files include all previous information. Basically we only need the very last files. Why we
    /// creates multiple files is because we still get useful information from remained files even if we
    /// got unexpected client crash
    /// InfoRec creates the following files for example. In this case, used setInfoRecFileName("./run_")
    /// <ul type="disc">
    ///   <li>run_2021May12Wed_1746_35_548.iRec-A</li>
    ///   <li>run_2021May12Wed_1747_36_249.iRec-A</li>
    ///   <li>run_2021May12Wed_1747_50_205.iRec-C</li>
    ///   <li>run_2021May12Wed_1747_50_593.iRec-F</li>
    /// </ul>
    /// In this case we have 4 files and the last file run_2021May12Wed_1747_50_593.iRec-F includes all
    /// of the previous file's information.
    /// <ul>
    ///   <li>iRec-A files are information from begginning to the rendering completion.</li>
    ///   <li>iRec-C file is information at rendering complete timing.</li>
    ///   <li>iRec-F file is information at finish rendering timing.</li>
    /// </ul>
    /// Usually render complete timing and render finish timing is different under multi-machine
    /// environments. Because, merge computation decides "render complete" timing and send stop
    /// request to the mcrt computation. All mcrt computations stop slightly late timing and this
    /// is render finish timing.
    /// If your session is finished in normal condition you have multiple iRec-A files and single iRec-C
    /// and single iRec-F files in the output directory.
    void setInfoRecFileName(const std::string& fileName);

    /// @brief This API records message receive timing in order to get statistical information
    ///
    /// @detail
    /// Inside the client application, this API should be called when the client receives any
    /// messages from the back-end.
    /// This information is used to analyze frequency of receiving messages internally.
    void updateStatsMsgInterval(); // update message interval info

    /// @brief This API updates progressiveFrame related internal information
    ///
    /// @detail
    /// Inside the client application, this API should be called when the client receives
    /// progressiveFrame messages from the back-end. Inside this API, progressiveFrame related
    /// statistical information is updated.
    void updateStatsProgressiveFrame(); // update progressiveFrame message info

    /// @brief This API creates a string for displaying statistical information at client side.
    /// @param intervalSec You can set a message display interval.
    /// @param outMsg output message string for display statistical information
    /// @return return true properly create output message and interval between previous display
    /// and current is more than intervalSec. return false if your call is short duration from
    /// previous display message timing. In this case, you don't need to display messages.
    ///
    /// @detail
    /// This API is used to create message strings for displaying statistical information at
    /// client applications. You call this function multiple times. If the return value is true,
    /// you get a proper output message string and are ready to display. If the return value is
    /// false, the output message is not ready yet. 
    /// Probably your action is too short from last display message timing. This function does
    /// not return true if this function is called less than intervalSec duration. In this case
    /// you should try again later. 
    bool getStats(const float intervalSec, std::string& outMsg);

    /// @brief This API returns how frequently received and decoded image data (as
    /// progressiveFrame message).
    /// @return Return fps value of receive and decode progressiveFrame message.
    ///
    /// @detail
    /// ClientReceiverFb is tracking frequency of receiving and decoding progressiveFrame
    /// internally. This API returns these frequency statistical results.
    float getRecvImageDataFps();

    /// @brief This API returns total update operation count regarding internal frame buffer
    /// @return Return total update operation count from beginning of process.
    ///
    /// @detail
    /// ClientReceiverFb is tracking the total number of internal frame buffer update operations.
    /// And this counter value is returned by this API. This counter is never reset even if the
    /// new frame is started. This counter is equivalent to indicate internal frame buffer activity. 
    /// If this counter is changed, this means the internal frame buffer was updated.
    /// If this counter is not changed, this means the internal frame buffer is guaranteed to be
    /// stable and never updated. Initial condition of this counter is 0.
    unsigned getFbActivityCounter();

    //------------------------------

    /// @brief This API enables ClientReceiverFb's debug console functionality by the environment variable
    /// @param sendMessage You need to setup call back function for send message to the back-end engine
    ///
    /// @detail
    /// ClientReceiver's debug console is enabled when environment variable
    /// "CLIENTRECEIVER_CONSOLE <port>" is set up. 
    /// <port> is used for port number in order to connect to the debug console by telnet-connection.
    /// A typical client to connect ClientReceiver's debug console is "telnet".
    void consoleAutoSetup(const CallBackSendMessage& sendMessage);

    /// @brief This API enables ClientReceiverFb's debug console functionality.
    /// @param port The port is used for port number in order to connect to the debug console by
    /// telnet-connection.
    /// @param sendMessage You need to setup call back function for send message to the back-end engine
    ///
    /// @detail
    /// This API enables ClientReceiverFb's debug console functionality by setup all necessary
    /// information by its arguments. You should use this API if you want to set up all information
    /// by yourself instead of using environment variables by consoleAutoSetup().
    void consoleEnable(const unsigned short port,
                       const CallBackSendMessage& sendMessage);

    /// @brief Returns console driver 
    /// @return ClientReceiverConsoleDriver
    ///
    /// @detail
    /// Sometimes, you want to access ClientReceiverConsoleDriver directly. 
    /// You need to access ClientReceiverConsoleDriver, if your client needs to add more parsing commands
    /// into them. arras_render is a good example.
    ClientReceiverConsoleDriver& consoleDriver();

    /// @brief Returns Parser object reference
    /// @return Parser
    ///
    /// @detail
    /// Sometimes, you might want to access Parser object to execute command-line action to this object.
    Parser& getParser();

    /// @brief String replasentation of DenoiseMode
    /// @return String
    ///
    /// @detail
    /// Return string representation of DenoiseEngine and this might be useful for message output
    static std::string showDenoiseEngine(const DenoiseEngine& engine);

    /// @brief String replasentation of DenoiseMode
    /// @return String
    ///
    /// @detail
    /// Return string representation of DenoiseMode and this might be useful for message output
    static std::string showDenoiseMode(const DenoiseMode& mode);

    /// @brief String reprasentation of BackendStat
    /// @return String
    ///
    /// @detail
    /// Return string representation of BackendStat and this might be useful for message output
    static std::string showBackendStat(const BackendStat& stat);

    //------------------------------

    /// @brief Set TimingRecorderHydra data point for performance analysis for hdMoonray.
    /// @param shared pointer of TimingRecorderHydra object
    ///
    /// @detail
    /// This TimingRecorderHydra data pointer is used to breakdown detailed timing analysis, especially
    /// for 1st returned pixel data from backend computations. You can access all timing analysis information
    /// via debug console command. Setting TimingRecorderHydra data pointer is optional.
    /// If this data pointer is not set up, internally all timing analysis logic is turned off.
    /// How to properly set up TimingRecorderHydra itself is totally up to the client's (i.e. hdMoonray)
    /// response.
    void setTimingRecorderHydra(std::shared_ptr<TimingRecorderHydra> ptr);

    /// @brief integer representation of machineId that originally sent last received image.
    /// @return integer representation of machineId    
    ///
    /// @detail
    /// returns machineId that originally sent last received image.
    /// <table>
    ///   <tr>
    ///     <th>value</th>
    ///     <th>description</th>
    ///   </tr>
    ///   <tr>
    ///     <td>0 or positive</td>
    ///     <td>mcrt machineId</td>
    ///   </tr>
    ///   <tr>
    ///     <td>SenderMachineId::DISPATCH</td>
    ///     <td>dispatch computation (reserved. not used at this moment)</td>
    ///   </tr>
    ///   <tr>
    ///     <td>SenderMachineId::MERGE</td>
    ///     <td>merge computation</td>
    ///   </tr>
    ///   <tr>
    ///     <td>SenderMacineId::UNKNOWN</td>
    ///     <td>Data is not received yet</td>
    ///   </tr>
    /// </table>
    int getReceivedImageSenderMachineId() const;

    /// @brief string representation of sender's machineId
    /// @param machineId integer representation
    /// @return String
    ///
    /// @detail
    /// ClientReceiverFb is tracking received image's sender machineId.
    /// This machineId is int or enum SenderMachineId. This API simply converts int representation of
    /// machineId to the easily understandable string message.
    static std::string showSenderMachineId(int machineId);

    //------------------------------

    /// @brief Set telemetry overlay information display resolution
    /// @param width overlay information display width
    /// @param height overlay information display height
    ///
    /// @detail
    /// We need to set telemetry overlay information display resolution by this API because ClientReceiverFb
    /// needs to produce telemetry overlay before receiving the first image. We don't know the image
    /// resolution at the beginning yet. You can change telemetry overlay resolution anytime during sessions.
    void setTelemetryOverlayReso(unsigned width, unsigned height);

    /// @brief Set telemetry overlay enable/disable condition
    /// @param sw display enable(=true)/disable(=false) condition
    /// 
    /// @detail
    /// You can enable/disable telemetry overlay by this API anytime during sessions. Default is false.
    void setTelemetryOverlayActive(bool sw);

    /// @brief Get telemetry overlay enable/disable condition
    /// @return return telemetry overlay condition. enable(=true) or disable(=false)
    ///
    /// @detail
    /// You can get the current telemetry overlay display condition by this API.
    bool getTelemetryOverlayActive() const;

    /// @brief Get all telemetry panel name
    /// @return return all telemetry panel name.
    ///
    /// @detail
    /// You can get all telemetry panel names as string vector.
    std::vector<std::string> getAllTelemetryPanelName();

    /// @brief Set the initial telemetry panel name
    /// @param panelName telemetry panel name
    ///
    /// @detail
    /// You can set the initial telemetry panel name that is shown if the telemetry overlay is turned on
    /// the first time. This panel is set to the current telemetry panel.
    /// If you don't set the initial panel name, the default is "devel".
    void setTelemetryInitialPanel(const std::string& panelName);

    /// @brief Switch the current telemetry panel by name
    /// @param panelName telemetry panelname
    ///
    /// @detail
    /// ClientReceiverFb has multiple telemetry panels inside. After finishing the initial telemetry overlay,
    /// you can change the telemetry panel by name. This panel is set to the current telemetry panel.
    /// This API only works after the initial telemetry overlay has been displayed.
    void switchTelemetryPanelByName(const std::string& panelName);

    /// @brief Switch telemetry panel to the next
    ///
    /// @detail
    /// ClientReceiverFb has multiple different versions of the telemetry panels and this API
    /// switches the current overlay panel to the next one in the panel table.
    void switchTelemetryPanelToNext();

    /// @brief Switch telemetry panel to the previous
    ///
    /// @detail
    /// ClientReceiverFb has multiple different versions of the telemetry panels and this API
    /// switches the current overlay panel to the previous one in the panel table.
    void switchTelemetryPanelToPrev();

    /// @brief Switch the current telemetry panel to the parent if the current panel has parent
    ///
    /// @detail
    /// The telemetry panel table can be constructed as tree structures. We can change the current
    /// displayed panel to the parent if the current panel has the parent.
    void switchTelemetryPanelToParent();

    /// @brief Switch the current telemetry panel to the child if the current panel has a child
    ///
    /// @detail
    /// The telemetry panel table can be constructed as tree structures. We can change the current
    /// displayed panel to the child if the current panel has the child.
    void switchTelemetryPanelToChild();

protected:
    class Impl;
    std::unique_ptr<Impl> mImpl;
}; // ClientReceiverFb

} // namespace mcrt_dataio
