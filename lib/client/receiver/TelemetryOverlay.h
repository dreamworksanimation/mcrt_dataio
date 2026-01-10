// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ScanConvertPolygon.h"

//#define SC_TIMELOG // scanconversion time log

#ifdef SC_TIMELOG
#include "ScTimeLog.h"
#endif // end of SC_TIMELOG

#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/math/BBox.h>
#include <scene_rdl2/common/math/Color.h>
#include <scene_rdl2/common/math/Math.h>

#include <algorithm> // clamp
#include <cctype> // isspace
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

// This directive set memory pool logic ON for telemetry overlay draw items and this should be
// enabled for the release version. Disabling this directive is only used for performance
// comparison purposes.
#define ENABLE_MEMPOOL

namespace mcrt_dataio {
namespace telemetry {

using FreeTypeBBox = scene_rdl2::math::BBox<FT_Vector>;

class C3
//
// 8bit RGB Color definition for TelemetryOverlay
//
{
public:
    C3() = default;
    C3(const unsigned char r, const unsigned char g, const unsigned char b) : mR(r), mG(g), mB(b) {}
    C3(const C3& c3) : mR(c3.mR), mG(c3.mG), mB(c3.mB) {}
    C3(const scene_rdl2::math::Color c) : mR(ftoUC(c.r)), mG(ftoUC(c.g)), mB(ftoUC(c.b)) {}

    bool isBlack() const { return (mR == 0x0 && mG == 0x0 && mB == 0x0); }

    C3
    bestContrastCol() const
    {
        const float l = luminance();
        const float contrastWhite = (1.0f + 0.05f) / (l + 0.05f);
        const float contrastBlack = (l + 0.05f) / (0.0f + 0.05f);
        return (contrastWhite > contrastBlack) ? C3(255, 255, 255) : C3(0, 0, 0);
    }

    float
    luminance() const
    {
        return (0.2126f * static_cast<float>(mR) / 255.0f +
                0.7152f * static_cast<float>(mG) / 255.0f +
                0.0722f * static_cast<float>(mB) / 255.0f);
    }

    void
    scale(const float s)
    {
        mR = static_cast<unsigned char>(static_cast<float>(mR) * s);
        mG = static_cast<unsigned char>(static_cast<float>(mG) * s);
        mB = static_cast<unsigned char>(static_cast<float>(mB) * s);
    }

    static unsigned char
    ftoUC(const float v)
    {
        if (v <= 0.0f) return 0;
        else if (v >= 1.0f) return 255;
        else return static_cast<unsigned char>(v * 256.0f);
    }

    unsigned char mR {0};
    unsigned char mG {0};
    unsigned char mB {0};

    std::string
    show() const
    {
        std::ostringstream ostr;
        ostr << '('
             << std::setw(3) << static_cast<int>(mR) << ","
             << std::setw(3) << static_cast<int>(mG) << ","
             << std::setw(3) << static_cast<int>(mB)
             << ')';
        return ostr.str();
    }

    std::string panelStr() const;
};

//------------------------------------------------------------------------------------------

class FontCacheItem
//
// Font bit mask information cache data. We want to avoid multiple times same font bitmap conversion.
//
{
public:
    FontCacheItem(const char c,
                  const FT_Bitmap& bitmap,
                  const unsigned bitmapLeft, const unsigned bitmapTop, const unsigned advanceX);

    unsigned char get(const int bx, const int by) const { return mBuffer[by * mPitch + bx]; }

    bool isSpace() const { return std::isspace(static_cast<unsigned char>(mC)); }

    unsigned getRows() const { return mRows; }
    unsigned getWidth() const { return mWidth; }
    unsigned getBitmapLeft() const { return mBitmapLeft; }
    unsigned getBitmapTop() const { return mBitmapTop; }
    unsigned getAdvanceX() const { return mAdvanceX; }

private:
    char mC {0x0};

    //   FreeType coordinate : Fixed point position value and Y flip
    //      -----> +X
    //    |            +<------------------->+ mAdvanceX
    //    |               +<------>+ mPitch, mWidth
    //    V            +<>+ mBitmapLeft
    //   +Y  m         +.../..........+      +..............+
    //       B +    +  :  +--------+  :      :              :
    //       i ^    ^  :  |*      *|  :      :              :
    //       t |  m |  :  | *    * |  :      :              :
    //       m |  R |  :  |  *  *  |  :      :     next     :
    //       a |  o |  :  |   **   |  :      :     Font     :
    //       p |  w |  :  |   *    |  :      :              :
    //       T |  s V  :  |  *     |  :      :              :
    //       o V    +  :  +-*---\--+  :      :              :
    //       p +       +.........\....+      +..............+
    //                  \         \--- Fontcacheitem
    //                   \--- OverlayCharItem : (mFontBasePos.x, mFontBasePos.y)
    //
    unsigned mRows {0};
    unsigned mWidth {0};
    unsigned mPitch {0};
    unsigned mBitmapLeft {0};
    unsigned mBitmapTop {0};
    unsigned mAdvanceX {0};

    std::vector<unsigned char> mBuffer;
};

//------------------------------------------------------------------------------------------

class Font
//
// Single Font data information for FreeType
//
{
public:
    using FontCacheItemShPtr = std::shared_ptr<FontCacheItem>;

    Font(const std::string& fontTTFFileName, const int fontSizePoint)
        : mFontTTFFileName(fontTTFFileName)
        , mFontSizePoint(fontSizePoint)
    {
        setupFontFace();
    }

    const std::string& getFontTTFFileName() const { return mFontTTFFileName; }

    int getFontSizePoint() const { return mFontSizePoint; }
    const FT_Face& getFace() const { return mFace; }

    FontCacheItemShPtr getFontCacheItem(const char c);

    float getBgYAdjustScale() const { return mBgYAdjustScale; }

    // fixedPoint FreeType coordinate value conversion from/to int
    static FT_Pos iToftPos(const int v) { return static_cast<FT_Pos>(v * 64); }
    static int ftPosToi(const FT_Pos v) { return static_cast<int>(v) / 64; }

private:
    void setupFontFace(); // throw scene_rdl2::except::RuntimeError

    //------------------------------

    const std::string mFontTTFFileName;
    int mFontSizePoint {0};

    FT_Library mFtLibrary;
    FT_Face mFace;

    float mBgYAdjustScale {0.0f};

    std::unordered_map<unsigned, FontCacheItemShPtr> mFontCacheMap;
};

//------------------------------------------------------------------------------------------

class OverlayCharItem
//
// Single char data for overlay draw action. We do drawing logic in 2 phases. 1st phase creates data sets
// of all draw char data (OverlayCharItem) and 2nd phase rasterize all of them and alpha blend.
//            
{
public:
    using FontCacheItemShPtr = std::shared_ptr<FontCacheItem>;

    OverlayCharItem() = default;

    void set(FontCacheItemShPtr fontCacheItem,
             const FT_Vector& fontPos,
             const unsigned fontHeight,
             const float bgYAdjustScale,
             const C3& fgC3,
             const C3& bgC3)
    {
        mFontCacheItem = fontCacheItem;

        mFontBasePos = fontPos;
        mFontSize = FT_Vector{fontCacheItem->getAdvanceX(), Font::iToftPos(fontHeight)};
        mFontDataPos = FT_Vector {mFontBasePos.x + Font::iToftPos(fontCacheItem->getBitmapLeft()),
                                  mFontBasePos.y - Font::iToftPos(fontCacheItem->getBitmapTop())};
        mBgYAdjustScale = bgYAdjustScale;
        mFgC3 = fgC3;
        mBgC3 = bgC3;
    }

    unsigned getAdvanceX() const { return mFontCacheItem->getAdvanceX(); }

    FontCacheItemShPtr getFontCacheItem() const { return mFontCacheItem; }

    unsigned getBaseX() const { return Font::ftPosToi(mFontBasePos.x); }
    unsigned getBaseY() const { return Font::ftPosToi(mFontBasePos.y); }
    unsigned getWidth() const {
        return static_cast<unsigned>(Font::ftPosToi(mFontBasePos.x + mFontSize.x) - getBaseX());
    }
    unsigned getHeight() const {
        return static_cast<unsigned>(Font::ftPosToi(mFontBasePos.y + mFontSize.y) - getBaseY());
    }

    unsigned getPosX() const { return Font::ftPosToi(mFontDataPos.x); }
    unsigned getPosY() const { return Font::ftPosToi(mFontDataPos.y); }

    unsigned getStepX() const { return Font::ftPosToi(getAdvanceX()); }

    float getBgYAdjustScale() const { return mBgYAdjustScale; }

    const C3& getFgC3() const { return mFgC3; }
    const C3& getBgC3() const { return mBgC3; }

    FreeTypeBBox getBBox() const;

    void move(const int deltaX, const int deltaY);

    // display framebuffer coord as well if you set non zero winHeight
    std::string show(const unsigned winHeight = 0) const;

private:
    FontCacheItemShPtr mFontCacheItem {nullptr};

    //   FreeType coordinate : Fixed point position value and Y flip
    //      -----> +X 
    //    |
    //    |      +<------>+ FontCacheItem::getWidth()
    //    V   +<------------>+ mFontSize.x
    //   +Y        /--- (mFontDataPos.x, mFontDataPos.y) -> getPosX(),  getPosY()
    //        +---/----------+  +
    //        |  +........+  |  ^              +
    //        |  :*      *:  |  |              ^
    //        |  : *    * :  |  |              |
    //        |  :  *  *  :  |  |mFontSize.y   |
    //        |  :   **   :  |  |              |FontCacheitem::getRaws()
    //        |  :   *    :  |  |              |
    //        |  :  *     :  |  |              V
    //        |  +.*...\..+  |  V              +
    //        +---------\----+  +
    //         \         \--- FontCacheItem
    //          \--- (mFontBasePos.x, mFontBasePos.y) -> getBaseX(), getBaseY()
    //
    FT_Vector mFontBasePos; // char position x y (FreeType coordinate value)
    FT_Vector mFontSize;    // char width and height (FreeType coordinate value)
    FT_Vector mFontDataPos; // font data position, (FreeType coordinate value)

    float mBgYAdjustScale {0.0f};

    C3 mFgC3 {0, 0, 0}; // foreground (font) color 0 ~ 255
    C3 mBgC3 {0, 0, 0}; // background color 0 ~ 255
};

class Overlay;

class OverlayStrItem
//
// Single strings that include multiple char items.
// All char items are organized as multiple string item data.
//
{
public:
    using OverlayCharItemShPtr = std::shared_ptr<OverlayCharItem>;

    OverlayStrItem() = default;

    void resetCharItemArray(Overlay& overlay);

    void set(Overlay& overlay,
             Font& font,
             const unsigned startX, const unsigned startY, const unsigned overlayHeight,
             const std::string& str, const C3& c3);

    void setupAllCharItems(std::vector<OverlayCharItemShPtr>& outCharItemArray);

    unsigned getFirstCharStepX() const; // return first char's stepX

    FreeTypeBBox getBBox() const;

    void move(const int deltaX, const int deltaY);

private:

    bool entryNewCharItem(Overlay& overlay,
                          Font& font,
                          const FT_Vector& fontPos,
                          const char c,
                          const C3& fgC3,
                          const C3& bgC3);

    //------------------------------

    std::string mStr;
    unsigned mStartX {0};
    unsigned mStartY {0};
    unsigned mOverlayHeight {0};

    std::vector<OverlayCharItemShPtr> mCharItemArray;
};

class OverlayBoxItem
{
public:
    using BBox2i = scene_rdl2::math::BBox2i;

