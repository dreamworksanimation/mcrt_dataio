// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TelemetryOverlay.h"

#include <scene_rdl2/common/grid_util/Arg.h>
#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/grid_util/VectorPacket.h>
#include <scene_rdl2/common/grid_util/VectorPacketDictionary.h>
#include <scene_rdl2/common/rec_time/RecTime.h>

#include <memory>
#include <string>

//
// Policy for decoding and rendering vecPacket data
//
// ClientReceiverFb receives VectorPacket data as one of the buffer entries in a progressiveFrame message.
// This data is initially stored as raw binary within the corresponding Rank without being decoded.
// Decoding is only required if ClientReceiverFb is performing telemetry display with the panel name
// "pathVis"; in all other cases decoding is unnecessary.
//
// Within the Rank, the stored data (mData) is decoded only when the telemetry draw function is called.
// At that point, various data encoded internally are extracted. In principle, these decoded values are
// not repackaged into another data structure; instead, they are directly consumed for rendering.
// The received binary vectorPacket data (mData) is retained until the next vectorPacket arrives.
// During this period, for every draw action across potentially multiple frames, the data is decoded
// repeatedly and rendered directly. This decode-per-frame approach trades some overhead for simplicity.
//
// An alternative approach would be to decode each newly received vectorPacket once, convert it into a
// data structure optimized for rendering, and then let all subsequent telemetry rendering access this
// structure. In that case, rendering from the second frame onward would be very fast, but the first
// frame would incur the additional cost of decoding plus data-structure construction. Since the most
// critical performance requirement is the very first frame of rendering, and the later frames can
// tolerate some overhead without harming interactivity, the current policy deliberately skips the
// conversion step and renders directly from decoded data. Consequently, the decode process is repeated
// every frame, but decoding in ValueContainerDequeue is extremely fast and the overhead is acceptable
// at present.
//
// Because vectorPacket data is rendered in this manner, additional care is needed when handling updates
// such as current line changes, current position changes, and picking operations.
//
// Normally, picking is implemented by searching inside a pre-built data structure to compare point
// coordinates with the target pixel coordinates. Such an approach requires efficient search mechanisms.
// However, in the current implementation, all line segment information is already scanned each frame
// for rendering. Therefore, picking checks can be performed during this traversal, eliminating the need
// for a separate high-performance search. As a result, a stable picking decision can be made simply as
// a byproduct of rendering. At present, this is the approach being used.
// After rendering is completed, if necessary, current information such as the selected line segment can
// be updated in place. This allows current state updates and display without requiring redundant searches
// over a database-like structure.
//
// Right now, there are two modes for line drawing: a current line mode, which visually highlights the
// current state, and a non-current line mode, which does not. In non-current line mode, all lines are
// drawn with the same brightness and width. In current line mode, extra effort is made to ensure the
// current line stands out. These two modes are intended to be switchable by the user depending on the
// situation.
//

