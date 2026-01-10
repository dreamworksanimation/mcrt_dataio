// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "VectorPacketManager.h"
#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

namespace mcrt_dataio {
namespace vectorPacket {

std::string
DrawLine::genPathVisCurrInfoPanelMsg(const std::function<std::string()>& colReset) const
//
// Generate strings for the telemetry panel regarding this line.
// This API is called from the telemetry panel generation logic.
//
{
    const bool startActiveCurrPos = isValidStartActiveCurrPos();
    const bool endActiveCurrPos = isValidEndActiveCurrPos();

    auto showPosV2ui = [](const Vec2ui& v) {
        auto showV = [](const unsigned u) {
            std::ostringstream ostr;
            ostr << std::setw(4) << u;
            return ostr.str();
        };
        std::ostringstream ostr;
        ostr << '(' << showV(v[0]) << ", " << showV(v[1]) << ')';
        return ostr.str();
    };
    auto showCurrPoint = [&](const bool flag) -> std::string {
        if (flag) return telemetry::LayoutBase::colFg(C3(255,0,0)) + "<<-CURR" + colReset();
        else return "";
    };
    auto showStatus = [&]() {
        std::ostringstream ostr;
        ostr << "LineStatus (0x" << std::hex << mStatus.getStat() << std::dec << ") {\n"
             << "  rayType:"
             << telemetry::LayoutBase::colFg(mC3.bestContrastCol()) << telemetry::LayoutBase::colBg(mC3)
             << mStatus.rayTypeStr(mStatus.getRayType())
             << colReset() << '\n'
             << "  drawEndPoint:" << scene_rdl2::str_util::boolStr(mStatus.getDrawEndPointFlag()) << '\n'
             << "  sPosType:" << mStatus.posTypeStr(mStatus.getStartPosType()) << ' ' << showCurrPoint(startActiveCurrPos) << '\n'
             << "  ePosType:" << mStatus.posTypeStr(mStatus.getEndPosType()) << ' ' << showCurrPoint(endActiveCurrPos) << '\n'
             << "}";
        return ostr.str();
    };

    std::ostringstream ostr;
    if (!mActive) return "activeLine:OFF";

    ostr << "activeLine:ON\n"
         << "sPos:" << showPosV2ui(mStart) << ' ' << showCurrPoint(startActiveCurrPos) << '\n'
         << "ePos:" << showPosV2ui(mEnd) << ' ' << showCurrPoint(endActiveCurrPos) << '\n'
         << showStatus() << '\n'
         << " color:" << mC3.show() << '\n'
         << " alpha:" << static_cast<unsigned>(mAlpha) << '\n'
         << " width:" << mWidth << '\n'
         << "nodeId:" << mNodeId << '\n'
         << "isCurrPosStart:" << scene_rdl2::str_util::boolStr(mIsCurrPosStart);
    return ostr.str();
}

void
DrawLine::toggleActiveCurrPos()
//
// Switch back and forth between start and end point if the point has one of the START, ISECT, and
// END position condition.
//
{
    if (mActive) {
        mIsCurrPosStart = !mIsCurrPosStart;
        /* for debug
        std::cerr << ">> VectorPacketManager.cc mIsCurrPosStart:"
                  << scene_rdl2::str_util::boolStr(mIsCurrPosStart) << '\n';
        */
    }
}

bool
DrawLine::isValidStartActiveCurrPos() const
{
    if (mIsCurrPosStart) return isPosType(mStatus.getStartPosType());
    return false;
}

bool    
DrawLine::isValidEndActiveCurrPos() const
{
    if (!mIsCurrPosStart) return isPosType(mStatus.getEndPosType());
    return false;
}

void
DrawLine::adjustCurrPos()
//
// Adjust CurrPos condition automatically and properly pick one of the start or end
//
{
    if (!mActive) return;

    /* for debug
    std::cerr << ">> VectorPacketManager.cc adjustCurrPos start"
              << " mIsCurrPosStart:" << scene_rdl2::str_util::boolStr(mIsCurrPosStart)
              << "  isPosType(start):" << scene_rdl2::str_util::boolStr(isPosType(mStatus.getStartPosType())) 
              << "  isPosType(end):" << scene_rdl2::str_util::boolStr(isPosType(mStatus.getEndPosType())) 
              << '\n';
    */

    if (mIsCurrPosStart) {
        if (!isPosType(mStatus.getStartPosType())) {
            if (isPosType(mStatus.getEndPosType())) mIsCurrPosStart = false;
        }
    } else {
        if (!isPosType(mStatus.getEndPosType())) {
            if (isPosType(mStatus.getStartPosType())) mIsCurrPosStart = true;
        }
    }

    /* for debug
    std::cerr << ">> VectorPacketManager.cc adjustCurrPos end"
              << " mIsCurrPosStart:" << scene_rdl2::str_util::boolStr(mIsCurrPosStart)
              << '\n';
    */
}

std::string
DrawLine::show() const
{
    std::ostringstream ostr;
    ostr << "DrawLine {\n"
         << "  mActive:" << scene_rdl2::str_util::boolStr(mActive) << '\n'
         << "  mStart:" << mStart << '\n'
         << "  mEnd:" << mEnd << '\n'
         << scene_rdl2::str_util::addIndent(mStatus.show()) << '\n'
         << "  mC3:" << mC3.show() << '\n'
         << "  mAlpha:" << static_cast<unsigned>(mAlpha) << '\n'
         << "  mWidth:" << mWidth << '\n'
         << "  mNodeId:" << mNodeId << '\n'
         << "  mLineId:" << mLineId << '\n'
         << "  mIsCurrPosStart:" << scene_rdl2::str_util::boolStr(mIsCurrPosStart) << '\n'
         << "}";
    return ostr.str();
}

std::string
DrawLine::showCurrPos() const
{
    std::ostringstream ostr;
    ostr << "CurrPos:";
    if (isCurrPosActive()) {
        if (mIsCurrPosStart) {
            ostr << "Start (" << mStatus.showStartPosType() << ')';
        } else {
            ostr << "End (" << mStatus.showEndPosType() << ')';
        }
    } else {
        ostr << "NotActive";
    }
    return ostr.str();
}

//------------------------------------------------------------------------------------------

void
Rank::configureActions()
//
// Configure all the actions for decoding vectorPacket data
//    
{
    using BBox2i = scene_rdl2::math::BBox2i;
    using Vec2i = scene_rdl2::math::Vec2i;
    using Vec2ui = scene_rdl2::math::Vec2<unsigned>;
    using Vec4uc = scene_rdl2::math::Vec4<unsigned char>;
    using Vec2f = scene_rdl2::math::Vec2f;

    /* for debug
    mVectorPacketDequeue.setMsgCallBack
        ([](const std::string& msg) {
            std::cerr << msg;
            return true;
        });
    */

    mVectorPacketDequeue.setActionDictionary
        ([](const scene_rdl2::grid_util::VectorPacketDictEntry& entry, std::string& errMsg) {
            /* for debug
            std::cerr << ">> VectorPacketManager.cc " << entry.show() << '\n'; 
            */
            return true;
        });

    mVectorPacketDequeue.setActionLine2DUInt
        ([&](const Vec2ui& s,
             const Vec2ui& e,
             const scene_rdl2::grid_util::VectorPacketLineStatus& status,
             const unsigned nodeId,
             std::string& errMsg) {
            if (mSkipDraw) return true;

            const bool drawEndPoint = status.getDrawEndPointFlag();
            const float width = (mOverrideWidthSw) ? mOverrideWidth : mCurrWidth;
            const float radiusEndPoint = width + 1.0f;
            {
                telemetry::C3 currC3 = mCurrC3;
                const float currWidth = width;
                const float currRadiusEndPoint = radiusEndPoint;
                if (getActiveCurrLineMode()) {
                    currC3.scale(0.8f);
                }
                mTelemetryDisplayObsrPtr->getOverlay()->drawLine(s[0], s[1], e[0], e[1],
                                                                 currWidth,
                                                                 false, // drawStartPoint
                                                                 0.0f, // radiusStartPoint
                                                                 drawEndPoint,
                                                                 currRadiusEndPoint,
                                                                 currC3,
                                                                 mCurrAlpha);
            }

            if (mSelectType == SelectType::NAIVE) {
                if (mCurrLine.getActive()) {
                    if (mDrawLineTotal == mCurrLineId) {
                        mCurrLine.set(s, e, status, mCurrC3, mCurrAlpha, width, nodeId, mDrawLineTotal);
                    }
                }
            } else if (mSelectType == SelectType::STD) {
                auto updateCurrLine = [&]() {
                    mCurrLine.set(s, e, status, mCurrC3, mCurrAlpha, width, nodeId, mDrawLineTotal);
                    mValidCurrLine = true;
                };

                switch (mDecodeCurrLineMode) {
                case DecodeCurrLineMode::NO_ACTION :
                    break;
                case DecodeCurrLineMode::NEXT : // Pick up the next possible line data
                    if (mDrawLineTotal == mCurrLineId) {
                        if (status.isCurrPosValid(true, true)) updateCurrLine();
                        else mCurrLineId++; // We need to try the next possible line, which has a valid pos
                    } else {
                        if (status.isCurrPosValid(true, true)) {
                            if (!mValidCurrLine) {
                                //
                                // We only get one pass over all the vecPacket line data. During this single
                                // traversal, we need to identify the next current line after the current one.
                                // However, the next current line must be valid-that is, its endpoint must be
                                // one of START, ISECT, or END.
                                // If we simply decode lines in order, detect the current line, and then start
                                // looking for the next candidate, we might miss it. In that case, we need to
                                // fall back to the first valid line found at the beginning of the traversal
                                // and use that as the next current line.
                                // To handle this within a single pass, the implementation records the first
                                // valid line it encounters as a candidate.
                                //
                                updateCurrLine(); // found very first curPos valid candidate
                            }
                        }
                    }
                    break;
                case DecodeCurrLineMode::PREV : // Pick up the previous possible line data
                    if (status.isCurrPosValid(true, true)) {
                        if (mDrawLineTotal <= mCurrLineId) {
                            updateCurrLine();
                        } else {
                            if (!mValidCurrLine) {
                                //
                                // If no previous candidate is found, then the last candidate discovered
                                // during the decode pass becomes the one we want. This is another trick
                                // to ensure the previous current line can be determined within a single
                                // decode pass.
                                //
                                updateCurrLine();
                            }
                        }
                    }
                    break;
                case DecodeCurrLineMode::PICK : { // Pick up the current line by pixel position
                    auto calcDist2 = [&](const Vec2ui& v) {
                        const Vec2i delta = mPickCurrPos - Vec2i(v);
                        return delta[0] * delta[0] + delta[1] * delta[1];
                    };
                    auto isPickCandidate = [&](const Vec2ui& v) {
                        const unsigned currDist2 = calcDist2(v);
                        const unsigned threshDist2 = mPickThreshDist * mPickThreshDist;
                        if (currDist2 > threshDist2 || currDist2 > mPickCurrDist2Work) return false;
                        mPickCurrDist2Work = currDist2;
                        return true;
                    };
                    if (status.isCurrPosValid(true, false) && isPickCandidate(s)) {
                        updateCurrLine();
                        mCurrLine.setCurrPosStart(true);
                    }
                    if (status.isCurrPosValid(false, true) && isPickCandidate(e)) {
                        updateCurrLine();
                        mCurrLine.setCurrPosStart(false);
                    }
                } break;
                }
            }

            mDrawLineTotal++;
            return true;
        });

    mVectorPacketDequeue.setActionBoxOutline2DUInt
        ([&](const Vec2ui& min, const Vec2ui& max, std::string& errMsg) {
            if (mSkipDraw) return true;
            mTelemetryDisplayObsrPtr->getOverlay()->drawBoxOutline(BBox2i(Vec2i(min), Vec2i(max)),
                                                                   mCurrC3, mCurrAlpha);
            /* for debug
            std::cerr << ">> VectorPacketManager.cc "
                      << "BoxOutline "
                      << "min(" << min[0] << ',' << min[1] << ") "
                      << "max(" << max[0] << ',' << max[1] << ")\n";
            */
            return true;
        });

    mVectorPacketDequeue.setActionRgbaUc
        ([&](const Vec4uc& rgba, std::string& errMsg) {
            if (mSkipDraw) return true;
            mCurrC3 = telemetry::C3(rgba[0], rgba[1], rgba[2]);
            mCurrAlpha = rgba[3];
            /* for debug
            std::cerr << ">> VectorPacketManager.cc "
                      << "Rgba ("
                      << static_cast<unsigned>(rgba[0]) << ','
                      << static_cast<unsigned>(rgba[1]) << ','
                      << static_cast<unsigned>(rgba[2]) << ','
                      << static_cast<unsigned>(rgba[3]) << ")\n";
            */
            return true;
        });

    mVectorPacketDequeue.setActionWidth16UInt
        ([&](const float width, std::string& errMsg) {
            if (mSkipDraw) return true;
            mCurrWidth = width;
            // std::cerr << "Width (" << width << ")\n"; // for debug
            return true;
        });

    mVectorPacketDequeue.setActionNodeDataAll
        ([&](const std::string& data, std::string& errMsg) {
            //
            // This receives all the NodeData at once and saves it.
            //
            try {
                scene_rdl2::cache::ValueContainerDequeue vcd(data.data(), data.size());
                const size_t nodeSize = vcd.deqVLSizeT();
                mNodeTbl.clear();
                mNodeTbl.reserve(nodeSize);
                for (size_t i = 0; i < nodeSize; ++i) mNodeTbl.emplace_back(vcd);
                const size_t vtxSize = vcd.deqVLSizeT();
                if (vtxSize) mVertexTbl = vcd.deqVec3fVector();
                else mVertexTbl.clear();
                /* for debug
                std::cerr << ">> VectorPacketManager.cc actionNodeDataAll() done."
                          << " dataSize:" << data.size() << "byte\n";
                */
            }
            catch (const scene_rdl2::except::RuntimeError& e) {
                std::ostringstream ostr;
                ostr << "decode nodeDataAll failed. err={\n"
                     << scene_rdl2::str_util::addIndent(e.what()) << '\n'
                     << "}";
                errMsg = ostr.str();
                return false;
            }
            return true;
        });
}

void
Rank::decode()
//
// Throw an exception(std::string) if an error occurs
//
{
    /* for debug
    std::cerr << ">> VectorPacketManager.cc Rank::decode()"
              << " rankId:" << mRankId
              << " mData:0x" << std::hex << reinterpret_cast<uintptr_t>(mData.get()) << std::dec
              << " mDataSize:" << mDataSize
              << '\n';
    */
    if (!mData || !mDataSize) return;
    if (!mRunDecode) return; // skip decode

    mVectorPacketDequeue.reset(static_cast<const void*>(mData.get()), mDataSize);
    if (mDecodeCounter == 0) { // This is a very first time to decode data
        // we only need to decode NodeDataAll data once at very first time.
        mVectorPacketDequeue.setActionNodeDataAllSkip(false);

        if (mSelectType == SelectType::NAIVE || mSelectType == SelectType::STD) {
            //
            // At the moment, right after a new vecPacket arrives (i.e., right after the simulation data has
            // been regenerated), we turn off the current line mode once. Ideally, if current line mode = ON
            // and, for example, the camera is moved interactively, it would be best to keep current line
            // mode = ON continuously. Unfortunately, this isn't working right now.
            // The reason is that it's difficult to stably maintain the same current line information across
            // frames. In each new frame, the line topology is completely regenerated, so the current line
            // from the previous frame can't simply be carried over. Figuring out how to preserve the current
            // line information as consistently as possible across frames is an open issue going forward.
            //
            mCurrLineId = ~static_cast<unsigned>(0);
            setActiveCurrLineMode(false);
            mValidCurrLine = false;
        }
    } else {
        if (mSelectType == SelectType::STD) {
            if (mDecodeCurrLineMode != DecodeCurrLineMode::NO_ACTION) mValidCurrLine = false;
            if (mDecodeCurrLineMode == DecodeCurrLineMode::PICK) {
                mPickCurrDist2Work = ~static_cast<unsigned>(0);
            }
        }
    }

    //------------------------------

    mDrawLineTotal = 0;
    try {
        mVectorPacketDequeue.decodeAll();
    }
    catch (const std::string& err) {
        std::ostringstream ostr;
        ostr << "WARNING : VectorPacketManager.cc Rank::decode() failed. Rank=" << mRankId << " {\n"
             << scene_rdl2::str_util::addIndent(err) << '\n'
             << "}";
        std::cerr << ostr.str() << '\n';
    }

    if (mDecodeCounter == 0) { // This is a very first time to decode data
        // We skip decoding NodeDataAll when the 2nd or later decoding
        mVectorPacketDequeue.setActionNodeDataAllSkip(true);
    }
    mDecodeCounter++;

    if (mSelectType == SelectType::STD) {
        if (mDecodeCurrLineMode != DecodeCurrLineMode::NO_ACTION) {
            mCurrLineId = mValidCurrLine ? mCurrLine.getLineId() : ~static_cast<unsigned>(0);
            mDecodeCurrLineMode = DecodeCurrLineMode::NO_ACTION;
        }
    }

    //------------------------------

    if (getActiveCurrLineMode()) {
        if (mSelectType == SelectType::NAIVE) {
            if (mDrawLineTotal > 0) {
                if (mCurrLineId >= mDrawLineTotal) mCurrLineId = mDrawLineTotal - 1;            
                mValidCurrLine = true;
            }
        }

        if (mDrawLineTotal > 0 && mValidCurrLine) {
            mCurrLine.adjustCurrPos();
            drawCurrLine();
            drawCurrPosVtx();
        }
    }
}

bool
Rank::getRenderCounter(unsigned& counter) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryRenderCounter>
            (VectorPacketDictEntry::Key::RENDER_COUNTER,
             [&](const VectorPacketDictEntryRenderCounter& dictEntry) {
                 counter = dictEntry.getCounter();
             }));
}