    OverlayBoxItem() = default;

    void set(const BBox2i& bbox, const C3& c, const unsigned char alpha)
    {
        mBBox = bbox;
        mC = c;
        mAlpha = alpha;
    }

    const BBox2i& getBBox() const { return mBBox; }
    const C3& getC() const { return mC; }
    unsigned char getAlpha() const { return mAlpha; }

private:
    BBox2i mBBox {scene_rdl2::math::Vec2i {0, 0}, scene_rdl2::math::Vec2i {0, 0}};
    C3 mC {0, 0, 0};
    unsigned char mAlpha {0};
};

class OverlayBoxOutlineItem
{
public:
    using BBox2i = scene_rdl2::math::BBox2i;

    OverlayBoxOutlineItem() = default;

    void set(const BBox2i& bbox, const C3& c, const unsigned char alpha)
    {
        mBBox = bbox;
        mC = c;
        mAlpha = alpha;
    }

    const BBox2i& getBBox() const { return mBBox; }
    const C3& getC() const { return mC; }
    unsigned char getAlpha() const { return mAlpha; }

private:
    BBox2i mBBox {scene_rdl2::math::Vec2i {0, 0}, scene_rdl2::math::Vec2i {0, 0}};
    C3 mC {0, 0, 0};
    unsigned char mAlpha {0};
};

class OverlayVLineItem
{
public:
    OverlayVLineItem() = default;

