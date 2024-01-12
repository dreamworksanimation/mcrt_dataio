// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/common/grid_util/Parser.h>
#include <scene_rdl2/common/math/BBox.h>
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
    C3(unsigned char r, unsigned char g, unsigned char b) : mR(r), mG(g), mB(b) {}
    C3(const C3& c3) : mR(c3.mR), mG(c3.mG), mB(c3.mB) {}

    bool isBlack() const { return (mR == 0x0 && mG == 0x0 && mB == 0x0); }

    unsigned char mR {0};
    unsigned char mG {0};
    unsigned char mB {0};
};

//------------------------------------------------------------------------------------------

class FontCacheItem
//
// Font bit mask information cache data. We want to avoid multiple times same font bitmap conversion.
//
{
public:
    FontCacheItem(const char c,
                  const FT_Bitmap& bitmap, unsigned bitmapLeft, unsigned bitmapTop, unsigned advanceX);

    unsigned char get(int bx, int by) const { return mBuffer[by * mPitch + bx]; }

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

    Font(const std::string& fontTTFFileName, int fontSizePoint)
        : mFontTTFFileName(fontTTFFileName)
        , mFontSizePoint(fontSizePoint)
    {
        setupFontFace();
    }

    const std::string& getFontTTFFileName() const { return mFontTTFFileName; }

    int getFontSizePoint() const { return mFontSizePoint; }
    const FT_Face& getFace() const { return mFace; }

    FontCacheItemShPtr getFontCacheItem(char c);

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
             unsigned fontHeight,
             float bgYAdjustScale,
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

    // display framebuffer coord as well if you set non zero winHeight
    std::string show(unsigned winHeight = 0) const;

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

private:

    bool entryNewCharItem(Overlay& overlay,
                          Font& font,
                          const FT_Vector& fontPos,
                          char c,
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

    void set(const BBox2i& bbox, const C3& c, unsigned char alpha)
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

    void set(unsigned x, unsigned minY, unsigned maxY, const C3& c, unsigned char alpha)
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

class Overlay
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using BBox2i = scene_rdl2::math::BBox2i;
    using OverlayCharItemShPtr = std::shared_ptr<OverlayCharItem>;
    using Parser = scene_rdl2::grid_util::Parser;

    enum class Align {
        SMALL,  // left or bottom
        MIDDLE, // center
        BIG     // right or top
    };

    Overlay() { parserConfigure(); }
    Overlay(const unsigned width, const unsigned height)
    {
        resize(width, height);
        clear({0, 0, 0}, 0, true);

        parserConfigure();
    }

    inline void resize(const unsigned width, const unsigned height); // no clear internally just change size
    void clear(const C3& c3, const unsigned char alpha, bool doParallel);
    void boxFill(const BBox2i& bbox, const C3& c3, const unsigned char alpha, bool doParallel);
    void vLine(const unsigned x, const unsigned minY, const unsigned maxY,
               const C3& c3, const unsigned char alpha); // no parallel option
    void fill(unsigned char r, unsigned char g, unsigned char b, unsigned char a); // for debug

    unsigned getWidth() const { return mWidth; }
    unsigned getHeight() const { return mHeight; }

    unsigned getMaxYLines(const Font& font, unsigned& offsetBottomPixY, unsigned& stepPixY) const;

    OverlayCharItemShPtr getNewOverlayCharItem() { return getMemOverlayCharItem(); }
    void restoreOverlayCharItemMem(OverlayCharItemShPtr ptr) { setMemOverlayCharItem(ptr); }

    void drawStrClear();
    bool drawStr(Font& font,
                 const unsigned startX,
                 const unsigned startY,
                 const std::string& str,
                 const C3& c3,
                 std::string& error);
    void drawStrFlush(bool doParallel);

    void drawBoxClear();
    void drawBox(const BBox2i box, const C3& c3, const unsigned char alpha); // push into mDrawBoxArray
    void drawBoxBar(const BBox2i box, const C3& c3, const unsigned char alpha); // push into mDrawBoxBarArray
    void drawBoxFlush(bool doParallel);

    void drawVLineClear();
    void drawVLine(const unsigned x, const unsigned minY, const unsigned maxY,
                   const C3& c3, const unsigned char alpha);
    void drawVLineFlush(bool doParallel);

    // available after first call of drawStr. (only works properly monospace font)
    unsigned getFontStepX() const { return mFontStepX; }

    size_t getDrawStrItemTotal() const { return mDrawStrArray.size(); }
    BBox2i calcDrawBbox(const size_t startStrItemId, const size_t endStrItemId) const;

    void finalizeRgb888(std::vector<unsigned char>& rgbFrame,
                        unsigned frameWidth,
                        unsigned frameHeight,
                        bool top2bottomFlag,
                        Align hAlign,
                        Align vAlign,
                        std::vector<unsigned char>* bgArchive,
                        bool doParallel) const;

    bool savePPM(const std::string& filename) const; // for debugging purposes

    Parser& getParser() { return mParser; }

    static size_t msgDisplayLen(const std::string& msg); // skip escape sequence
    static size_t msgDisplayWidth(const std::string& msg); // skip escape sequence and return max width

private:
    using OverlayBoxItemShPtr = std::shared_ptr<OverlayBoxItem>;
    using OverlayStrItemShPtr = std::shared_ptr<OverlayStrItem>;
    using OverlayVLineItemShPtr = std::shared_ptr<OverlayVLineItem>;

    inline unsigned getPixOffset(const unsigned x, const unsigned y) const;
    inline unsigned getPixDataOffset(const unsigned x, const unsigned y) const;
    inline unsigned char* getPixDataAddr(const unsigned x, const unsigned y);
    inline const unsigned char* getPixDataAddr(const unsigned x, const unsigned y) const;
    inline bool isPixInside(const unsigned x, const unsigned y) const;

    inline void alphaBlendPixC4(const C3& fgC3, const unsigned char fgAlpha, unsigned char* bgPixC4) const;
    inline void alphaBlendPixC3(const C3& fgC3, const unsigned char fgAlpha, unsigned char* bgPixC3) const;
    inline C3 blendCol3(const C3& fgC3, float fgFraction, unsigned char* bgPix) const;
    inline unsigned char clampCol0255(const float v0255) const;
    inline void setCol4(const C3& inC3, unsigned char inAlpha, unsigned char* outPix) const;
    inline void setCol3(const C3& inC3, unsigned char* outPix) const;

    void overlayDrawFontCache(OverlayCharItemShPtr charItem);

    inline void resizeRgb888(std::vector<unsigned char>& rgbFrame, unsigned width, unsigned height) const;
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

    OverlayStrItemShPtr getMemOverlayStrItem();
    void setMemOverlayStrItem(OverlayStrItemShPtr ptr);
    OverlayCharItemShPtr getMemOverlayCharItem();
    void setMemOverlayCharItem(OverlayCharItemShPtr ptr);
    OverlayBoxItemShPtr getMemOverlayBoxItem();
    void setMemOverlayBoxItem(OverlayBoxItemShPtr ptr);
    OverlayVLineItemShPtr getMemOverlayVLineItem();
    void setMemOverlayVLineItem(OverlayVLineItemShPtr ptr);

    void parserConfigure();
    std::string showMemPoolSize() const;

    //------------------------------

    std::vector<OverlayStrItemShPtr> mDrawStrArray;
    std::vector<OverlayBoxItemShPtr> mDrawBoxArray;
    std::vector<OverlayBoxItemShPtr> mDrawBoxBarArray;
    std::vector<OverlayVLineItemShPtr> mDrawVLineArray;

    unsigned mMaxOverlayStrItemMemPool {0};
    unsigned mMaxOverlayCharItemMemPool {0};
    unsigned mMaxOverlayBoxItemMemPool {0};
    unsigned mMaxOverlayVLineItemMemPool {0};
    std::deque<OverlayStrItemShPtr> mOverlayStrItemMemPool;
    std::deque<OverlayCharItemShPtr> mOverlayCharItemMemPool;
    std::deque<OverlayBoxItemShPtr> mOverlayBoxItemMemPool;
    std::deque<OverlayVLineItemShPtr> mOverlayVLineItemMemPool;

    unsigned mFontStepX {0};

    unsigned mWidth {0};
    unsigned mHeight {0};
    std::vector<unsigned char> mPixelsRGBA;

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
    float currFgA = static_cast<float>(fgAlpha) / 255.0f;
    float currBgA = static_cast<float>(bgPixC4[3]) / 255.0f;
    float currOutA = currFgA + currBgA * (1.0f - currFgA);
    if (currOutA == 0.0f) {
        setCol4({0, 0, 0}, 0, bgPixC4);
    } else {
        auto calcC = [&](const unsigned char fgC, const float fgA,
                         const unsigned char bgC, const float bgA,
                         const float outA) {
            float outC = (static_cast<float>(fgC) * fgA + static_cast<float>(bgC) * bgA * (1.0f - fgA)) / outA;
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
Overlay::blendCol3(const C3& fgC3, float fgFraction, unsigned char* bgPix) const
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
Overlay::setCol4(const C3& inC3, unsigned char inAlpha, unsigned char* outPix) const
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

//------------------------------

inline void
Overlay::resizeRgb888(std::vector<unsigned char>& rgbFrame, unsigned width, unsigned height) const
{
    unsigned dataSize = width * height * 3;
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
