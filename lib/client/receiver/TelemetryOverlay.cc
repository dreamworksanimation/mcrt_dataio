// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryOverlay.h"

#include <scene_rdl2/common/except/exceptions.h>

#include <tbb/parallel_for.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// This directive set memory pool logic ON for telemetry overlay draw items and this should be
// enabled for the release version. Disabling this directive is only used for performance
// comparison purposes.
#define ENABLE_MEMPOOL

FT_Pos min(const FT_Pos a, const FT_Pos b) { return a < b ? a : b; }
FT_Pos max(const FT_Pos a, const FT_Pos b) { return a < b ? b : a; }

FT_Vector
min(const FT_Vector& a, const FT_Vector& b)
{
    FT_Vector v;
    v.x = min(a.x, b.x);
    v.y = min(a.y, b.y);
    return v;
}

FT_Vector
max(const FT_Vector& a, const FT_Vector& b)
{
    FT_Vector v;
    v.x = max(a.x, b.x);
    v.y = max(a.y, b.y);
    return v;
}

static constexpr float lineSpacingScale = 1.1f;

namespace mcrt_dataio {
namespace telemetry {

FreeTypeBBox
emptyFreeTypeBBox()
{
    return FreeTypeBBox {{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()},
                         {std::numeric_limits<int>::min(), std::numeric_limits<int>::min()}};
}

//------------------------------------------------------------------------------------------

FontCacheItem::FontCacheItem(const char c,
                             const FT_Bitmap& bitmap,
                             unsigned bitmapLeft,
                             unsigned bitmapTop,
                             unsigned advanceX)
    : mC(c)
    , mRows(bitmap.rows)
    , mWidth(bitmap.width)
    , mPitch(bitmap.pitch)
    , mBitmapLeft(bitmapLeft)
    , mBitmapTop(bitmapTop)
    , mAdvanceX(advanceX)
{
    mBuffer.resize(mRows * mWidth);
    int id = 0;
    for (int by = 0; by < bitmap.rows; ++by) {
        for (int bx = 0; bx < bitmap.width; ++bx) {
            mBuffer[id++] = bitmap.buffer[by * bitmap.pitch + bx];
        }
    }
}

//------------------------------------------------------------------------------------------

Font::FontCacheItemShPtr
Font::getFontCacheItem(char c)
{
    unsigned glyphIndex = FT_Get_Char_Index(mFace, c);
    auto itr = mFontCacheMap.find(glyphIndex);
    if (itr != mFontCacheMap.end()) {
        // found
        return itr->second;
    } else {
        // not found
        if (FT_Load_Glyph(mFace, glyphIndex, FT_LOAD_DEFAULT) != 0) {
            return nullptr;
        }
        if (FT_Render_Glyph(mFace->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            return nullptr;
        }

        FontCacheItemShPtr shPtr = std::make_shared<FontCacheItem>(c,
                                                                   mFace->glyph->bitmap,
                                                                   mFace->glyph->bitmap_left,
                                                                   mFace->glyph->bitmap_top,
                                                                   mFace->glyph->advance.x);

        mFontCacheMap[glyphIndex] = shPtr;
        return shPtr;
    }
}

void
Font::setupFontFace()
{
    if (FT_Init_FreeType(&mFtLibrary) != 0) {
        throw scene_rdl2::except::RuntimeError("FT_Init_FreeType() failed");
    }

    if (FT_New_Face(mFtLibrary, mFontTTFFileName.c_str(), 0, &mFace) != 0) {
        std::ostringstream ostr;
        ostr << "Construct new face failed. font:" << mFontTTFFileName;
        throw scene_rdl2::except::RuntimeError(ostr.str());
    }

    if (FT_Set_Char_Size(mFace,
                         mFontSizePoint * 64, 0,
                         72, 0) != 0) { // horizontal and vertical resolution = 72 dpi
        std::ostringstream ostr;
        ostr << "Set font size failed." << " fontSizePoint:" << mFontSizePoint << " font:" << mFontTTFFileName;
        throw scene_rdl2::except::RuntimeError(ostr.str());
    }

    // This scale is finally used to adjust font bg fill window y direction.
    // This value is a font-dependent value.
    // See Overlay::overlayDrawFontCache() for more detail.
    mBgYAdjustScale = 0.15f;
}

//------------------------------------------------------------------------------------------

FreeTypeBBox
OverlayCharItem::getBBox() const
{
    // Adjust y position based on the bgYAdjustScale value if we have.
    FT_Pos offsetY = mFontSize.y * mBgYAdjustScale;
    return FreeTypeBBox({mFontBasePos.x,               mFontBasePos.y - mFontSize.y + offsetY},
                        {mFontBasePos.x + mFontSize.x, mFontBasePos.y + offsetY              });
}

std::string
OverlayCharItem::show(unsigned winHeight) const
{
    auto showFTVec = [](const FT_Vector& v, unsigned wHeight) {
        std::ostringstream ostr;
        ostr << "x:" << v.x << " (ix:" << Font::ftPosToi(v.x) << ") ";

        int iy = Font::ftPosToi(v.y);
        if (wHeight == 0) {
            ostr << "y:" << v.y << " (iy:" << iy << ")";
        } else {
            int flipIy = wHeight - 1 - iy;
            ostr << "y:" << v.y << " (iy:" << iy << " flipY:" << flipIy << ")";
        }
        return ostr.str();
    };
    auto showC3 = [](const C3& c) {
        std::ostringstream ostr;
        ostr
        <<  "r:" << std::setw(3) << static_cast<int>(c.mR)
        << " g:" << std::setw(3) << static_cast<int>(c.mG)
        << " b:" << std::setw(3) << static_cast<int>(c.mB);
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "OverlayCharItem {\n"
         << "  mFontBasePos: " << showFTVec(mFontBasePos, winHeight) << '\n'
         << "     mFontSize: " << showFTVec(mFontSize, 0) << '\n'
         << "  mFontDataPos: " << showFTVec(mFontDataPos, winHeight)
         << " getWidth():" << getWidth() << " getHeight():" << getHeight() << '\n'
         << "         mFgC3: " << showC3(mFgC3) << '\n'
         << "         mBgC3: " << showC3(mBgC3) << '\n'
         << "}";
    return ostr.str();
}

//------------------------------------------------------------------------------------------

void
OverlayStrItem::resetCharItemArray(Overlay& overlay)
{
    for (auto itr : mCharItemArray) {
        overlay.restoreOverlayCharItemMem(itr);
    }
    mCharItemArray.clear();    
}

void
OverlayStrItem::set(Overlay& overlay,
                    Font& font,
                    const unsigned startX,
                    const unsigned startY,
                    const unsigned overlayHeight,
                    const std::string& str,
                    const C3& c3)
{
    mStr = str;
    mStartX = startX;
    mStartY = startY;
    mOverlayHeight = overlayHeight;

    C3 fgC3 {c3};
    C3 bgC3 {0, 0, 0};

    const char *ptr = &str[0];
    auto get0255col = [&](unsigned char& col) {
        char colBuff[4] = {0x0, 0x0, 0x0, 0x0};
        for (int i = 0; i < 3; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(*ptr))) {
                if (i == 0) return false;
                break;
            }
            colBuff[i] = *(ptr++);
        }
        col = static_cast<unsigned char>(atoi(colBuff));
        return true;
    };
    auto processEscapeSequence = [&]() {
        // \e[38;2;R;G;Bm : define fg color escape sequence
        // \e[48;2;R;G;Bm : define bg color escape sequence
        if (*(++ptr) != '[') return;
        ptr++;
        if (*ptr != '3' && *ptr != '4') return;
        bool fgMode = (*ptr == '3') ? true : false;
        if (*(++ptr) != '8') return;
        if (*(++ptr) != ';') return;
        if (*(++ptr) != '2') return;
        if (*(++ptr) != ';') return;

        ptr++;
        C3 c;
        if (!get0255col(c.mR)) return;
        if (*ptr != ';') return;
        ptr++;
        if (!get0255col(c.mG)) return;
        if (*ptr != ';') return;
        ptr++;
        if (!get0255col(c.mB)) return;
        if (*ptr != 'm') return;
        ptr++;

        if (fgMode) fgC3 = c;
        else        bgC3 = c;
    };
    auto getChar = [&](char& outC) {
        if (*ptr == 0x0) return false;
        if (*ptr == '\e') {
            while (true) {
                processEscapeSequence();
                if (*ptr == 0x0) return false;
                if (*ptr != '\e') break;
            }
        }
        outC = *(ptr++);
        return true;
    };
    auto flipY = [&](unsigned fbY) {
        return mOverlayHeight - 1 - fbY;
    };

    // fontPos is FreeType coordinate. (Left, Top) is origin and
    // Right direction is +X, Bottom direction is +Y
    FT_Vector fontPos;
    fontPos.x = Font::iToftPos(mStartX);
    fontPos.y = Font::iToftPos(flipY(mStartY));
    
    while (true) {
        char c;
        if (!getChar(c)) break;
        
        if (c == '\n') {
            fontPos.x = Font::iToftPos(mStartX);
            fontPos.y += Font::iToftPos(font.getFontSizePoint() * lineSpacingScale);
            continue;
        }

        if (!entryNewCharItem(overlay, font, fontPos, c, fgC3, bgC3)) {
            throw scene_rdl2::except::RuntimeError("entryNewCharItem() failed. FreeType related error");            
        }

        fontPos.x += mCharItemArray.back()->getAdvanceX();
    }
}

void
OverlayStrItem::setupAllCharItems(std::vector<OverlayCharItemShPtr>& outCacheItemArray)
{
    for (size_t i = 0; i < mCharItemArray.size(); ++i) {
        if (mCharItemArray[i]) {
            outCacheItemArray.push_back(mCharItemArray[i]);
        }
    }
}

unsigned    
OverlayStrItem::getFirstCharStepX() const
{
    if (mCharItemArray.empty()) return 0;
    return mCharItemArray[0]->getStepX();
}

FreeTypeBBox
OverlayStrItem::getBBox() const
{
    FreeTypeBBox bbox = emptyFreeTypeBBox();
    for (size_t i = 0; i < mCharItemArray.size(); ++i) {
        if (mCharItemArray[i]) bbox.extend(mCharItemArray[i]->getBBox());
    }
    return bbox;
}

bool
OverlayStrItem::entryNewCharItem(Overlay& overlay,
                                 Font& font,
                                 const FT_Vector& fontPos,
                                 char c,
                                 const C3& fgC3,
                                 const C3& bgC3)
{
    std::shared_ptr<FontCacheItem> fontCacheItem = font.getFontCacheItem(c);
    if (!fontCacheItem) return false;

    OverlayCharItemShPtr overlayCharItem = overlay.getNewOverlayCharItem();
    overlayCharItem->set(fontCacheItem,
                         fontPos, // FreeType coordinate pos
                         font.getFontSizePoint(),
                         font.getBgYAdjustScale(),
                         fgC3,
                         bgC3);
    mCharItemArray.push_back(overlayCharItem);

    return true;
}

//------------------------------------------------------------------------------------------

void    
Overlay::clear(const C3& c3, const unsigned char alpha, bool doParallel)
{
    if (!doParallel) {
        for (size_t i = 0; i < mPixelsRGBA.size(); i += 4) {
            mPixelsRGBA[i  ] = c3.mR;
            mPixelsRGBA[i+1] = c3.mG;
            mPixelsRGBA[i+2] = c3.mB;
            mPixelsRGBA[i+3] = alpha;
        }
    } else {
        tbb::blocked_range<size_t> range(0, mPixelsRGBA.size() / 4, 128);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                for (size_t i = range.begin(); i < range.end(); ++i) {
                    size_t id = i * 4;
                    mPixelsRGBA[id  ] = c3.mR;
                    mPixelsRGBA[id+1] = c3.mG;
                    mPixelsRGBA[id+2] = c3.mB;
                    mPixelsRGBA[id+3] = alpha;
                }
            });
    }
}