    void set(const unsigned x, const unsigned minY, const unsigned maxY,
             const C3& c, const unsigned char alpha)
    {
        mX = x;
        mMinY = minY;
        mMaxY = maxY;
        mC = c;
        mAlpha = alpha;
    }

    unsigned getX() const { return mX; }
    unsigned getMinY() const { return mMinY; }
    unsigned getMaxY() const { return mMaxY; }
    const C3& getC() const { return mC; }
    unsigned char getAlpha() const { return mAlpha; }

private:
    unsigned mX {0};
    unsigned mMinY {0};
    unsigned mMaxY {0};

    C3 mC {0, 0, 0};
    unsigned char mAlpha {0};
};

class OverlayHLineItem
{
public:
    OverlayHLineItem() = default;

    void set(const unsigned minX, const unsigned maxX, const unsigned y,
             const C3& c, const unsigned char alpha)
    {
        mMinX = minX;
        mMaxX = maxX;
        mY = y; 
        mC = c;
        mAlpha = alpha;
    }

    unsigned getMinX() const { return mMinX; }
    unsigned getMaxX() const { return mMaxX; }
    unsigned getY() const { return mY; }
    const C3& getC() const { return mC; }
    unsigned char getAlpha() const { return mAlpha; }

private:
    unsigned mMinX {0};
    unsigned mMaxX {0};
    unsigned mY {0};