bool
Rank::getHostname(std::string& hostName) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryHostname>
            (VectorPacketDictEntry::Key::HOSTNAME,
             [&](const VectorPacketDictEntryHostname& dictEntry) {
                 hostName = dictEntry.getHostname();
             }));
}

bool
Rank::getPathVis(bool& flag) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryPathVis>
            (VectorPacketDictEntry::Key::PATH_VIS,
             [&](const VectorPacketDictEntryPathVis& dictEntry) {
                 flag = dictEntry.getPathVis();
             }));
}

bool
Rank::getPos(Vec2ui& pos) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryPixPos>
            (VectorPacketDictEntry::Key::PIX_POS,
             [&](const VectorPacketDictEntryPixPos& dictEntry) {
                 pos = dictEntry.getPixPos();
             }));
}

bool
Rank::getMaxDepth(unsigned& maxDepth) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryMaxDepth>
            (VectorPacketDictEntry::Key::MAX_DEPTH,
             [&](const VectorPacketDictEntryMaxDepth& dictEntry) {
                 maxDepth = dictEntry.getMaxDepth();
             }));
}

bool
Rank::getSamples(unsigned& pixelSamples,
                 unsigned& lightSamples,
                 unsigned& bsdfSamples) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntrySamples>
            (VectorPacketDictEntry::Key::SAMPLES,
             [&](const VectorPacketDictEntrySamples& dictEntry) {
                 pixelSamples = dictEntry.getPixelSamples();
                 lightSamples = dictEntry.getLightSamples();
                 bsdfSamples = dictEntry.getBsdfSamples();
             }));
}