void
Overlay::boxFill(const BBox2i& bbox, const C3& c3, const unsigned char alpha, bool doParallel)
{
    if (!doParallel) {
        for (unsigned y = bbox.lower.y; y <= bbox.upper.y; ++y) {
            unsigned char *ptr = getPixDataAddr(bbox.lower.x, y);
            for (unsigned x = bbox.lower.x; x <= bbox.upper.x; ++x) {
                setCol4(c3, alpha, ptr);
                ptr += 4;
            }
        }
    } else {
        tbb::blocked_range<size_t> range(bbox.lower.y, bbox.upper.y, 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                for (size_t y = range.begin(); y < range.end(); ++y) {
                    unsigned char *ptr = getPixDataAddr(bbox.lower.x, y);
                    for (unsigned x = bbox.lower.x; x <= bbox.upper.x; ++x) {
                        setCol4(c3, alpha, ptr);
                        ptr += 4;
                    }
                }
            });
    }
}

void
Overlay::vLine(const unsigned x, const unsigned minY, const unsigned maxY,
               const C3& c3, const unsigned char alpha)
{
    unsigned char *ptr = getPixDataAddr(x, minY);
    int offset = mWidth * 4;
    for (unsigned y = minY; y <= maxY; ++y) {
        setCol4(c3, alpha, ptr);
        ptr += offset;
    }
}