    C3 mC {0, 0, 0};
    unsigned char mAlpha {0};
};

class OverlayLineItem
{
public:
    OverlayLineItem() = default;

    void set(const unsigned sx, const unsigned sy, const unsigned ex, const unsigned ey,
             const float width,
             const bool drawStartPoint,
             const float radiusStartPoint,
             const bool drawEndPoint,
             const float radiusEndPoint,
             const C3& c, const unsigned char alpha)
    {
        mSx = sx;
        mSy = sy;
        mEx = ex;
        mEy = ey;
        mWidth = width;
        mDrawStartPoint = drawStartPoint;
        mRadiusStartPoint = radiusStartPoint;
        mDrawEndPoint = drawEndPoint;
        mRadiusEndPoint = radiusEndPoint;
        mC = c;
        mAlpha = alpha;
    }

    unsigned getSx() const { return mSx; }
    unsigned getSy() const { return mSy; }
    unsigned getEx() const { return mEx; }
    unsigned getEy() const { return mEy; }
    float getWidth() const { return mWidth; }
    bool getDrawStartPoint() const { return mDrawStartPoint; }
    float getRadiusStartPoint() const { return mRadiusStartPoint; }
    bool getDrawEndPoint() const { return mDrawEndPoint; }
    float getRadiusEndPoint() const { return mRadiusEndPoint; }
    const C3& getC() const { return mC; }
    unsigned char getAlpha() const { return mAlpha; }

private:
    unsigned mSx {0};
    unsigned mSy {0};
    unsigned mEx {0};
    unsigned mEy {0};
    float mWidth {0.0f};

    bool mDrawStartPoint {false};
    float mRadiusStartPoint {0.0f};

    bool mDrawEndPoint {false};
    float mRadiusEndPoint {0.0f};

    C3 mC {0, 0, 0};
    unsigned char mAlpha {0};
};

class OverlayCircleItem
{
public:
    OverlayCircleItem() = default;

    void set(const unsigned x, const unsigned y, const float radius, const float width,
             const C3& c, const unsigned char alpha)
    {
        mX = x;
        mY = y;
        mRadius = radius;
        mWidth = width;
        mC = c;
        mAlpha = alpha;
    }

    unsigned getX() const { return mX; }
    unsigned getY() const { return mY; }
    float getRadius() const { return mRadius; }
    float getWidth() const { return mWidth; }
    const C3& getC() const { return mC; }
    unsigned char getAlpha() const { return mAlpha; }

private:
    unsigned mX {0};
    unsigned mY {0};
    float mRadius {0.0f};
    float mWidth {0.0f};

    C3 mC {0, 0, 0};
    unsigned char mAlpha {0};
};

template <typename T>
class OverlayDrawItemMemManager
{
public:
    using TShPtr = std::shared_ptr<T>;

    TShPtr getMemItem()
    {
        if (mItemMemPool.empty()) return std::make_shared<T>();
        TShPtr item = mItemMemPool.front();
        mItemMemPool.pop_front();
        return item;
    }
    void setMemItem(TShPtr ptr)
    {
#       ifdef ENABLE_MEMPOOL
        mItemMemPool.push_front(ptr);
#       else // !ENABLE_MEMPOOL
        ptr.reset();
#       endif // end of !ENABLE_MEMPOOL
    }

    void updateMax() { if (mItemMemPool.size() > mMaxItemMemPool) mMaxItemMemPool = mItemMemPool.size(); }
    size_t getMemUsage() const { return mMaxItemMemPool * sizeof(T); } // byte

    std::string showSize() const
    {
        std::ostringstream ostr;
        ostr << "poolSizeMax:" << mMaxItemMemPool << " (" << scene_rdl2::str_util::byteStr(getMemUsage()) << ")";
        return ostr.str();
    }

private:
    unsigned mMaxItemMemPool {0};
    std::deque<TShPtr> mItemMemPool;
};

class Overlay
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using BBox2i = scene_rdl2::math::BBox2i;
    using OverlayCharItemShPtr = std::shared_ptr<OverlayCharItem>;
    using Parser = scene_rdl2::grid_util::Parser;
    using Vec2f = scene_rdl2::math::Vec2f;

    enum class Align {
        SMALL,  // left or bottom
        MIDDLE, // center
        BIG     // right or top
    };

    enum class LineDrawType {
        BRESENHAM,
        DDA,
        WU,
        WU_HQ, // experimental
        OUTLINE_STROKE,
        OUTLINE_STROKE2, // experimental
        HYBRID
    };

    Overlay() { parserConfigure(); }
    Overlay(const unsigned width, const unsigned height)
    {
        resize(width, height);
        clear({0, 0, 0}, 0, true);

        // for the performance comparison purposes
        mScanConvertPolygon = std::make_unique<ScanConvertPolygon>(*this, 8, 8);

        parserConfigure();
    }