bool
Rank::getRayTypeSelection(bool& useSceneSamples,
                          bool& occlusionRaysOn,
                          bool& specularRaysOn,
                          bool& diffuseRaysOn,
                          bool& bsdfSamplesOn,
                          bool& lightSamplesOn) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryRayTypeSelection>
            (VectorPacketDictEntry::Key::RAY_TYPE_SELECTION,
             [&](const VectorPacketDictEntryRayTypeSelection& dictEntry) {
                 useSceneSamples = dictEntry.getUseSceneSamples();
                 occlusionRaysOn = dictEntry.getOcclusionRaysOn();
                 specularRaysOn = dictEntry.getSpecularRaysOn();
                 diffuseRaysOn = dictEntry.getDiffuseRaysOn();
                 bsdfSamplesOn = dictEntry.getBsdfSamplesOn();
                 lightSamplesOn = dictEntry.getLightSamplesOn();
             }));
}
bool
Rank::getColor(Color& cameraRayColor,
               Color& specularRayColor,
               Color& diffuseRayColor,
               Color& bsdfSampleColor,
               Color& lightSampleColor) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryColor>
            (VectorPacketDictEntry::Key::COLOR,
             [&](const VectorPacketDictEntryColor& dictEntry) {
                 cameraRayColor = dictEntry.getCameraRayColor();
                 specularRayColor = dictEntry.getSpecularRayColor();
                 diffuseRayColor = dictEntry.getDiffuseRayColor();
                 bsdfSampleColor = dictEntry.getBsdfSampleColor();
                 lightSampleColor = dictEntry.getLightSampleColor();
             }));
}