void
Overlay::fill(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    for (size_t i = 0; i < mPixelsRGBA.size() / 4; ++i) {
        setCol4(C3(r, g, b), a, &mPixelsRGBA[i * 4]);
    }
}

unsigned
Overlay::getMaxYLines(const Font& font, unsigned& offsetBottomPixY, unsigned& stepPixY) const
{
    stepPixY = static_cast<unsigned>(font.getFontSizePoint() * lineSpacingScale);
    unsigned maxYLines =  mHeight / stepPixY;
    unsigned spaceY = mHeight - maxYLines * stepPixY;
    offsetBottomPixY = spaceY / 2.0f;
    return maxYLines;
}

void
Overlay::drawStrClear()
{
    for (auto itr : mDrawStrArray) {
        setMemOverlayStrItem(itr);
    }

    if (mOverlayStrItemMemPool.size() > mMaxOverlayStrItemMemPool) {
        mMaxOverlayStrItemMemPool = mOverlayStrItemMemPool.size();
    }
    if (mOverlayCharItemMemPool.size() > mMaxOverlayCharItemMemPool) {
        mMaxOverlayCharItemMemPool = mOverlayCharItemMemPool.size();
    }

    mDrawStrArray.clear();
}

bool
Overlay::drawStr(Font& font,
                 const unsigned startX, const unsigned startY,
                 const std::string& str,
                 const C3& c3,
                 std::string& error)
{
    try {
        OverlayStrItemShPtr item = getMemOverlayStrItem();
        item->set(*this, font, startX, startY, mHeight, str, c3);
        mDrawStrArray.push_back(item);
        mFontStepX = item->getFirstCharStepX();
    }
    catch (scene_rdl2::except::RuntimeError& e) {
        std::ostringstream ostr;
        ostr << "ERROR : construct new OverlayStrItem failed."
             << " RuntimeError:" << e.what();
        error = ostr.str();
        return false;
    }

    return true;
}