    LineDrawType getLineDrawType() const { return mLineDrawType; }

    inline void resize(const unsigned width, const unsigned height); // no clear internally just change size
    void clear(const C3& c3, const unsigned char alpha, const bool doParallel);
    inline void pixFill(const unsigned x, const unsigned y, const C3& c3, const unsigned char alpha);
    inline void pixMaxFill(const unsigned x, const unsigned y, const C3& c3, const unsigned char alpha);
    void boxFill(const BBox2i& bbox, const C3& c3, const unsigned char alpha, const bool doParallel);
    void boxOutline(const BBox2i& bbox, const C3& c3, const unsigned char alpha, const bool doParallel);
    void polygonFill(const std::vector<Vec2f>& polygon,
                     const unsigned xSubSamples, const unsigned ySubSamples,
                     const C3& col3, const unsigned char alpha);
    void circleFill(const Vec2f& center, const float radius, const C3& c3, const unsigned char alpha);
    void circle(const Vec2f& center, const float radius, const float width, const C3& c3, const unsigned char alpha);
    void vLine(const unsigned x, const unsigned minY, const unsigned maxY,
               const C3& c3, const unsigned char alpha); // no parallel option
    void hLine(const unsigned minX, const unsigned maxX, const unsigned y,
               const C3& c3, const unsigned char alpha); // no parallel option
    void line(const Vec2f& s, const Vec2f& e, const float width,
              const bool drawStartPoint, const float radiusStartPoint,
              const bool drawEndPoint, const float radiusEndPoint,
              const C3& c3, const unsigned char alpha) { // no parallel option
        (this->*mLineDrawFunc)(s, e, width, c3, alpha);
        if (drawStartPoint) circleFill(s, radiusStartPoint, c3, alpha);
        if (drawEndPoint) circleFill(e, radiusEndPoint, c3, alpha);
    }
    void fill(const unsigned char r, const unsigned char g, const unsigned char b,
              const unsigned char a); // fill whole fb for debug

    unsigned getWidth() const { return mWidth; }
    unsigned getHeight() const { return mHeight; }

    unsigned getMaxYLines(const Font& font, unsigned& offsetBottomPixY, unsigned& stepPixY) const;

    OverlayCharItemShPtr getNewOverlayCharItem() { return mCharItemMemMgr.getMemItem(); }
    void restoreOverlayCharItemMem(OverlayCharItemShPtr ptr) { mCharItemMemMgr.setMemItem(ptr); }

    void drawClearAllItems();
    void drawFlushAllItems(const bool doParallel);

    bool drawStr(Font& font,
                 const unsigned startX,
                 const unsigned startY,
                 const std::string& str,
                 const C3& c3,
                 std::string& error);
    void drawBox(const BBox2i& box, const C3& c3, const unsigned char alpha); // push into mDrawBoxArray
    void drawBoxBar(const BBox2i& box, const C3& c3, const unsigned char alpha); // push into mDrawBoxBarArray
    void drawBoxOutline(const BBox2i& box, const C3& c3, const unsigned char alpha);
    void drawVLine(const unsigned x, const unsigned minY, const unsigned maxY,
                   const C3& c3, const unsigned char alpha);
    void drawHLine(const unsigned minX, const unsigned maxX, const unsigned y,
                   const C3& c3, const unsigned char alpha);
    void drawLine(const unsigned sx, const unsigned sy,
                  const unsigned ex, const unsigned ey,
                  const float width,
                  const bool drawStartPoint, const float radiusStartPoint, 
                  const bool drawEndPoint, const float radiusEndPoint,
                  const C3& c3, const unsigned char alpha);
    void drawCircle(const unsigned sx, const unsigned sy,
                    const float radius,
                    const float width,
                    const C3& c3, const unsigned char alpha);

    // available after first call of drawStr. (only works properly monospace font)
    unsigned getFontStepX() const { return mFontStepX; }

    size_t getDrawStrItemTotal() const { return mDrawStrArray.size(); }
    BBox2i calcDrawBbox(const size_t startStrItemId, const size_t endStrItemId) const;
    void moveStr(const size_t strItemId, const int deltaX, const int deltaY);

    void finalizeRgb888(std::vector<unsigned char>& rgbFrame,
                        const unsigned frameWidth,
                        const unsigned frameHeight,
                        const bool top2bottomFlag,
                        const Align hAlign,
                        const Align vAlign,
                        std::vector<unsigned char>* bgArchive,
                        const bool doParallel) const;

    bool savePPM(const std::string& filename) const; // for debugging purposes

    Parser& getParser() { return mParser; }