namespace mcrt_dataio {

namespace telemetry {
    class Display;
}

namespace vectorPacket {

class DrawLine
//
// This class keeps a single 2D line drawing segment info
//
{
public:
    using Vec2ui = scene_rdl2::math::Vec2<unsigned>;
    using C3 = telemetry::C3;
    using PosType = scene_rdl2::grid_util::VectorPacketLineStatus::PosType;

    DrawLine() {}

    void set(const Vec2ui& start,
             const Vec2ui& end,
             const scene_rdl2::grid_util::VectorPacketLineStatus& status,
             const C3& c3,
             const unsigned char alpha,
             const float width,
             const unsigned nodeId,
             const unsigned lineId)
    //
    // lineId is a drawing order ID during the single frame drawing. This starts from 0.
    // If the camera is updated and change the topology of 2D line segments will be changed lineId.
    //
    {
        mStart = start;
        mEnd = end;
        mStatus = status;
        mC3 = c3;
        mAlpha = alpha;
        mWidth = width;
        mNodeId = nodeId;
        mIsCurrPosStart = isPosType(mStatus.getStartPosType());
        mLineId = lineId;
    }

    void setActive(const bool flag) { mActive = flag; }
    bool getActive() const { return mActive; }

    const Vec2ui& getStart() const { return mStart; }
    const Vec2ui& getEnd() const { return mEnd; }
    const scene_rdl2::grid_util::VectorPacketLineStatus& getStatus() const { return mStatus; }
    PosType getStartPosType() const { return mStatus.getStartPosType(); }
    PosType getEndPosType() const { return mStatus.getEndPosType(); }
    const telemetry::C3& getC3() const { return mC3; }
    unsigned char getAlpha() const { return mAlpha; }
    float getWidth() const { return mWidth; }
    unsigned getNodeId() const { return mNodeId; }
    unsigned getLineId() const { return mLineId; }

    // Generate strings for the telemetry panel regarding this line
    std::string genPathVisCurrInfoPanelMsg(const std::function<std::string()>& colReset) const;

    //------------------------------
    //
    // What is a CurrPos?
    // If the start/end point of a 2D line segment has one of the PosType::START, ISECT, and END,
    // these start/end points can be a CurrPos. CurrPos is one of the start or end points. This
    // indicates the current position of line points which used to indicate current vertex information,
    // like surface property.
    // If the line start/end point has PosType::UNKNOWN this means this point is generated by a clipping
    // operation, and this does not have any vertex information. This UNKNOWN PosType start/end point is
    // ignored from all the picking/selecting operations.
    // If both of the start/end point has UNKNOWN condition, this DrawLine data itself is not generated
    // (i.e. has mActive - false condition). If mActive = true, one or both start/end points have
    // non UNKNOWN PosType.
    //

    // Switch back and forth between start and end point if the point has one of the START, ISECT, and
    // END position condition.
    void toggleActiveCurrPos();

    bool isCurrPosActive() const { return isPosType(getCurrPosType()); }
    bool isValidStartActiveCurrPos() const;
    bool isValidEndActiveCurrPos() const;

    void setCurrPosStart(const bool st) { mIsCurrPosStart = st; }
    void adjustCurrPos(); // Adjust CurrPos condition automatically and properly pick one of the start or end

    //------------------------------

    std::string show() const; 
    std::string showCurrPos() const;

private:

    PosType getCurrPosType() const { return mIsCurrPosStart ? getStartPosType() : getEndPosType(); }
    static bool isPosType(const PosType posType)
    {
        return posType == PosType::START || posType == PosType::ISECT || posType == PosType::END;
    }

    //------------------------------

    bool mActive {false}; // drawing this data ON or OFF

    Vec2ui mStart;
    Vec2ui mEnd;
    scene_rdl2::grid_util::VectorPacketLineStatus mStatus;
    telemetry::C3 mC3 {0,0,0};
    unsigned char mAlpha {0};
    float mWidth {0.0f};
    unsigned mNodeId {0};
    unsigned mLineId {0};

    bool mIsCurrPosStart {true}; // true:CurrPos_is_mStart false:CurrPos_is_mEnd
}; // class DrawLine

class Rank
//
// This class operates a single backend computation of vecPacket data.
//
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Color = scene_rdl2::math::Color;
    using C3 = telemetry::C3;
    using DataShPtr = std::shared_ptr<uint8_t>;
    using Overlay = telemetry::Overlay;
    using Parser = scene_rdl2::grid_util::Parser;
    using PosType = scene_rdl2::grid_util::VectorPacketLineStatus::PosType;
    using Vec2ui = scene_rdl2::math::Vec2<unsigned>;
    using Vec3f = scene_rdl2::math::Vec3f;

    Rank(const unsigned rankId, telemetry::Display* obsrPtr)
        : mRankId {rankId}
        , mTelemetryDisplayObsrPtr {obsrPtr}
    {
        configureActions();
        parserConfigure();
    }

    void configureActions(); // Configure all the actions for decoding vectorPacket data

    // Decoding data but skip all drawing action (like line, outline, color, width, ...)
    // if setSkipDraw = true. Otherwise do decode and drawing.
    void setSkipDraw(const bool st) { mSkipDraw = st; }

    unsigned getRankId() const { return mRankId; }
    float getTimeStamp() const { return mTimeStamp; }

    void resetPushedData() { mData.reset(); mDataSize = 0; }
    void pushData(const float timeStamp, DataShPtr ptr, const size_t dataSize)
    {
        mTimeStamp = timeStamp;
        mData = ptr; mDataSize = dataSize;
        mDecodeCounter = 0;
    }

    void decode(); // Throw exception(std::string) if error