void
Overlay::drawStrFlush(bool doParallel)
{
    std::vector<OverlayCharItemShPtr> charItemArray;
    for (size_t i = 0; i < mDrawStrArray.size(); ++i) {
        mDrawStrArray[i]->setupAllCharItems(charItemArray);
    }

    if (!doParallel) {
        for (size_t cId = 0; cId < charItemArray.size(); ++cId) {
            overlayDrawFontCache(charItemArray[cId]);
        }
    } else {
        tbb::blocked_range<size_t> range(0, charItemArray.size(), 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                for (size_t cId = range.begin(); cId < range.end(); ++cId) {
                    overlayDrawFontCache(charItemArray[cId]);
                }
            });
    }
}

void
Overlay::drawBoxClear()
{
    for (auto itr : mDrawBoxArray) {
        setMemOverlayBoxItem(itr);
    }
    for (auto itr : mDrawBoxBarArray) {
        setMemOverlayBoxItem(itr);
    }

    if (mOverlayBoxItemMemPool.size() > mMaxOverlayBoxItemMemPool) {
        mMaxOverlayBoxItemMemPool = mOverlayBoxItemMemPool.size();
    }

    mDrawBoxArray.clear();
    mDrawBoxBarArray.clear();
}

void    
Overlay::drawBox(const BBox2i box,
                 const C3& c3,
                 const unsigned char alpha)
{
    OverlayBoxItemShPtr item = getMemOverlayBoxItem();
    item->set(box, c3, alpha);
    mDrawBoxArray.push_back(item);
}

void    
Overlay::drawBoxBar(const BBox2i box,
                    const C3& c3,
                    const unsigned char alpha)
{
    OverlayBoxItemShPtr item = getMemOverlayBoxItem();
    item->set(box, c3, alpha);
    mDrawBoxBarArray.push_back(item);
}