    static size_t msgDisplayLen(const std::string& msg); // skip escape sequence
    static size_t msgDisplayWidth(const std::string& msg); // skip escape sequence and return max width

#ifdef SC_TIMELOG
    ScTimeLog& getScTimeLog() { return mScanConvertTimeLog;}
#endif // end of SC_TIMELOG

private:
    using OverlayBoxItemShPtr = std::shared_ptr<OverlayBoxItem>;
    using OverlayBoxOutlineItemShPtr = std::shared_ptr<OverlayBoxOutlineItem>;
    using OverlayStrItemShPtr = std::shared_ptr<OverlayStrItem>;
    using OverlayVLineItemShPtr = std::shared_ptr<OverlayVLineItem>;
    using OverlayHLineItemShPtr = std::shared_ptr<OverlayHLineItem>;
    using OverlayLineItemShPtr = std::shared_ptr<OverlayLineItem>;
    using OverlayCircleItemShPtr = std::shared_ptr<OverlayCircleItem>;
    using Vec2i = scene_rdl2::math::Vec2i;

    void setLineDrawFunc(const LineDrawType type);
    void lineDrawBresenham(const Vec2f& s, const Vec2f& e, const float w, const C3& c, const unsigned char a);
    void lineDrawDDA(const Vec2f& s, const Vec2f& e, const float w, const C3& c, const unsigned char a);
    void lineDrawWu(const Vec2f& s, const Vec2f& e, const float w, const C3& c3, const unsigned char a);
    void lineDrawWuBody(const Vec2f& s, const Vec2f& e, const float halfW, const C3& c3, const unsigned char a);
    void lineDrawWuHQ(const Vec2f& s, const Vec2f& e, const float w, const C3& c3, const unsigned char a);
    void lineDrawWuHQBody(const Vec2f& s, const Vec2f& e, const float halfW, const C3& c3, const unsigned char a);
    void lineDrawWuCap(const Vec2f& p, const float halfW, const C3& c3, const unsigned char alpha);
    void lineDrawOutlineStroke(const Vec2f& s, const Vec2f& e, const float w, const C3& c3, const unsigned char a)
    {
        lineDrawOutlineStrokeMain(s, e, w, 0, c3, a);
    }
    void lineDrawOutlineStroke2(const Vec2f& s, const Vec2f& e, const float w, const C3& c3, const unsigned char a)
    {
        lineDrawOutlineStrokeMain(s, e, w, 1, c3, a);
    }
    void lineDrawOutlineStrokeMain(const Vec2f& s, const Vec2f& e, const float w,
                                   const int mode, // 0:lambda-version 1:class-inheritance-version
                                   const C3& c3, const unsigned char a);
    void lineDrawHybrid(const Vec2f& s, const Vec2f& e, const float w, const C3& c3, const unsigned char a)
    {
        //
        // This approach switches between WU and outline-stroke based on the line width: thin lines are drawn
        // with fast WU, while only thicker lines that WU cannot render cleanly are drawn with outline-stroke.
        // However, there is a difference in how the two algorithms interpret width, so their visual
        // thicknesses don't perfectly match around width = 1.0. Also, since WU is an integer-based algorithm,
        // it doesn't produce the expected results for widths below 1.0. While this implementation does have
        // these limitations, it's still practical enough to be useful.
        //
        if (w <= 1.0f) lineDrawWu(s, e, w, c3, a);
        else lineDrawOutlineStroke(s, e, w, c3, a);
    }

    void scanConvert(const unsigned xSubSamples, const unsigned ySubSamples,
                     const C3& c3, const unsigned char alpha,
                     const std::function<bool(unsigned& min, unsigned& max)>& calcMinMaxY,
                     const std::function<bool(const unsigned y, const unsigned ySubPixReso,
                                              std::vector<float> iTbl[],
                                              unsigned& sPix, unsigned& ePix)>& xScan);

    inline unsigned getPixOffset(const unsigned x, const unsigned y) const;
    inline unsigned getPixDataOffset(const unsigned x, const unsigned y) const;
    inline unsigned char* getPixDataAddr(const unsigned x, const unsigned y);
    inline const unsigned char* getPixDataAddr(const unsigned x, const unsigned y) const;
    inline bool isPixInside(const unsigned x, const unsigned y) const;

    inline void alphaBlendPixC4(const C3& fgC3, const unsigned char fgAlpha, unsigned char* bgPixC4) const;
    inline void alphaBlendPixC3(const C3& fgC3, const unsigned char fgAlpha, unsigned char* bgPixC3) const;
    inline C3 blendCol3(const C3& fgC3, const float fgFraction, unsigned char* bgPix) const;
    inline unsigned char clampCol0255(const float v0255) const;
    inline void setCol4(const C3& inC3, const unsigned char inAlpha, unsigned char* outPix) const;
    inline void setCol3(const C3& inC3, unsigned char* outPix) const;
    inline void maxAddCol4(const C3& inC3, const unsigned char inAlpha, unsigned char* outPix) const;
    inline void maxAddCol3(const C3& inC3, unsigned char* outPix) const;