bool
Rank::getLineWidth(float& width) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryLineWidth>
            (VectorPacketDictEntry::Key::LINE_WIDTH,
             [&](const VectorPacketDictEntryLineWidth& dictEntry) {
                 width = dictEntry.getLineWidth();
             }));
}

bool
Rank::getCamPos(Vec3f& pos) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryCamPos>
            (VectorPacketDictEntry::Key::CAM_POS,
             [&](const VectorPacketDictEntryCamPos& dictEntry) {
                 pos = dictEntry.getCamPos();
             }));
}

bool
Rank::getCamRayIsectSfPos(std::vector<Vec3f>& tbl) const
{
    using namespace scene_rdl2::grid_util;
    return (getDictValue<VectorPacketDictEntryCamRayIsectSfPos>
            (VectorPacketDictEntry::Key::CAMRAY_ISECT_SURFACE_POS,
             [&](const VectorPacketDictEntryCamRayIsectSfPos& dictEntry) {
                 tbl = dictEntry.getPosTbl();
             }));
}

std::string
Rank::genPathVisCurrInfoPanelMsg(const std::function<std::string()>& colReset) const
//
// Generate strings for the telemetry panel
//
{
    auto showCurrLineId = [&]() -> std::string {
        if (mCurrLineId == ~static_cast<unsigned>(0)) return "?";
        return std::to_string(mCurrLineId);
    };

    std::ostringstream ostr;
    ostr << "RankId:" << mRankId << '\n'
         << " timeStamp:" << scene_rdl2::str_util::secStr(mTimeStamp) << '\n'
         << " lineTotal:" << mDrawLineTotal << '\n';
    if (mSelectType == SelectType::NAIVE || mSelectType == SelectType::STD) {
        ostr << " currLineId:" << showCurrLineId() << '\n';
    }

    if (mDrawLineTotal > 0) {
        if (mCurrLine.getActive()) {
            ostr << '\n'
                 << mCurrLine.genPathVisCurrInfoPanelMsg(colReset) << '\n'
                 << '\n'
                 << genPathVisCurrNodeInfo() << '\n';

            unsigned vtxId = 0;
            std::string errMsg;
            if (!getCurrPosVtxId(vtxId, &errMsg)) {
                ostr << '\n'
                     << errMsg;
            } else {
                ostr << '\n'
                     << genPathVisCurrPosInfo(vtxId);
            }
        } else {
            ostr << '\n'
                 << "no ActiveCurrLine\n";
        }
    } else {
        ostr << '\n'
             << "no lines";
    }        
    return ostr.str();
}

bool
Rank::getCurrPosPosition(Vec3f& p) const
{
    if (!mCurrLine.getActive()) return false;

    unsigned vtxId;
    if (!getCurrPosVtxId(vtxId, nullptr)) {
        return false;
    }

    p = mVertexTbl[vtxId];

    return true;
}

std::string
Rank::show() const
{
    std::ostringstream ostr;
    ostr << "Rank {\n"
         << "  mRunDecode:" << scene_rdl2::str_util::boolStr(mRunDecode) << '\n'
         << "  mTimeStamp:" << mTimeStamp << "sec (" << scene_rdl2::str_util::secStr(mTimeStamp) << ")\n"
         << "  mDataSize:" << mDataSize << '\n'
         << "  mRankId:" << mRankId << '\n'
         << "  mTelemetryDisplayObsrPtr:0x" << std::hex << reinterpret_cast<uintptr_t>(mTelemetryDisplayObsrPtr) << std::dec << '\n' 
         << scene_rdl2::str_util::addIndent(mVectorPacketDequeue.show()) << '\n'
         << "  mCurrWidth:" << mCurrWidth << '\n'
         << "  mCurrC3:" << mCurrC3.show() << '\n'
         << "  mCurrAlpha:" << mCurrAlpha << '\n'
         << "  mOverrideWidthSw:" << mOverrideWidthSw << '\n'
         << "  mOverrideWidth:" << mOverrideWidth << '\n'
         << "  mDrawLineTotal:" << mDrawLineTotal << '\n'
         << scene_rdl2::str_util::addIndent(showCurrLine()) << '\n'
         << "}";
    return ostr.str();
}

std::string
Rank::showSimple() const
{
    std::ostringstream ostr;
    ostr << "Rank (this:0x" << std::hex << reinterpret_cast<uintptr_t>(this) << std::dec << ") {\n"
         << "  mRunDecode:" << scene_rdl2::str_util::boolStr(mRunDecode) << '\n'
         << "  mTimeStamp:" << mTimeStamp << "sec (" << scene_rdl2::str_util::secStr(mTimeStamp) << ")\n"
         << "  mRankId:" << mRankId << '\n'
         << "  mDataSize:" << mDataSize << '\n'
         << "  mData:0x" << std::hex << reinterpret_cast<uintptr_t>(mData.get()) << std::dec << '\n'
         << "}";
    return ostr.str();
}