void    
Overlay::drawBoxFlush(bool doParallel)
{
    auto drawBoxArray = [&](std::vector<OverlayBoxItemShPtr>& boxArray) {
        if (!doParallel) {
            for (size_t id = 0; id < boxArray.size(); ++id) {
                boxFill(boxArray[id]->getBBox(),
                        boxArray[id]->getC(),
                        boxArray[id]->getAlpha(),
                        false);
            }
        } else {
            tbb::blocked_range<size_t> range(0, boxArray.size(), 1);
            tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                    for (size_t id = range.begin(); id < range.end(); ++id) {
                        boxFill(boxArray[id]->getBBox(),
                                boxArray[id]->getC(),
                                boxArray[id]->getAlpha(),
                                true);
                    }
                });
        }
    };

    drawBoxArray(mDrawBoxArray);
    drawBoxArray(mDrawBoxBarArray);
}

void
Overlay::drawVLineClear()
{
    for (auto itr : mDrawVLineArray) {
        setMemOverlayVLineItem(itr);
    }

    if (mOverlayVLineItemMemPool.size() > mMaxOverlayVLineItemMemPool) {
        mMaxOverlayVLineItemMemPool = mOverlayVLineItemMemPool.size();
    }

    mDrawVLineArray.clear();
}

void
Overlay::drawVLine(const unsigned x,
                   const unsigned minY,
                   const unsigned maxY,
                   const C3& c3,
                   const unsigned char alpha)
{
    OverlayVLineItemShPtr item = getMemOverlayVLineItem();
    item->set(x, minY, maxY, c3, alpha);
    mDrawVLineArray.push_back(item);
}

void
Overlay::drawVLineFlush(bool doParallel)
{
    if (!doParallel) {
        for (size_t id = 0; id < mDrawVLineArray.size(); ++id) {
            vLine(mDrawVLineArray[id]->getX(),
                  mDrawVLineArray[id]->getMinY(),
                  mDrawVLineArray[id]->getMaxY(),
                  mDrawVLineArray[id]->getC(),
                  mDrawVLineArray[id]->getAlpha());
        }
    } else {
        tbb::blocked_range<size_t> range(0, mDrawVLineArray.size(), 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                for (size_t id = range.begin(); id < range.end(); ++id) {
                    vLine(mDrawVLineArray[id]->getX(),
                          mDrawVLineArray[id]->getMinY(),
                          mDrawVLineArray[id]->getMaxY(),
                          mDrawVLineArray[id]->getC(),
                          mDrawVLineArray[id]->getAlpha());
                }
            });
    }
}

Overlay::BBox2i
Overlay::calcDrawBbox(const size_t startStrItemId, const size_t endStrItemId) const
{
    FreeTypeBBox bbox = emptyFreeTypeBBox();
    for (size_t i = startStrItemId; i <= endStrItemId; ++i) {
        bbox.extend(mDrawStrArray[i]->getBBox());
    }        

    auto flipY = [&](unsigned y) {
        return mHeight - 1 - y;
    };

    BBox2i bbox2i;
    bbox2i.lower.x = Font::ftPosToi(bbox.lower.x);
    bbox2i.lower.y = flipY(Font::ftPosToi(bbox.upper.y));
    bbox2i.upper.x = Font::ftPosToi(bbox.upper.x);
    bbox2i.upper.y = flipY(Font::ftPosToi(bbox.lower.y));
    return bbox2i;
}

void
Overlay::finalizeRgb888(std::vector<unsigned char>& rgbFrame,
                        unsigned frameWidth,
                        unsigned frameHeight,
                        bool top2bottomFlag,
                        Align hAlign,
                        Align vAlign,
                        std::vector<unsigned char>* bgArchive,
                        bool doParallel) const
{
    resizeRgb888(rgbFrame, frameWidth, frameHeight);
    if (bgArchive) {
        copyRgb888(rgbFrame, *bgArchive, doParallel);
    }
    bakeOverlayMainRgb888(mPixelsRGBA,
                          mWidth,
                          mHeight,
                          hAlign,
                          vAlign,
                          rgbFrame,
                          frameWidth,
                          frameHeight,
                          top2bottomFlag,
                          doParallel);
}

bool
Overlay::savePPM(const std::string& filename) const
//
// for debug
//
{
    std::ofstream ofs(filename);
    if (!ofs) return false;

    ofs << "P3\n" << mWidth << ' ' << mHeight << '\n' << 255 << '\n';
    for (int y = mHeight - 1; y >= 0; --y) {
        for (unsigned x = 0; x < mWidth; ++x) {
            const unsigned char* pix = getPixDataAddr(x, y);
            ofs << static_cast<int>(pix[0]) << ' '
                << static_cast<int>(pix[1]) << ' '
                << static_cast<int>(pix[2]) << ' ';
        }
    }

    return true;
}