    //------------------------------
    //
    // Get dictionary information APIs
    //
    // Get dictionary info from VectorPacketDictEntry. All the values are valid after the
    // initial decoding action; otherwise returns false.
    //
    bool getRenderCounter(unsigned& counter) const;
    bool getHostname(std::string& hostName) const; 
    bool getPathVis(bool& flag) const;
    bool getPos(Vec2ui& pos) const;
    bool getMaxDepth(unsigned& maxDepth) const;
    bool getSamples(unsigned& pixelSamples,
                    unsigned& lightSamples,
                    unsigned& bsdfSamples) const;
    bool getRayTypeSelection(bool& useSceneSamples,
                             bool& occlusionRaysOn,
                             bool& specularRaysOn,
                             bool& diffuseRaysOn,
                             bool& bsdfSamplesOn,
                             bool& lightSamplesOn) const;
    bool getColor(Color& cameraRayColor,
                  Color& specularRayColor,
                  Color& diffuseRayColor,
                  Color& bsdfSampleColor,
                  Color& lightSampleColor) const;
    bool getLineWidth(float& width) const;

    bool getCamPos(Vec3f& pos) const;
    bool getCamRayIsectSfPos(std::vector<Vec3f>& tbl) const;

    //------------------------------

    void setActiveCurrLineMode(const bool flag) { mCurrLine.setActive(flag); }
    bool getActiveCurrLineMode() const { return mCurrLine.getActive(); }

    void toggleActiveCurrPos() { mCurrLine.toggleActiveCurrPos(); }

    // Generate strings for the telemetry panel
    std::string genPathVisCurrInfoPanelMsg(const std::function<std::string()>& colReset) const;

    bool getCurrPosPosition(Vec3f& p) const;

    Parser& getParser() { return mParser; }

    std::string show() const;
    std::string showSimple() const;
    std::string showCurrLine() const;
    std::string showNodeTbl() const;
    std::string showNode(const unsigned nodeId) const;
    std::string showVtxTbl() const;
    std::string showCurrPos() const;

private:
    using Vec3uc = scene_rdl2::math::Vec3<unsigned char>;
    using Vec2i = scene_rdl2::math::Vec2i;

    // This defines the logic of how to select the current line segment.
    // NAIVE type should not be used for the release version and should only be used
    // for debugging purposes.
    enum class SelectType : int {
        NAIVE, // For debug. This is a simple current lineId based solution and this might
               // pick up every 2D line segment as a current line even if line segment does
               // not have valid endpoints. Valid endpoint would be pure START, ISECT, or
               // END points.
        STD    // Pick up only the segment that has a valid endpoint, and this skips all
               // the line segment which does not have a valid endpoint as the current line.
    };

    // The operation mode for decoding line data is only used if mSelectType is STD.
    enum class DecodeCurrLineMode : int {
        NO_ACTION,
        NEXT, // Pick up the next possible line data
        PREV, // Pick up the previous possible line data
        PICK  // Pick up the current line by pixel position (for mouse pick operation)
    };

    const scene_rdl2::grid_util::VectorPacketDictionary& getDictionary() const
    {
        return mVectorPacketDequeue.getDictionary();
    }

    template <typename T>
    const T&
    getDictEntry(const scene_rdl2::grid_util::VectorPacketDictEntry::Key key) const
    {
        return static_cast<const T&>(getDictionary().getDictEntry(key));
    }

    template <typename T, typename F>
    bool
    getDictValue(const scene_rdl2::grid_util::VectorPacketDictEntry::Key key, F callBack) const
    {
        const auto& dictEntry = getDictEntry<T>(key);
        if (!dictEntry.getActive()) return false;
        callBack(dictEntry);
        return true;
    }

    void drawCurrLine() const;
    void drawCurrPosVtx() const;

    std::string genPathVisCurrNodeInfo() const;
    std::string genPathVisCurrPosInfo(const unsigned vtxId) const;

    bool getCurrPosVtxId(unsigned& vtxId, std::string* errMsg) const;

    void parserConfigure();
    template <typename T>
    bool parseSingleArgCmd(Arg& arg, T& target)
    {
        using namespace scene_rdl2::str_util;
        if (arg() == "show") arg++;
        else target = (arg++).as<T>(0);
        if constexpr (std::is_same_v<T, bool>) return arg.msg(boolStr(target) + '\n');
        else                                   return arg.msg(std::to_string(target) + '\n');
    }
    void deltaCurrLineId(const int delta);
    void setPickCurrLineParam(const int sx, const int sy);