    void overlayDrawFontCache(OverlayCharItemShPtr charItem);

    void drawStrClear();
    void drawStrFlush(const bool doParallel);
    void drawBoxClear();
    void drawBoxFlush(const bool doParallel);
    void drawBoxOutlineClear();
    void drawBoxOutlineFlush(const bool doParallel);
    void drawVLineClear();
    void drawVLineFlush(const bool doParallel);
    void drawHLineClear();
    void drawHLineFlush(const bool doParallel);
    void drawLineClear();
    void drawLineFlush(const bool doParallel);
    void drawCircleClear();
    void drawCircleFlush(const bool doParallel);

    inline void resizeRgb888(std::vector<unsigned char>& rgbFrame, const unsigned width, const unsigned height) const;
    inline void clearRgb888(std::vector<unsigned char>& rgbFrame) const;
    void copyRgb888(const std::vector<unsigned char>& in,
                    std::vector<unsigned char>& out,
                    const bool doParallel) const;

    void bakeOverlayMainRgb888(const std::vector<unsigned char>& fgFrameRGBA,
                               const unsigned fgWidth,
                               const unsigned fgHeight,
                               const Align hAlign,
                               const Align vAlign,
                               std::vector<unsigned char>& bgFrameRGB,
                               const unsigned bgWidth,
                               const unsigned bgHeight,
                               const bool top2bottomFlag,
                               const bool doParallel) const;

    std::string showPixFrameRGBA(const std::vector<unsigned char>& frameRGBA,
                                 const unsigned frameWidth,
                                 const unsigned frameHeight,
                                 const unsigned pixX,
                                 const unsigned pixY) const; // for debug

    void parserConfigure();
    std::string showMemPoolSize() const;
    std::string showLineDrawType() const;

    static std::string lineDrawStr(const LineDrawType type);

    //------------------------------

    OverlayDrawItemMemManager<OverlayStrItem> mStrItemMemMgr;
    OverlayDrawItemMemManager<OverlayCharItem> mCharItemMemMgr;
    OverlayDrawItemMemManager<OverlayBoxItem> mBoxItemMemMgr;
    OverlayDrawItemMemManager<OverlayBoxOutlineItem> mBoxOutlineItemMemMgr;
    OverlayDrawItemMemManager<OverlayVLineItem> mVLineItemMemMgr;
    OverlayDrawItemMemManager<OverlayHLineItem> mHLineItemMemMgr;
    OverlayDrawItemMemManager<OverlayLineItem> mLineItemMemMgr;
    OverlayDrawItemMemManager<OverlayCircleItem> mCircleItemMemMgr;

    std::vector<OverlayStrItemShPtr> mDrawStrArray;
    std::vector<OverlayBoxItemShPtr> mDrawBoxArray;
    std::vector<OverlayBoxItemShPtr> mDrawBoxBarArray;
    std::vector<OverlayBoxOutlineItemShPtr> mDrawBoxOutlineArray;
    std::vector<OverlayVLineItemShPtr> mDrawVLineArray;
    std::vector<OverlayHLineItemShPtr> mDrawHLineArray;
    std::vector<OverlayLineItemShPtr> mDrawLineArray;
    std::vector<OverlayCircleItemShPtr> mDrawCircleArray;

    unsigned mFontStepX {0};

    unsigned mWidth {0};
    unsigned mHeight {0};
    std::vector<unsigned char> mPixelsRGBA;

    //------------------------------

    using LineDrawFunc = void (Overlay::*)(const Vec2f& s, const Vec2f& e, const float width,
                                           const C3& c3, const unsigned char alpha);
    LineDrawFunc mLineDrawFunc {&Overlay::lineDrawHybrid};
    LineDrawType mLineDrawType { LineDrawType::HYBRID };

    // For the performance comparison purposes
    // This is a naive class inheritance type implementation of scan conversion.
    // Compared with the lambda version, the class inheritance version is always slow.
    // So we just use the lambda version. But keep the class inheritance version for
    // performance comparison purposes.
    std::unique_ptr<ScanConvertPolygon> mScanConvertPolygon;
#ifdef SC_TIMELOG
    ScTimeLog mScanConvertTimeLog;
#endif // end of SC_TIMELOG

    //------------------------------