// static function
size_t
Overlay::msgDisplayLen(const std::string& msg)
//
// Skip escape sequence data and returns the actual display message length
//    
{
    size_t total = 0;
    const char* ptr = &msg[0];
    while (1) {
        if (*ptr == 0x0) break;
        if (*ptr == '\e') {
            while (1) {
                ptr++;
                if (*ptr == 0x0) return total;
                if (*ptr == 'm') break; // end of escape sequence
            }
        } else {
            total++;
        }
        ptr++;
    }
    return total;
}

// static function
size_t
Overlay::msgDisplayWidth(const std::string& msg)
//
// skip escape sequence and find max width for multiple string lines
//
{
    std::stringstream sstr(msg);
    std::string currLine;
    size_t width = 0;
    while (getline(sstr, currLine)) {
        if (currLine.empty()) break;
        size_t currSize = msgDisplayLen(currLine);
        if (currSize > width) width = currSize;
    }
    return width;
}

//------------------------------------------------------------------------------------------

void
Overlay::overlayDrawFontCache(const OverlayCharItemShPtr charItem)
{
    auto flipY = [&](unsigned freeTypeY) {
        return mHeight - 1 - freeTypeY;
    };

    const C3& bgC3 = charItem->getBgC3();
    if (!bgC3.isBlack()) {

        unsigned offsetY =
            (charItem->getBgYAdjustScale() > 0.0f)
            ? charItem->getHeight() * charItem->getBgYAdjustScale()
            : 0;
        for (unsigned y = 0; y < charItem->getHeight(); ++y) {
            unsigned fbSpaceY = flipY(charItem->getBaseY() - y + offsetY);
            for (unsigned x = 0; x < charItem->getWidth(); ++x) {
                unsigned fbSpaceX = charItem->getBaseX() + x;
                setCol4(bgC3, 255, getPixDataAddr(fbSpaceX, fbSpaceY));                
            }
        }
    }

    const FontCacheItem* fontCacheItem = charItem->getFontCacheItem().get();
    if (fontCacheItem->isSpace()) return;

    unsigned freeTypeX = charItem->getPosX();
    unsigned freeTypeY = charItem->getPosY();
    const C3& fgC3 = charItem->getFgC3();
    for (unsigned by = 0; by < fontCacheItem->getRows(); ++by) {
        unsigned fbY = flipY(freeTypeY + by);
        for (unsigned bx = 0; bx < fontCacheItem->getWidth(); ++bx) {
            unsigned char alpha = fontCacheItem->get(bx, by);
            if (alpha) {
                unsigned fbX = freeTypeX + bx;
                if (isPixInside(fbX, fbY)) {
                    alphaBlendPixC4(fgC3, alpha, getPixDataAddr(fbX, fbY));
                }
            }
        }
    }
}

void
Overlay::copyRgb888(const std::vector<unsigned char>& in,
                    std::vector<unsigned char>& out,
                    const bool doParallel) const
{
    if (!doParallel) {
        out = in;
    } else {
        if (in.size() != out.size()) {
            out.resize(in.size());
        }
        constexpr size_t grainSize = 128;
        tbb::blocked_range<size_t> range(0, in.size(), grainSize);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                for (size_t i = range.begin(); i < range.end(); ++i) {
                    out[i] = in[i];
                }
            });
    }
}