    static std::string selectTypeStr(const SelectType type);

    //------------------------------

    bool mRunDecode {true};

    float mTimeStamp {0.0f};
    size_t mDecodeCounter {0};

    size_t mDataSize {0};
    DataShPtr mData {nullptr};

    unsigned mRankId {0};
    telemetry::Display* mTelemetryDisplayObsrPtr {nullptr};
    scene_rdl2::grid_util::VectorPacketDequeue mVectorPacketDequeue;

    bool mSkipDraw {false};

    float mCurrWidth {1.0f};
    telemetry::C3 mCurrC3 {255, 255, 255};
    unsigned char mCurrAlpha {255};

    SelectType mSelectType {SelectType::STD};

    std::vector<scene_rdl2::grid_util::VectorPacketNode> mNodeTbl;
    std::vector<Vec3f> mVertexTbl;

    bool mOverrideWidthSw {false};
    float mOverrideWidth {1.0f};

    unsigned mDrawLineTotal {0}; // total number of lines to draw
    unsigned mCurrLineId {~static_cast<unsigned>(0)};
    DrawLine mCurrLine;
    DecodeCurrLineMode mDecodeCurrLineMode {DecodeCurrLineMode::NO_ACTION};
    Vec2i mPickCurrPos;
    unsigned mPickCurrDist2Work {~static_cast<unsigned>(0)};
    unsigned mPickThreshDist {10}; // pixels
    bool mValidCurrLine {false}; // for mDecodeCurrLineMode == NEXT, PREV

    Parser mParser;
};

class Manager
//
// This class keeps all the vectorPacket related information for all the Ranks.
//
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using Color = scene_rdl2::math::Color;
    using DataShPtr = std::shared_ptr<uint8_t>;
    using Parser = scene_rdl2::grid_util::Parser;
    using Vec2ui = scene_rdl2::math::Vec2<unsigned>;
    using Vec3f = scene_rdl2::math::Vec3f;

    Manager()
    {
        mRecTime.start();
        parserConfigure();
    };

    void setTelemetryDisplay(telemetry::Display* obsrPtr) { mTelemetryDisplayObsrPtr = obsrPtr; }

    void resetPushedData();
    void pushData(const unsigned rankId, DataShPtr ptr, const size_t dataSize);

    // Throw an exception(std::string) if an error occurs
    void decodeAll();
    void decode(const unsigned rankId);

    size_t getTotalRank() const { return mTable.size(); }
    using ColStrFunc = std::function<std::string(const Color& c)>;
    std::string genPathVisCtrlInfoPanelMsg(const ColStrFunc& colStrFunc) const;
    std::string genPathVisCurrInfoPanelMsg(const std::function<std::string()>& colReset) const;

    bool getCurrPosPosition(Vec3f& p) const;

    Parser& getParser() { return mParser; }

    std::string show() const;
    std::string showTable() const;
    std::string showRank(const unsigned rankId) const;

private:
    Rank* findRank(const unsigned rankId) const; // return observer pointer or nullptr
    Rank* findLatestUpdatedRank() const;  // return observer pointer or nullptr
    unsigned getMaxRankId() const;

    template <typename F>
    bool
    crawlRankTable(F func) const
    {
        for (const auto& [key, uqPtr] : mTable) {
            if (uqPtr.get()) {
                  if (!func(key, uqPtr.get())) return false; // early exit
            }
        }
        return true;
    }

    void parserConfigure();
    void toggleActiveCurrLineMode();
    void toggleCurrentRankDrawMode();
    void deltaCurrentRankId(const int delta);
    void toggleOnlyDrawCurrentRank();
    
    bool isPathVisActive() const; // valid after decodeAll()

    std::string cmdGetCurrPosXYZ() const;

    //------------------------------

    scene_rdl2::rec_time::RecTime mRecTime; // time since construction of this object

    telemetry::Display* mTelemetryDisplayObsrPtr {nullptr};

    std::unordered_map<unsigned, std::unique_ptr<Rank>> mTable; // mTable[rankId]
    bool mActiveCurrLineMode {false};
    bool mOnlyDrawCurrRank {false};
    unsigned mCurrRankId {0};

    Parser mParser;
};

} // namespace vectorPacket
} // namespace mcrt_dataio