    Parser mParser;
};

inline void
Overlay::resize(const unsigned width, const unsigned height)
{
    if (mWidth == width && mHeight == height) return; // no need to resize

    mWidth = width;
    mHeight = height;
    mPixelsRGBA.resize(mWidth * mHeight * 4);
}

inline void
Overlay::pixFill(const unsigned x, const unsigned y, const C3& c3, const unsigned char alpha)
{
    setCol4(c3, alpha, getPixDataAddr(x, y));
}

inline void
Overlay::pixMaxFill(const unsigned x, const unsigned y, const C3& c3, const unsigned char alpha)
{
    maxAddCol4(c3, alpha, getPixDataAddr(x, y));
}

//------------------------------

inline unsigned
Overlay::getPixOffset(const unsigned x, const unsigned y) const
{
    return y * mWidth + x;
}

inline unsigned
Overlay::getPixDataOffset(const unsigned x, const unsigned y) const
{
    return getPixOffset(x, y) * 4;
}

inline unsigned char*
Overlay::getPixDataAddr(const unsigned x, const unsigned y)
{
    return &mPixelsRGBA[getPixDataOffset(x, y)];
}

inline const unsigned char*
Overlay::getPixDataAddr(const unsigned x, const unsigned y) const
{
    return &mPixelsRGBA[getPixDataOffset(x, y)];
}

inline bool
Overlay::isPixInside(const unsigned x, const unsigned y) const
{
    return (x < mWidth && y < mHeight);
}

//------------------------------

inline void
Overlay::alphaBlendPixC4(const C3& fgC3, const unsigned char fgAlpha, unsigned char* bgPixC4) const
//
// This is a full alpha blending (fg over bg) operation.
//    
{
    const float currFgA = static_cast<float>(fgAlpha) / 255.0f;
    const float currBgA = static_cast<float>(bgPixC4[3]) / 255.0f;
    const float currOutA = currFgA + currBgA * (1.0f - currFgA);
    if (currOutA == 0.0f) {
        setCol4({0, 0, 0}, 0, bgPixC4);
    } else {
        auto calcC = [&](const unsigned char fgC, const float fgA,
                         const unsigned char bgC, const float bgA,
                         const float outA) {
            const float outC = (static_cast<float>(fgC) * fgA + static_cast<float>(bgC) * bgA * (1.0f - fgA)) / outA;
            return static_cast<unsigned char>(clampCol0255(outC));
        };

        setCol4({calcC(fgC3.mR, currFgA, bgPixC4[0], currBgA, currOutA),
                 calcC(fgC3.mG, currFgA, bgPixC4[1], currBgA, currOutA),
                 calcC(fgC3.mB, currFgA, bgPixC4[2], currBgA, currOutA)},
                static_cast<unsigned char>(clampCol0255(currOutA * 255.0f)),
                bgPixC4);
    }
}

inline void
Overlay::alphaBlendPixC3(const C3& fgC3, const unsigned char fgAlpha, unsigned char* bgPixC3) const
//
// We assume bg alpha is 1.0f
//    
{
    setCol3(blendCol3(fgC3, static_cast<float>(fgAlpha) / 255.0f, bgPixC3),
            bgPixC3);
}

inline C3
Overlay::blendCol3(const C3& fgC3, const float fgFraction, unsigned char* bgPix) const
{
    auto blendCol = [&](const unsigned char fg, const unsigned char bg) {
        return clampCol0255(static_cast<float>(fg) * fgFraction +
                            static_cast<float>(bg) * (1.0f - fgFraction));
    };
    return C3(blendCol(fgC3.mR, bgPix[0]),
              blendCol(fgC3.mG, bgPix[1]),
              blendCol(fgC3.mB, bgPix[2]));
}

inline unsigned char
Overlay::clampCol0255(const float v0255) const
{
    return static_cast<unsigned char>(scene_rdl2::math::clamp(v0255, 0.0f, 255.0f));
}

inline void
Overlay::setCol4(const C3& inC3, const unsigned char inAlpha, unsigned char* outPix) const
{
    setCol3(inC3, outPix);
    outPix[3] = inAlpha;
}

inline void
Overlay::setCol3(const C3& inC3, unsigned char* outPix) const
{
    outPix[0] = inC3.mR;
    outPix[1] = inC3.mG;
    outPix[2] = inC3.mB;
}

inline void
Overlay::maxAddCol4(const C3& inC3, const unsigned char inAlpha, unsigned char* outPix) const
{
    maxAddCol3(inC3, outPix);
    outPix[3] = std::max(outPix[3], inAlpha);
}

inline void
Overlay::maxAddCol3(const C3& inC3, unsigned char* outPix) const
{
    outPix[0] = std::max(outPix[0], inC3.mR);
    outPix[1] = std::max(outPix[1], inC3.mG);
    outPix[2] = std::max(outPix[2], inC3.mB);
}

//------------------------------

inline void
Overlay::resizeRgb888(std::vector<unsigned char>& rgbFrame, const unsigned width, const unsigned height) const
{
    const unsigned dataSize = width * height * 3;
    if (rgbFrame.size() != dataSize) {
        rgbFrame.resize(dataSize);
        clearRgb888(rgbFrame);
    }
}

inline void
Overlay::clearRgb888(std::vector<unsigned char>& rgbFrame) const
{
    for (size_t i = 0; i < rgbFrame.size(); ++i) rgbFrame[i] = 0x0;
}

} // namespace telemetry
} // namespace mcrt_dataio