void
Overlay::bakeOverlayMainRgb888(const std::vector<unsigned char>& fgFrameRGBA,
                               const unsigned fgWidth,
                               const unsigned fgHeight,
                               const Align hAlign,
                               const Align vAlign,
                               std::vector<unsigned char>& bgFrameRGB,
                               const unsigned bgWidth,
                               const unsigned bgHeight,
                               const bool top2bottomFlag,
                               const bool doParallel) const
{
    auto adjustSrcRange = [](const unsigned fgSize, const unsigned bgSize, Align align,
                             unsigned& fgMin, unsigned& fgMax) {
        if (fgSize <= bgSize) {
            fgMin = 0;
            fgMax = fgSize - 1;
        } else {
            switch (align) {
            case Align::SMALL  : fgMin = 0; fgMax = bgSize - 1; break;
            case Align::MIDDLE : fgMin = fgSize / 2 - bgSize / 2; fgMax = fgMin + bgSize - 1; break;
            case Align::BIG    : fgMax = fgSize - 1; fgMin = fgMax - bgSize + 1; break;
            }
        }
    };
    auto adjustDstStartPos = [](const unsigned fgSize, const unsigned bgSize, Align align, bool flip) {
        unsigned pos;
        if (fgSize < bgSize) {
            switch (align) {
            case Align::SMALL  : pos = 0; break;
            case Align::MIDDLE : pos = bgSize / 2 - fgSize / 2; break;
            case Align::BIG    : pos = bgSize - 1 - fgSize + 1; break;
            }
        } else {
            pos = 0;
        }
        if (flip) {
            pos = bgSize - 1 - pos;
        }
        return pos;
    };
    auto calcFgDataAddr = [&](unsigned x, unsigned y) { return &fgFrameRGBA[(y * fgWidth + x) * 4]; };
    auto calcBgDataAddr = [&](unsigned x, unsigned y) { return &bgFrameRGB[(y * bgWidth + x) * 3]; };

    unsigned fgXmin, fgXmax, fgYmin, fgYmax;
    adjustSrcRange(fgWidth, bgWidth, hAlign, fgXmin, fgXmax);
    adjustSrcRange(fgHeight, bgHeight, vAlign, fgYmin, fgYmax);

    if (!doParallel) {
        // single thread execution
        unsigned bgY = adjustDstStartPos(fgHeight, bgHeight, vAlign, top2bottomFlag);
        unsigned bgX = adjustDstStartPos(fgWidth, bgWidth, hAlign, false);

        for (unsigned fgY = fgYmin ; fgY <= fgYmax; ++fgY) {
            const unsigned char* fgPtrC4 = calcFgDataAddr(fgXmin, fgY);
            unsigned char* bgPtrC3 = calcBgDataAddr(bgX, bgY);
            for (unsigned fgX = fgXmin ; fgX <= fgXmax; ++fgX) {
                alphaBlendPixC3({fgPtrC4[0], fgPtrC4[1], fgPtrC4[2]}, fgPtrC4[3], bgPtrC3);
                fgPtrC4 += 4;
                bgPtrC3 += 3;
            }
            bgY += ((top2bottomFlag) ? -1 : 1);
        }
    } else {
        // multi-thread execution
        unsigned fgYsize = fgYmax - fgYmin + 1;
        constexpr size_t grainSize = 2;
        tbb::blocked_range<size_t> range(0, fgYsize, grainSize);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                unsigned bgY = adjustDstStartPos(fgHeight, bgHeight, vAlign, top2bottomFlag);
                bgY += ((top2bottomFlag) ? - range.begin() : range.begin());
                unsigned bgX = adjustDstStartPos(fgWidth, bgWidth, hAlign, false);
                for (size_t fgYoffset = range.begin(); fgYoffset < range.end(); ++fgYoffset) {
                    unsigned fgY = fgYmin + fgYoffset;

                    const unsigned char* fgPtrC4 = calcFgDataAddr(fgXmin, fgY);
                    unsigned char* bgPtrC3 = calcBgDataAddr(bgX, bgY);
                    for (unsigned fgX = fgXmin ; fgX <= fgXmax; ++fgX) {
                        alphaBlendPixC3({fgPtrC4[0], fgPtrC4[1], fgPtrC4[2]}, fgPtrC4[3], bgPtrC3);
                        fgPtrC4 += 4;
                        bgPtrC3 += 3;
                    }
                    bgY += ((top2bottomFlag) ? -1 : 1);
                }
            });
    }
}