std::string
Rank::showCurrLine() const
{
    return "mCurrLine " + mCurrLine.show();
}

std::string
Rank::showNodeTbl() const
{
    const int wi = scene_rdl2::str_util::getNumberOfDigits(mNodeTbl.size()); 

    std::ostringstream ostr;
    ostr << "nodeTbl (size:" << mNodeTbl.size() << ") {\n";
    for (size_t i = 0; i < mNodeTbl.size(); ++i) {
        ostr << "  i:" << std::setw(wi) << i << ' ' << mNodeTbl[i].showSimple() << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
Rank::showNode(const unsigned nodeId) const
{
    if (nodeId >= mNodeTbl.size()) {
        std::ostringstream ostr;
        ostr << "nodeId:" << nodeId << " is out of range. max:" << (mNodeTbl.size() - 1);
        return ostr.str();
    }
    return mNodeTbl[nodeId].show();
}

std::string
Rank::showVtxTbl() const
{
    auto showVec = [](const Vec3f& v) {
        auto showV = [](const float f) {
            std::ostringstream ostr;
            ostr << std::setw(10) << std::fixed << std::setprecision(5) << f;
            return ostr.str();
        };
        std::ostringstream ostr;
        ostr << '(' << showV(v[0]) << ',' << showV(v[1]) << ',' << showV(v[2]) << ')';
        return ostr.str();
    };

    const int wi = scene_rdl2::str_util::getNumberOfDigits(mVertexTbl.size());

    std::ostringstream ostr;
    ostr << "VertexTbl (size:" << mVertexTbl.size() << ") {\n";
    for (size_t i = 0; i < mVertexTbl.size(); ++i) {
        ostr << "  i:" << std::setw(wi) << i << ' ' << showVec(mVertexTbl[i]) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
Rank::showCurrPos() const
{
    return mCurrLine.showCurrPos();
}

void
Rank::drawCurrLine() const
{
    const float width = mCurrLine.getWidth() * 2.0f;
    const bool drawEndPoint = mCurrLine.getStatus().getDrawEndPointFlag();
    const float radiusEndPoint = width + 1.0f;
    float alpha = mCurrLine.getAlpha();
    alpha = static_cast<unsigned char>((255 - alpha) * 0.5f) + alpha;

    mTelemetryDisplayObsrPtr->getOverlay()->drawLine(mCurrLine.getStart()[0],
                                                     mCurrLine.getStart()[1],
                                                     mCurrLine.getEnd()[0],
                                                     mCurrLine.getEnd()[1],
                                                     width,
                                                     false, // drawStartPoint,
                                                     0.0f, // radiusStartPoint
                                                     drawEndPoint,
                                                     radiusEndPoint,
                                                     mCurrLine.getC3(),
                                                     alpha);
}

void
Rank::drawCurrPosVtx() const
{
    if (!mCurrLine.getActive()) return; // skip

    Vec2ui vec2;
    if (mCurrLine.isValidStartActiveCurrPos())    vec2 = mCurrLine.getStart();
    else if (mCurrLine.isValidEndActiveCurrPos()) vec2 = mCurrLine.getEnd();
    else return; // skip

    const float radius = mCurrLine.getWidth() * 5.0f;
    constexpr float width = 3.0f;
    
    mTelemetryDisplayObsrPtr->getOverlay()->drawCircle(vec2[0], vec2[1], radius, width,
                                                       telemetry::C3(255, 0, 0), 255);
}

std::string
Rank::genPathVisCurrNodeInfo() const
{
    if (!mCurrLine.getActive()) return "";

    const unsigned nodeId = mCurrLine.getNodeId();
    if (nodeId >= mNodeTbl.size()) {
        std::ostringstream ostr;
        ostr << "Unknown (nodeId:" << nodeId << ')';
        return ostr.str();
    }

    return mNodeTbl[nodeId].genTelemetryPanelPathVisCurrNodeMsg(nodeId);
}

std::string
Rank::genPathVisCurrPosInfo(const unsigned vtxId) const
{
    const Vec3f& vtx = mVertexTbl[vtxId];

    auto showVec = [](const Vec3f& vec) {
        auto showF = [](const float f) {
            std::ostringstream ostr;
            ostr << std::setw(10) << std::fixed << std::setprecision(5) << f;
            return ostr.str();
        };
        std::ostringstream ostr;
        ostr << "posX:" << showF(vec[0]) << '\n'
             << "posY:" << showF(vec[1]) << '\n'
             << "posZ:" << showF(vec[2]);
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "===== Vtx (id:" << vtxId << ") =====\n"
         << showVec(vtx) << '\n';
    return ostr.str();
}

bool
Rank::getCurrPosVtxId(unsigned& vtxId, std::string* errMsg) const
{
    if (!mCurrLine.getActive()) {
        if (errMsg) (*errMsg) = "no currLine";
        return false;
    }
    if (!mCurrLine.isCurrPosActive()) {
        if (errMsg) (*errMsg) = "no currPos";
        return false;
    }

    const unsigned nodeId = mCurrLine.getNodeId();
    if (nodeId >= mNodeTbl.size()) {
        if (errMsg) {
            std::ostringstream ostr;
            ostr << "Unknown (nodeId:" << nodeId << ')';
            (*errMsg) = ostr.str();
        }
        return false;
    }
    const scene_rdl2::grid_util::VectorPacketNode& node = mNodeTbl[nodeId];

    const PosType posType =
        mCurrLine.isValidStartActiveCurrPos() ? mCurrLine.getStartPosType() : mCurrLine.getEndPosType();

    unsigned currVtxId = 0;
    switch (posType) {
    case PosType::START : currVtxId = node.getStartId(); break;
    case PosType::END : currVtxId = node.getEndId(); break;
    case PosType::ISECT :
        if (!node.getIsectActive()) {
            if (errMsg) (*errMsg) = "inactive isectVtx";
            return false;
        }
        currVtxId = node.getIsectId();
        break;

    default :
        if (errMsg) (*errMsg) = "unknown posType";
        return false;
    }
    if (currVtxId >= mVertexTbl.size()) {
        if (errMsg) {
            std::ostringstream ostr;
            ostr << "Unknown (vtxId:" << currVtxId << ')';
            (*errMsg) = ostr.str();
        }
        return false;
    }

    vtxId = currVtxId;

    return true;
}

void
Rank::parserConfigure()
{
    mParser.description("Rank command");

    mParser.opt("show", "", "show info",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
    mParser.opt("showSimple", "", "show only vecPacket data info",
                [&](Arg& arg) { return arg.msg(showSimple() + '\n'); });
    mParser.opt("showCurrLine", "", "show current draw line info",
                [&](Arg& arg) { return arg.msg(showCurrLine() + '\n'); });
    mParser.opt("overrideWidthSw", "<on|off|show>", "overrideWidth condition",
                [&](Arg& arg) { return parseSingleArgCmd(arg, mOverrideWidthSw); });
    mParser.opt("overrideWidth", "<w|show>", "set overrideWidth by float",
                [&](Arg& arg) { return parseSingleArgCmd(arg, mOverrideWidth); });
    mParser.opt("runDecode", "<on|off|show>", "set run decode condition",
                [&](Arg& arg) { return parseSingleArgCmd(arg, mRunDecode); });
    mParser.opt("dequeue", "...command...", "vectorPacketDequeue command",
                [&](Arg& arg) { return mVectorPacketDequeue.getParser().main(arg.childArg()); });
                
    mParser.opt("selectType", "<naive|std|show>", "set selection logic type.",
                [&](Arg& arg) {
                    if (arg() == "show") { arg++; }
                    else if (arg() == "naive") { mSelectType = SelectType::NAIVE; arg++; }
                    else if (arg() == "std") { mSelectType = SelectType::STD; arg++; }
                    else { return arg.msg("unknown selectType:" + (arg++)() + '\n'); }
                    return arg.msg(selectTypeStr(mSelectType) + '\n');
                    // return parseSingleArgCmd(arg, mSelectType); // OLD
                });
    mParser.opt("nextCurr", "", "set next current",
                [&](Arg& arg) {
                    if (!getActiveCurrLineMode()) return arg.msg("disable active curr line mode\n");
                    deltaCurrLineId(1);
                    if (mSelectType == SelectType::STD) mDecodeCurrLineMode = DecodeCurrLineMode::NEXT;
                    return arg.msg(std::to_string(mCurrLineId) + '\n');
                });
    mParser.opt("prevCurr", "", "set prev current",
                [&](Arg& arg) {
                    if (!getActiveCurrLineMode()) return arg.msg("disable active curr line mode\n");
                    deltaCurrLineId(-1);
                    if (mSelectType == SelectType::STD) mDecodeCurrLineMode = DecodeCurrLineMode::PREV;
                    return arg.msg(std::to_string(mCurrLineId) + '\n');
                });
    mParser.opt("pickCurr", "<sx> <sy>", "pick current",
                [&](Arg& arg) {
                    const int sx = (arg++).as<int>(0);
                    const int sy = (arg++).as<int>(0);
                    if (!getActiveCurrLineMode()) return arg.msg("disable active curr line mode\n");
                    if (mSelectType != SelectType::STD) return arg.msg("select type != STD\n");
                    setPickCurrLineParam(sx, sy);
                    std::ostringstream ostr;
                    ostr << "pick (" << sx << ',' << sy << ')';
                    return arg.msg(ostr.str() + '\n');
                });
    mParser.opt("pickCurrDist", "<len|show>", "set pick current threshold distance",
                [&](Arg& arg) { return parseSingleArgCmd(arg, mPickThreshDist); });

    mParser.opt("showNodeTblSize", "", "show nodeTbl size",
                [&](Arg& arg) { return arg.msg(std::to_string(mNodeTbl.size()) + '\n'); });
    mParser.opt("showNodeTbl", "", "show all nodeTbl",
                [&](Arg& arg) { return arg.msg(showNodeTbl() + '\n'); });
    mParser.opt("showNode", "<nodeId>", "show node",
                [&](Arg& arg) { return arg.msg(showNode((arg++).as<unsigned>(0)) + '\n'); });
    mParser.opt("showVtxTbl", "", "show vertex table",
                [&](Arg& arg) { return arg.msg(showVtxTbl() + '\n'); });

    mParser.opt("activeCurrPosToggle", "", "toggle active current pos condition",
                [&](Arg& arg) {
                    toggleActiveCurrPos();
                    return arg.msg(showCurrPos() + '\n');
                });
}

void
Rank::deltaCurrLineId(const int delta)
{
    const int currId = static_cast<int>(mCurrLineId) + delta;
    if (currId < 0)                    mCurrLineId = mDrawLineTotal - 1;
    else if (currId >= mDrawLineTotal) mCurrLineId = 0;
    else                               mCurrLineId = currId;
}

void
Rank::setPickCurrLineParam(const int sx, const int sy)
{
    mDecodeCurrLineMode = DecodeCurrLineMode::PICK;
    mPickCurrPos[0] = sx;
    mPickCurrPos[1] = sy;
}

// static function
std::string
Rank::selectTypeStr(const SelectType type)
{
    switch (type) {
    case SelectType::NAIVE : return "NAIVE";
    case SelectType::STD : return "STD";
    default : return "?";
    }
}

//------------------------------------------------------------------------------------------

void
Manager::resetPushedData()
{
    crawlRankTable([](unsigned key, Rank* rankObsrPtr) {
        rankObsrPtr->resetPushedData();
        return true;
    });
}

void
Manager::pushData(const unsigned rankId, DataShPtr ptr, const size_t dataSize)
//
// Save the vecPacket binary data without decoding. Data is saved with timestamp information.
//
{
    if (!ptr || !dataSize) return; // just in case

    Rank* rankObsrPtr = findRank(rankId);
    if (rankObsrPtr == nullptr) {
        mTable[rankId] = std::make_unique<Rank>(rankId, mTelemetryDisplayObsrPtr);
        rankObsrPtr = mTable[rankId].get();
    }

    rankObsrPtr->pushData(mRecTime.end(), ptr, dataSize);
}

void
Manager::decodeAll()
//
// Throw an exception(std::string) if an error occurs
//
{
    if (mOnlyDrawCurrRank) {
        Rank* currRank = findRank(mCurrRankId);
        crawlRankTable([&](unsigned key, Rank* rankObsrPtr) {
            rankObsrPtr->setSkipDraw((rankObsrPtr == currRank) ? false : true);
            rankObsrPtr->decode();
            return true;
        });
    } else {
        crawlRankTable([&](unsigned key, Rank* rankObsrPtr) {
            rankObsrPtr->setSkipDraw(false);
            rankObsrPtr->decode();
            return true;
        });
    }
}

void
Manager::decode(const unsigned rankId)
//
// Throw an exception(std::string) if an error occurs
//
{
    Rank* rankObsrPtr = findRank(rankId);
    if (rankObsrPtr == nullptr) return; // skip
    rankObsrPtr->decode();
}

std::string
Manager::genPathVisCtrlInfoPanelMsg(const ColStrFunc& colStrFunc) const
{
    auto emptyLine = []() -> std::string { return "          ?"; };
    auto emptyPanel = [&]() -> std::string { return "\n" + emptyLine() + "\n "; };

    if (!isPathVisActive()) return emptyPanel();

    Rank* rank = mOnlyDrawCurrRank ? findRank(mCurrRankId) : findLatestUpdatedRank();
    if (!rank) return emptyPanel(); // not ready yet

    //------------------------------

    auto showRenderCounter = [&]() -> std::string {
        unsigned counter;
        if (!rank->getRenderCounter(counter)) return emptyLine(); // not ready yet
        std::ostringstream ostr;
        ostr << "RenderCounter:" << counter;
        return ostr.str();
    };
    auto showPos = [&]() -> std::string {
        Vec2ui pos;
        if (!rank->getPos(pos)) return emptyLine(); // not ready yet
        std::ostringstream ostr;
        ostr << "Pixel(" << std::setw(4) << pos[0] << ',' << std::setw(4) << pos[1] << ")";
        return ostr.str();
    };
    auto showMaxDepth = [&]() -> std::string {
        unsigned depth;
        if (!rank->getMaxDepth(depth)) return emptyLine(); // not ready yet
        std::ostringstream ostr;
        ostr << "     MaxDepth:" << depth;
        return ostr.str();
    };
    auto showRayTypeSelection = [&]() -> std::string {
        auto showOnOff = [](const bool b) -> std::string { return b ? "on" : "off"; };
        bool useSceneSamples;
        bool occlusionRaysOn;
        bool specularRaysOn;
        bool diffuseRaysOn;
        bool bsdfSamplesOn;
        bool lightSamplesOn;
        if (!rank->getRayTypeSelection(useSceneSamples,
                                       occlusionRaysOn,
                                       specularRaysOn,
                                       diffuseRaysOn,
                                       bsdfSamplesOn,
                                       lightSamplesOn)) {
            return "\n\n\n" + emptyLine() + "\n\n "; // not ready yet
        }
        std::ostringstream ostr;
        ostr << "useScnSampl:" << showOnOff(useSceneSamples) << '\n'
             << "  occlusion:" << showOnOff(occlusionRaysOn) << '\n'
             << "   specular:" << showOnOff(specularRaysOn) << '\n'
             << "    diffuse:" << showOnOff(diffuseRaysOn) << '\n'
             << "       bsdf:" << showOnOff(bsdfSamplesOn) << '\n'
             << "      light:" << showOnOff(lightSamplesOn);
        return ostr.str();
    };
    auto showSamples = [&]() -> std::string {
        unsigned pixelSamples;
        unsigned lightSamples;
        unsigned bsdfSamples;
        if (!rank->getSamples(pixelSamples, lightSamples, bsdfSamples)) {
            return "\n" + emptyLine() + "\n "; // not ready yet
        }
        std::ostringstream ostr;
        ostr << "   PixSamples:" << pixelSamples << '\n'
             << " LightSamples:" << lightSamples << '\n'
             << "  BsdfSamples:" << bsdfSamples;
        return ostr.str();
    };
    auto showLineWidth = [&]() -> std::string {
        float lineWidth;
        if (!rank->getLineWidth(lineWidth)) return emptyLine(); // not ready yet
        std::ostringstream ostr;
        ostr << "    LineWidth:" << lineWidth;
        return ostr.str();
    };
    auto showColor = [&]() -> std::string {
        Color cameraRayColor, specularRayColor, diffuseRayColor, bsdfSampleColor, lightSampleColor;
        if (!rank->getColor(cameraRayColor, specularRayColor, diffuseRayColor, bsdfSampleColor, lightSampleColor)) {
            return "\n\n" + emptyLine() + "\n\n ";
        }
        std::ostringstream ostr;
        ostr << " Cam:" << colStrFunc(cameraRayColor) << '\n'
             << "Spcl:" << colStrFunc(specularRayColor) << '\n'
             << "Diff:" << colStrFunc(diffuseRayColor) << '\n'
             << "Bsdf:" << colStrFunc(bsdfSampleColor) << '\n'
             << "  Lt:" << colStrFunc(lightSampleColor);
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "McrtTotal:" << getTotalRank() << '\n'
         << "RankId:" << rank->getRankId() << (mOnlyDrawCurrRank ? " selected" : " latest") << '\n'
         << showRenderCounter() << '\n'    // 1 line
         << '\n'
         << showPos() << '\n'              // 1 line
         << showSamples() << '\n'          // 3 lines
         << showMaxDepth() << '\n'         // 1 line
         << '\n'
         << showRayTypeSelection() << '\n' // 6 lines
         << '\n'
         << showLineWidth() << '\n'        // 1 line
         << showColor();                   // 5 lines
    return ostr.str();
}

std::string
Manager::genPathVisCurrInfoPanelMsg(const std::function<std::string()>& colReset) const
{
    const bool pathVisActiveFlag = isPathVisActive(); 

    const Rank* const rankObsrPtr = findRank(mCurrRankId);

    std::ostringstream ostr;
    ostr << "rankTotal:" << mTable.size() << '\n';
    if (!pathVisActiveFlag || !rankObsrPtr) {
        ostr << "currRank is EMPTY";
    } else {
        ostr << '\n' 
             << rankObsrPtr->genPathVisCurrInfoPanelMsg(colReset);
    }

    return ostr.str();
}

bool
Manager::getCurrPosPosition(Vec3f& p) const
{
    if (!isPathVisActive()) return false;

    const Rank* const rnkObsrPtr = findRank(mCurrRankId);
    if (!rnkObsrPtr) return false;

    return rnkObsrPtr->getCurrPosPosition(p);
}

std::string
Manager::show() const
{
    std::ostringstream ostr;
    ostr << "Manager {\n"
         << scene_rdl2::str_util::addIndent(showTable()) << '\n'
         << "  mActiveCurrLineMode:" << scene_rdl2::str_util::boolStr(mActiveCurrLineMode) << '\n'
         << "  mCurrRankId:" << mCurrRankId << '\n'
         << "}";
    return ostr.str();
}

std::string
Manager::showTable() const
{
    const int wi = scene_rdl2::str_util::getNumberOfDigits(mTable.size());
    const int wr = scene_rdl2::str_util::getNumberOfDigits(getMaxRankId());

    auto showKey = [&](const unsigned i, const unsigned rankId) {
        std::ostringstream ostr;
        ostr << "i:" << std::setw(wi) << i
             << " rankId:" << std::setw(wr) << rankId;
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "mTable (size:" << mTable.size() << ") {\n";
    unsigned i = 0;
    crawlRankTable([&](const unsigned key, const Rank* rankObsrPtr) {
        ostr << scene_rdl2::str_util::addIndent(showKey(i, rankObsrPtr->getRankId()) + ' ' + rankObsrPtr->show()) << '\n';
        i++;
        return true;
    });
    ostr << "}";
    return ostr.str();
}

std::string
Manager::showRank(const unsigned rankId) const
{
    Rank* rank = findRank(rankId);
    if (!rank) {
        std::ostringstream ostr;
        ostr << "unknown rankId:" << rankId;
        return ostr.str();
    }
    return rank->show();
}

Rank*
Manager::findRank(const unsigned rankId) const
//
// return observer pointer or nullptr
//
{
    auto itr = mTable.find(rankId);
    if (itr != mTable.end()) return itr->second.get(); // found rankInfo, return observer pointer
    return nullptr;
}

Rank*
Manager::findLatestUpdatedRank() const
{
    if (!mTable.size()) return nullptr;

    Rank* latestRank {nullptr};
    crawlRankTable([&](const unsigned rankId, Rank* rank) {
        if (!latestRank || rank->getTimeStamp() > latestRank->getTimeStamp()) {
            latestRank = rank;
        }
        return true;
    });
    return latestRank;
}

unsigned
Manager::getMaxRankId() const
{
    unsigned max = 0;
    crawlRankTable([&](const unsigned key, const Rank* rankObsrPtr) {
        max = std::max(max, key);
        return true;
    });
    return max;
}

void
Manager::parserConfigure()
{
    mParser.description("Manager command");

    mParser.opt("show", "", "show all info",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
    mParser.opt("showRank", "<rankId>", "show rankId info",
                [&](Arg& arg) { return arg.msg(showRank((arg++).as<unsigned>(0))); });
    mParser.opt("rank", "<rankId> ...command...", "rankId command",
                [&](Arg& arg) {
                    const unsigned rankId = (arg++).as<unsigned>(0);
                    if (!findRank(rankId)) return arg.msg("Unknown rankId:" + std::to_string(rankId) + "\n");
                    return mTable[rankId]->getParser().main(arg.childArg());
                });
    mParser.opt("currRank", "...command...", "current rank command",
                [&](Arg& arg) {
                    if (!findRank(mCurrRankId)) return arg.msg("Unknown current rankId:" + std::to_string(mCurrRankId) + "\n");
                    return mTable[mCurrRankId]->getParser().main(arg.childArg());
                });
    mParser.opt("reset", "", "reset pushed vecPacket data",
                [&](Arg& arg) { resetPushedData(); return true; });

    mParser.opt("activeCurrLineToggle", "", "toggle condition of active current line mode",
                [&](Arg& arg) {
                    toggleActiveCurrLineMode();
                    return arg.msg(scene_rdl2::str_util::boolStr(mActiveCurrLineMode) + '\n');
                });
    mParser.opt("deltaCurrRankId", "<delta>", "add delta to current rankId",
                [&](Arg& arg) {
                    deltaCurrentRankId((arg++).as<int>(0));
                    return arg.msg(std::to_string(mCurrRankId) + '\n');
                });
    mParser.opt("getCurrPosXYZ", "", "get currPos's world XYZ",
                [&](Arg& arg) { return arg.msg(cmdGetCurrPosXYZ() + '\n'); });
    mParser.opt("onlyDrawCurrRankToggle", "", "toggle only draw current rank",
                [&](Arg& arg) {
                    toggleOnlyDrawCurrentRank();
                    return arg.msg(scene_rdl2::str_util::boolStr(mOnlyDrawCurrRank));
                });
    mParser.opt("findLatestUpdatedRank", "", "dump latest updated rank",
                [&](Arg& arg) {
                    const Rank* const rank = findLatestUpdatedRank();
                    if (!rank) return arg.msg("Rank is empty\n");
                    return arg.msg(rank->show() + '\n');
                });
}

void
Manager::toggleActiveCurrLineMode()
{
    {
        const Rank* const currRank = findRank(mCurrRankId);
        if (currRank) {
            mActiveCurrLineMode = currRank->getActiveCurrLineMode();
        }
    }

    mActiveCurrLineMode = !mActiveCurrLineMode; 
    if (mActiveCurrLineMode) {
        mOnlyDrawCurrRank = true;

        mCurrRankId = 0;

        crawlRankTable([&](const unsigned key, Rank* rank) {
            if (key == mCurrRankId) rank->setActiveCurrLineMode(true);
            else rank->setActiveCurrLineMode(false);
            return true;
        });
    } else {
        mOnlyDrawCurrRank = false;

        crawlRankTable([&](const unsigned key, Rank* rank) {
            rank->setActiveCurrLineMode(false);
            return true;
        });
    }
}

void
Manager::deltaCurrentRankId(const int delta)
{
    const int maxRankId = getMaxRankId();
    int currId = static_cast<int>(mCurrRankId);
    currId += delta;
    if (currId < 0)              mCurrRankId = maxRankId;
    else if (currId > maxRankId) mCurrRankId = 0;
    else                         mCurrRankId = currId;

    if (mActiveCurrLineMode) {
        crawlRankTable([&](const unsigned key, Rank* rank) {
            if (key == mCurrRankId) rank->setActiveCurrLineMode(true);
            else                    rank->setActiveCurrLineMode(false);
            return true;
        });
    }
}

void
Manager::toggleOnlyDrawCurrentRank()
{
    mOnlyDrawCurrRank = !mOnlyDrawCurrRank;
}

bool
Manager::isPathVisActive() const
{
    const Rank* const rank = findLatestUpdatedRank();
    if (!rank) return false;
    bool flag;
    if (!rank->getPathVis(flag)) return false;
    return flag;
}

std::string
Manager::cmdGetCurrPosXYZ() const
{
    Vec3f p;
    std::ostringstream ostr;
    if (!getCurrPosPosition(p)) {
        ostr << "f 0 0 0";
    } else {
        ostr << "t " << p[0] << ' ' << p[1] << ' ' << p[2];
    }
    return ostr.str();
}

} // namespace vectorPacket
} // namespace mcrt_dataio