std::string
Overlay::showPixFrameRGBA(const std::vector<unsigned char>& frameRGBA,
                          const unsigned frameWidth,
                          const unsigned frameHeight,
                          const unsigned pixX,
                          const unsigned pixY) const
{
    auto showPos = [&]() {
        std::ostringstream ostr;
        ostr << "(x:" << pixX << ',' << pixY << ")";
        return ostr.str();
    };
    auto showPixCol = [](const unsigned char *pix) {
        std::ostringstream ostr;
        ostr
        << "(r:" << static_cast<int>(pix[0])
        << ",g:" << static_cast<int>(pix[1])
        << ",b:" << static_cast<int>(pix[2])
        << ",a:" << static_cast<int>(pix[3]) << ')';
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "pix " << showPos() << ' ' << showPixCol(&frameRGBA[(pixY * frameWidth + pixX) * 4]);
    return ostr.str();
}

Overlay::OverlayStrItemShPtr
Overlay::getMemOverlayStrItem()
{
    if (mOverlayStrItemMemPool.empty()) {
        return std::make_shared<OverlayStrItem>();
    }

    OverlayStrItemShPtr overlayStrItem = mOverlayStrItemMemPool.front();
    mOverlayStrItemMemPool.pop_front();
    return overlayStrItem;
}

void
Overlay::setMemOverlayStrItem(OverlayStrItemShPtr ptr)
{
    ptr->resetCharItemArray(*this);
#   ifdef ENABLE_MEMPOOL
    mOverlayStrItemMemPool.push_front(ptr);
#   else // else ENABLE_MEMPOOL    
    ptr.reset();
#   endif // end !ENABLE_MEMPOOL
}

Overlay::OverlayCharItemShPtr
Overlay::getMemOverlayCharItem()
{
    if (mOverlayCharItemMemPool.empty()) {
        return std::make_shared<OverlayCharItem>();
    }

    OverlayCharItemShPtr overlayCharItem = mOverlayCharItemMemPool.front();
    mOverlayCharItemMemPool.pop_front();
    return overlayCharItem;
}

void
Overlay::setMemOverlayCharItem(OverlayCharItemShPtr ptr)
{
#   ifdef ENABLE_MEMPOOL
    mOverlayCharItemMemPool.push_front(ptr);
#   else // else ENABLE_MEMPOOL    
    ptr.reset();
#   endif // end !ENABLE_MEMPOOL
}

Overlay::OverlayBoxItemShPtr
Overlay::getMemOverlayBoxItem()
{
    if (mOverlayBoxItemMemPool.empty()) {
        return std::make_shared<OverlayBoxItem>();
    }

    OverlayBoxItemShPtr overlayBoxItem = mOverlayBoxItemMemPool.front();
    mOverlayBoxItemMemPool.pop_front();
    return overlayBoxItem;
}

void
Overlay::setMemOverlayBoxItem(OverlayBoxItemShPtr ptr)
{
#   ifdef ENABLE_MEMPOOL
    mOverlayBoxItemMemPool.push_front(ptr);
#   else // else ENABLE_MEMPOOL    
    ptr.reset();
#   endif // end !ENABLE_MEMPOOL
}

Overlay::OverlayVLineItemShPtr
Overlay::getMemOverlayVLineItem()
{
    if (mOverlayVLineItemMemPool.empty()) {
        return std::make_shared<OverlayVLineItem>();
    }

    OverlayVLineItemShPtr overlayVLineItem = mOverlayVLineItemMemPool.front();
    mOverlayVLineItemMemPool.pop_front();
    return overlayVLineItem;
}

void
Overlay::setMemOverlayVLineItem(OverlayVLineItemShPtr ptr)
{
#   ifdef ENABLE_MEMPOOL
    mOverlayVLineItemMemPool.push_front(ptr);
#   else // else ENABLE_MEMPOOL    
    ptr.reset();
#   endif // end !ENABLE_MEMPOOL
}

void
Overlay::parserConfigure()
{
    mParser.description("telemetry overlay command");

    mParser.opt("showMemPoolSize", "", "show memory pool information",
                [&](Arg& arg) { return arg.msg(showMemPoolSize() + '\n'); });
}

std::string
Overlay::showMemPoolSize() const
{
    size_t strItemSize = mMaxOverlayStrItemMemPool * sizeof(OverlayStrItem);
    size_t charItemSize = mMaxOverlayCharItemMemPool * sizeof(OverlayCharItem);
    size_t boxItemSize = mMaxOverlayBoxItemMemPool * sizeof(OverlayBoxItem);
    size_t vLineItemSize = mMaxOverlayVLineItemMemPool * sizeof(OverlayVLineItem);

    using scene_rdl2::str_util::byteStr;

    std::ostringstream ostr;
    ostr << "memPool {\n"
         << "  mMaxOverlayStrItemMemPool:" << mMaxOverlayStrItemMemPool << " (" << byteStr(strItemSize) << ")\n"
         << "  mMaxOverlayCharItemMemPool:" << mMaxOverlayCharItemMemPool << " (" << byteStr(charItemSize) << ")\n"
         << "  mMaxOverlayBoxItemMemPool:" << mMaxOverlayBoxItemMemPool << " (" << byteStr(boxItemSize) << ")\n"
         << "  mMaxOverlayVLineItemMemPool:" << mMaxOverlayVLineItemMemPool << " (" << byteStr(vLineItemSize) << ")\n"
         << "}";
    return ostr.str();
}

} // namespace telemetry
} // namespace mcrt_dataio
