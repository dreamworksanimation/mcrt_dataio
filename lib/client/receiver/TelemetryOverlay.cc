// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "TelemetryOverlay.h"
#include "TelemetryLayout.h"

#include <scene_rdl2/common/except/exceptions.h>

#include <tbb/parallel_for.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

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

std::string
C3::panelStr() const
{
    std::ostringstream ostr;
    ostr << LayoutBase::colFg(bestContrastCol()) << LayoutBase::colBg(*this) << '('
         << std::setw(3) << static_cast<int>(mR) << ","
         << std::setw(3) << static_cast<int>(mG) << ","
         << std::setw(3) << static_cast<int>(mB)
         << ')';
    return ostr.str();
}

//------------------------------------------------------------------------------------------

FontCacheItem::FontCacheItem(const char c,
                             const FT_Bitmap& bitmap,
                             const unsigned bitmapLeft,
                             const unsigned bitmapTop,
                             const unsigned advanceX)
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
Font::getFontCacheItem(const char c)
{
    const unsigned glyphIndex = FT_Get_Char_Index(mFace, c);
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

void
OverlayCharItem::move(const int deltaX, const int deltaY)
//
// deltaX, deltaY should be regular coordinate value (not a FreeType coordinate)
//
{
    const FT_Pos ftDeltaX = Font::iToftPos(deltaX);
    const FT_Pos ftDeltaY = Font::iToftPos(deltaY);

    mFontBasePos.x += ftDeltaX;
    mFontBasePos.y -= ftDeltaY;
    mFontDataPos.x += ftDeltaX;
    mFontDataPos.y -= ftDeltaY;
}

std::string
OverlayCharItem::show(const unsigned winHeight) const
{
    auto showFTVec = [](const FT_Vector& v, unsigned wHeight) {
        std::ostringstream ostr;
        ostr << "x:" << v.x << " (ix:" << Font::ftPosToi(v.x) << ") ";

        const int iy = Font::ftPosToi(v.y);
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

void
OverlayStrItem::move(const int deltaX, const int deltaY)
{
    mStartX += deltaX;
    mStartY += deltaY;

    for (OverlayCharItemShPtr item : mCharItemArray) {
        item->move(deltaX, deltaY);
    }
}

bool
OverlayStrItem::entryNewCharItem(Overlay& overlay,
                                 Font& font,
                                 const FT_Vector& fontPos,
                                 const char c,
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
Overlay::clear(const C3& c3, const unsigned char alpha, const bool doParallel)
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
Overlay::boxFill(const BBox2i& bbox, const C3& c3, const unsigned char alpha, const bool doParallel)
{
    auto boxScanlineFill = [&](const size_t y) {
        unsigned char *ptr = getPixDataAddr(bbox.lower.x, y);
        for (unsigned x = bbox.lower.x; x <= bbox.upper.x; ++x) {
            setCol4(c3, alpha, ptr);
            ptr += 4;
        }
    };

    if (!doParallel) {
        for (unsigned y = bbox.lower.y; y <= bbox.upper.y; ++y) {
            boxScanlineFill(y);
        }
    } else {
        tbb::blocked_range<size_t> range(bbox.lower.y, bbox.upper.y + 1, 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                for (size_t y = range.begin(); y < range.end(); ++y) {
                    boxScanlineFill(y);
                }
            });
    }
}

void
Overlay::boxOutline(const BBox2i& bbox, const C3& c3, const unsigned char alpha, const bool doParallel)
{
    if (!doParallel) {
        hLine(bbox.lower.x, bbox.upper.x, bbox.lower.y, c3, alpha);
        vLine(bbox.upper.x, bbox.lower.y, bbox.upper.y, c3, alpha);
        hLine(bbox.lower.x, bbox.upper.x, bbox.upper.y, c3, alpha);
        vLine(bbox.lower.x, bbox.lower.y, bbox.upper.y, c3, alpha);
    } else {
        tbb::blocked_range<size_t> range(0, 4, 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
            switch (range.begin()) {
            case 0 : hLine(bbox.lower.x, bbox.upper.x, bbox.lower.y, c3, alpha); break;
            case 1 : vLine(bbox.upper.x, bbox.lower.y, bbox.upper.y, c3, alpha); break;
            case 2 : hLine(bbox.lower.x, bbox.upper.x, bbox.upper.y, c3, alpha); break;
            case 3 : vLine(bbox.lower.x, bbox.lower.y, bbox.upper.y, c3, alpha); break;
            default : break;
            }
        });
    }
}

void
Overlay::polygonFill(const std::vector<Vec2f>& polygon,
                     const unsigned xSubSamples, const unsigned ySubSamples,
                     const C3& c3, const unsigned char alpha)
{
    auto showInputParam = [&]() { // for debug
        std::ostringstream ostr;
        ostr << "polygon (size:" << polygon.size() << ")"
             << " width:" << getWidth() << " height:" << getHeight() << " {\n";
        for (size_t i = 0; i < polygon.size(); ++i) {
            ostr << "  i:" << i << " (" << polygon[i][0] << ',' << polygon[i][1] << ")\n"; 
        }
        ostr << "}";
        return ostr.str();
    }; // showInputParam()

    auto calcMinMaxY = [&](unsigned& min, unsigned& max) {
        float currMin = polygon[0][1];
        float currMax = currMin;
        for (const auto& p : polygon) {
            currMin = std::min(currMin, p[1]);
            currMax = std::max(currMax, p[1]);
        }
        if (currMax <= 0.0f || currMin >= static_cast<float>(getHeight())) return false; // outside window
        min = static_cast<unsigned>(std::max(0, static_cast<int>(std::floor(currMin))));
        max = std::min(static_cast<unsigned>(getHeight()) - 1, static_cast<unsigned>(std::floor(currMax)));
        return true;
    }; // calcMinMaxY()

    class Edge
    {
    public:
        Edge(const Vec2f& btm, const Vec2f& top)
            : mBtm {btm}
            , mTop {top}
            , mLeftX {(btm[0] < top[0]) ? btm[0] : top[0]}
        {}

        const Vec2f& mBtm;
        const Vec2f& mTop;
        const float mLeftX {0.0f};
    };
    
    std::vector<std::unique_ptr<Edge>> edgeTbl;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const Vec2f& a = polygon[i];
        const Vec2f& b = polygon[(i + 1) % polygon.size()];
        if (a[1] < b[1]) edgeTbl.emplace_back(std::make_unique<Edge>(a, b));
        else             edgeTbl.emplace_back(std::make_unique<Edge>(b, a));
    }
    std::sort(edgeTbl.begin(), edgeTbl.end(),
              [](const std::unique_ptr<Edge>& a, const std::unique_ptr<Edge>& b) { return a->mLeftX < b->mLeftX; });
        
    const float w = getWidth();
    auto edgeXScan = [&edgeTbl, &ySubSamples, &w](const unsigned y, const unsigned ySubPixReso,
                                                  std::vector<float> iTbl[], unsigned& sPix, unsigned& ePix) {
        auto calcIsectX = [](const float y, const Vec2f& btm, const Vec2f& top) {
            return btm[0] + ((y - btm[1]) / (top[1] - btm[1])) * (top[0] - btm[0]);
        }; // calcIsectX()

        auto subScanlines = [&](const unsigned y, const unsigned ySubPixReso,
                                std::vector<float> iTbl[], const Vec2f& btm, const Vec2f& top) {
            const float subPixH = 1.0f / static_cast<float>(ySubPixReso);
            float scanlineY = static_cast<float>(y) + subPixH * 0.5f;
            for (unsigned subY = 0; subY < ySubPixReso; ++subY) {
                if ((btm[1] <= scanlineY && scanlineY < top[1])) {
                    iTbl[subY].push_back(calcIsectX(scanlineY, btm, top));
                } 
                scanlineY += subPixH;
            }
        }; // subScanlines()

        auto subScanlinesFullCover = [&](const unsigned y, const unsigned ySubPixReso,
                                         std::vector<float> iTbl[], const Vec2f& btm, const Vec2f& top) {
            const float subPixH = 1.0f / static_cast<float>(ySubPixReso);
            const float scanlineY = static_cast<float>(y) + subPixH * 0.5f;
            float isectX = calcIsectX(scanlineY, btm, top);
            const float stepX = (1.0f / static_cast<float>(ySubPixReso)) / (top[1] - btm[1]) * (top[0] - btm[0]);
            for (unsigned subY = 0; subY < ySubPixReso; ++subY) {
                iTbl[subY].push_back(isectX);
                isectX += stepX;
            }
        }; // subScanlineFullCover()

        for (unsigned subY = 0; subY < ySubPixReso; ++subY) iTbl[subY].clear();            

        for (size_t i = 0; i < edgeTbl.size(); ++i) {
            const Vec2f& btm = edgeTbl[i]->mBtm;
            const Vec2f& top = edgeTbl[i]->mTop;
            if (top[1] <= static_cast<float>(y) || static_cast<float>(y + 1) <= btm[1]) continue;
            if ((btm[1] <= static_cast<float>(y) && static_cast<float>(y + 1) <= top[1])) {
                subScanlinesFullCover(y, ySubPixReso, iTbl, btm, top);
            } else {
                subScanlines(y, ySubPixReso, iTbl, btm, top);
            }
        }

        float xSegmentStart = w;
        float xSegmentEnd = 0.0f;
        for (unsigned subY = 0; subY < ySubPixReso; ++subY) {
            if (iTbl[subY].size() >= 2) {
                xSegmentStart = std::min(xSegmentStart, iTbl[subY].front());
                xSegmentEnd = std::max(xSegmentEnd, iTbl[subY].back());
            }
        }
        if (xSegmentStart >= xSegmentEnd) return false; // no intersection -> skip this scanline.
        if (xSegmentStart >= w || xSegmentEnd <= 0.0f) return false;
        sPix = static_cast<unsigned>(std::floor(std::max(0.0f, xSegmentStart)));
        ePix = static_cast<unsigned>(std::floor(std::min(w - 1.0f, xSegmentEnd)));
        return true;
    }; // edgeXScan()

    scanConvert(xSubSamples, ySubSamples, c3, alpha, calcMinMaxY, edgeXScan);
}

void
Overlay::circleFill(const Vec2f& center, const float radius, const C3& c3, const unsigned char alpha)
{
    auto calcMinMaxY = [&](unsigned& min, unsigned& max) {
        const float minF = center[1] - radius;
        const float maxF = center[1] + radius;
        if (maxF <= 0.0f || minF >= static_cast<float>(getHeight())) return false; // outside window
        min = static_cast<unsigned>(std::max(0, static_cast<int>(std::floor(minF))));
        max = std::min(static_cast<unsigned>(getHeight()) - 1, static_cast<unsigned>(std::floor(maxF)));
        return true;
    }; // calcMinMaxY()

    auto circleXScan = [&](const unsigned y, const int ySubPixReso,
                           std::vector<float> iTbl[], unsigned& sPix, unsigned& ePix) {
        auto calcIsectSpan = [&](const float scanlineY, float& x0, float& x1) {
            const float deltaY = scanlineY - center[1];
            const float sq = radius * radius - deltaY * deltaY;
            if (sq < 0.0f) return false;
            const float deltaX = sqrtf(sq);
            x0 = center[0] - deltaX;
            x1 = center[0] + deltaX;
            return true;
        }; // calcIsectSpan()

        const float subPixH = 1.0f / static_cast<float>(ySubPixReso);
        float xSegmentStart = static_cast<float>(getWidth());
        float xSegmentEnd = 0.0f;
        for (unsigned subY = 0; subY < ySubPixReso; ++subY) {
            const float scanlineY = static_cast<float>(y) + subPixH * (static_cast<float>(subY) + 0.5f);
            iTbl[subY].clear();

            float x0, x1;
            if (calcIsectSpan(scanlineY, x0, x1)) {
                iTbl[subY].push_back(x0);
                iTbl[subY].push_back(x1);
                xSegmentStart = std::min(xSegmentStart, iTbl[subY].front());
                xSegmentEnd = std::max(xSegmentEnd, iTbl[subY].back());
            }
        }
        if (xSegmentStart >= xSegmentEnd) return false; // no intersection -> skip this scanline
        if (xSegmentStart >= static_cast<float>(getWidth()) || xSegmentEnd <= 0.0f) return false;
        sPix = static_cast<unsigned>(std::floor(std::max(0.0f, xSegmentStart)));
        ePix = static_cast<unsigned>(std::floor(std::min(static_cast<float>(getWidth() - 1), xSegmentEnd)));
        return true;
    }; // circleIntersection()

    scanConvert(8, 8, c3, alpha, calcMinMaxY, circleXScan);
}

void
Overlay::circle(const Vec2f& center, const float radius, const float width, const C3& c3, const unsigned char alpha)
{
    auto calcMinMaxY = [&](unsigned& min, unsigned& max) {
        const float widthH = width * 0.5001f;
        const float minF = center[1] - radius - widthH;
        const float maxF = center[1] + radius + widthH;
        if (maxF <= 0.0f || minF >= static_cast<float>(getHeight())) return false; // outside window
        min = static_cast<unsigned>(std::max(0, static_cast<int>(std::floor(minF))));
        max = std::min(static_cast<unsigned>(getHeight()) - 1, static_cast<unsigned>(std::floor(maxF)));
        return true;
    }; // calcMinMaxY()

    auto circleXScan = [&](const unsigned y, const int ySubPixReso,
                           std::vector<float> iTbl[], unsigned& sPix, unsigned& ePix) {
        auto calcIsectSpan = [&](const float currRadius, const float scanlineY, float& x0, float& x1) {
            const float deltaY = scanlineY - center[1];
            const float sq = currRadius * currRadius - deltaY * deltaY;
            if (sq < 0.0f) return false;
            const float deltaX = sqrtf(sq);
            x0 = center[0] - deltaX;
            x1 = center[0] + deltaX;
            return true;
        }; // calcIsectSpan()

        const float subPixH = 1.0f / static_cast<float>(ySubPixReso);
        float xSegmentStart = static_cast<float>(getWidth());
        float xSegmentEnd = 0.0f;
        const float radiusOuter = radius + width * 0.5001f;
        const float radiusInner = radius - width * 0.5001f;
        for (unsigned subY = 0; subY < ySubPixReso; ++subY) {
            const float scanlineY = static_cast<float>(y) + subPixH * (static_cast<float>(subY) + 0.5f);
            iTbl[subY].clear();

            float x0, x1, x2, x3;
            if (calcIsectSpan(radiusOuter, scanlineY, x0, x1)) {
                iTbl[subY].push_back(x0);
                if (calcIsectSpan(radiusInner, scanlineY, x2, x3)) {
                    iTbl[subY].push_back(x2);
                    iTbl[subY].push_back(x3);
                }
                iTbl[subY].push_back(x1);
                xSegmentStart = std::min(xSegmentStart, iTbl[subY].front());
                xSegmentEnd = std::max(xSegmentEnd, iTbl[subY].back());
            }
        }
        if (xSegmentStart >= xSegmentEnd) return false; // no intersection -> skip this scanline
        if (xSegmentStart >= static_cast<float>(getWidth()) || xSegmentEnd <= 0.0f) return false;
        sPix = static_cast<unsigned>(std::floor(std::max(0.0f, xSegmentStart)));
        ePix = static_cast<unsigned>(std::floor(std::min(static_cast<float>(getWidth() - 1), xSegmentEnd)));
        return true;
    }; // circleIntersection()

    scanConvert(8, 8, c3, alpha, calcMinMaxY, circleXScan);
}

void
Overlay::vLine(const unsigned x, const unsigned minY, const unsigned maxY,
               const C3& c3, const unsigned char alpha)
{
    unsigned char *ptr = getPixDataAddr(x, minY);
    const int offset = mWidth * 4;
    for (unsigned y = minY; y <= maxY; ++y) {
        setCol4(c3, alpha, ptr);
        ptr += offset;
    }
}

void
Overlay::hLine(const unsigned minX, const unsigned maxX, const unsigned y,
               const C3& c3, const unsigned char alpha)
{
    unsigned char *ptr = getPixDataAddr(minX, y);
    for (unsigned x = minX; x <= maxX; ++x) {
        setCol4(c3, alpha, ptr);
        ptr += 4;
    }
}

void
Overlay::fill(const unsigned char r, const unsigned char g, const unsigned char b, const unsigned char a)
{
    for (size_t i = 0; i < mPixelsRGBA.size() / 4; ++i) {
        setCol4(C3(r, g, b), a, &mPixelsRGBA[i * 4]);
    }
}

unsigned
Overlay::getMaxYLines(const Font& font, unsigned& offsetBottomPixY, unsigned& stepPixY) const
{
    stepPixY = static_cast<unsigned>(font.getFontSizePoint() * lineSpacingScale);
    const unsigned maxYLines =  mHeight / stepPixY;
    const unsigned spaceY = mHeight - maxYLines * stepPixY;
    offsetBottomPixY = spaceY / 2.0f;
    return maxYLines;
}

void
Overlay::drawClearAllItems()
{
    drawLineClear();
    drawCircleClear();
    drawBoxClear();
    drawBoxOutlineClear();
    drawVLineClear();
    drawHLineClear();
    drawStrClear();
}

void
Overlay::drawFlushAllItems(const bool doParallel)
{
    //
    // Order does matter
    //
    drawLineFlush(doParallel);
    drawCircleFlush(doParallel);
    drawBoxFlush(doParallel);
    drawBoxOutlineFlush(doParallel);
    drawVLineFlush(doParallel);
    drawHLineFlush(doParallel);
    drawStrFlush(doParallel);
}

bool
Overlay::drawStr(Font& font,
                 const unsigned startX, const unsigned startY,
                 const std::string& str,
                 const C3& c3,
                 std::string& error)
{
    try {
        OverlayStrItemShPtr item = mStrItemMemMgr.getMemItem();
        item->set(*this, font, startX, startY, mHeight, str, c3);
        mDrawStrArray.push_back(item);
        mFontStepX = item->getFirstCharStepX();
    }
    catch (scene_rdl2::except::RuntimeError& e) {
        std::ostringstream ostr;
        ostr << "ERROR : construct new OverlayStrItem failed." << " RuntimeError:" << e.what();
        error = ostr.str();
        return false;
    }

    return true;
}

void    
Overlay::drawBox(const BBox2i& box, const C3& c3, const unsigned char alpha)
{
    OverlayBoxItemShPtr item = mBoxItemMemMgr.getMemItem();
    item->set(box, c3, alpha);
    mDrawBoxArray.push_back(item);
}

void    
Overlay::drawBoxBar(const BBox2i& box, const C3& c3, const unsigned char alpha)
{
    OverlayBoxItemShPtr item = mBoxItemMemMgr.getMemItem();
    item->set(box, c3, alpha);
    mDrawBoxBarArray.push_back(item);
}

void    
Overlay::drawBoxOutline(const BBox2i& box, const C3& c3, const unsigned char alpha)
{
    OverlayBoxOutlineItemShPtr item = mBoxOutlineItemMemMgr.getMemItem();
    item->set(box, c3, alpha);
    mDrawBoxOutlineArray.push_back(item);
}

void
Overlay::drawVLine(const unsigned x, const unsigned minY, const unsigned maxY, const C3& c3, const unsigned char alpha)
{
    OverlayVLineItemShPtr item = mVLineItemMemMgr.getMemItem();
    item->set(x, minY, maxY, c3, alpha);
    mDrawVLineArray.push_back(item);
}

void
Overlay::drawHLine(const unsigned minX, const unsigned maxX, const unsigned y, const C3& c3, const unsigned char alpha)
{
    OverlayHLineItemShPtr item = mHLineItemMemMgr.getMemItem();
    item->set(minX, maxX, y, c3, alpha);
    mDrawHLineArray.push_back(item);
}

void
Overlay::drawLine(const unsigned sx,
                  const unsigned sy,
                  const unsigned ex,
                  const unsigned ey,
                  const float width,
                  const bool drawStartPoint,
                  const float radiusStartPoint,
                  const bool drawEndPoint,
                  const float radiusEndPoint,
                  const C3& c3,
                  const unsigned char alpha)
{
    OverlayLineItemShPtr item = mLineItemMemMgr.getMemItem();
    item->set(sx, sy, ex, ey, width,
              drawStartPoint, radiusStartPoint, drawEndPoint, radiusEndPoint,
               c3, alpha);
    mDrawLineArray.push_back(item);
}

void
Overlay::drawCircle(const unsigned sx,
                    const unsigned sy,
                    const float radius,
                    const float width,
                    const C3& c3,
                    const unsigned char alpha)
{
    OverlayCircleItemShPtr item = mCircleItemMemMgr.getMemItem();
    item->set(sx, sy, radius, width, c3, alpha);
    mDrawCircleArray.push_back(item);
}

Overlay::BBox2i
Overlay::calcDrawBbox(const size_t startStrItemId, const size_t endStrItemId) const
{
    FreeTypeBBox bbox = emptyFreeTypeBBox();
    for (size_t i = startStrItemId; i <= endStrItemId; ++i) {
        bbox.extend(mDrawStrArray[i]->getBBox());
    }        

    auto flipY = [&](const unsigned y) {
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
Overlay::moveStr(const size_t strItemId, const int deltaX, const int deltaY)
{
    if (strItemId >= mDrawStrArray.size()) return; // just in case
    mDrawStrArray[strItemId]->move(deltaX, deltaY);
}

void
Overlay::finalizeRgb888(std::vector<unsigned char>& rgbFrame,
                        const unsigned frameWidth,
                        const unsigned frameHeight,
                        const bool top2bottomFlag,
                        const Align hAlign,
                        const Align vAlign,
                        std::vector<unsigned char>* bgArchive,
                        const bool doParallel) const
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

// This directive enables runtime verification for scan convert code. This should be used
// for debugging purposes and should be commented out for the release vesion.
//#define RUNTIME_SCAN_CONVERT_VERIFY

void
Overlay::scanConvert(const unsigned xSubSamples, const unsigned ySubSamples,
                     const C3& c3, const unsigned char alpha,
                     const std::function<bool(unsigned& min, unsigned& max)>& calcMinMaxY,
                     const std::function<bool(const unsigned y, const unsigned ySubPixReso,
                                              std::vector<float> iTbl[], unsigned& sPix,
                                              unsigned& ePix)>& xScan)
{
    auto showIsectTbl = [](const std::vector<float> iTbl[], const unsigned ySubSamples) { // for debug
        std::ostringstream ostr;
        ostr << "iTbl (size:" << ySubSamples << ") {\n";
        for (unsigned ySub = 0; ySub < ySubSamples; ++ySub) {
            ostr << "  iTbl[ySub:" << ySub << "] (size:" << iTbl[ySub].size() << ") {\n";
            for (size_t i = 0; i < iTbl[ySub].size(); ++i) {
                ostr << "    i:" << i << ' ' << iTbl[ySub][i] << '\n';
            }
            ostr << "  }\n";
        }
        ostr << "}";
        return ostr.str();
    }; // showIsectTbl()

    auto setupCoverageTbl = [](const unsigned sPix, const unsigned ePix, std::vector<unsigned>& coverageTbl) {
        const size_t tblSize = ePix - sPix + 1;
        if (coverageTbl.size() < tblSize) coverageTbl.resize(tblSize);
        std::memset(coverageTbl.data(), 0, coverageTbl.size() * sizeof(unsigned));
    }; // setupCoverageTbl()

    //
    // The following code uses nested lambda function definitions. There are several reasons for adopting
    // this style, even at the cost of reduced readability.
    //
    //   1 Ability to limit scope (prevents unintended exposure)
    //     A nested lambda is not visible outside its parent lambda, making it ideal for defining small
    //     helper functions while avoiding name collisions. (However, since lambdas are variables, they
    //     cannot be forward-declared. As a result, child lambdas must be defined at the beginning of the
    //     parent lambda, which can reduce readability-this requires caution.) Unfortunately, C++
    //     currently does not provide an equivalent mechanism for nesting regular functions.
    //   2 Shared capture with the parent lambda
    //     A child lambda can reference the parent lambda's capture directly, so there is no need to
    //     construct complex capture chains manually.
    //   3 Clear separation of logic
    //     Using child lambdas as helpers inside the parent lambda can make the structure and intent of
    //     the implementation clearer.
    //   4 Function-level inlining
    //     If the child lambda is sufficiently small, the compiler can easily inline it, making the runtime
    //     overhead effectively zero. Compared to function pointers or std::function, lambdas are much
    //     lighter-weight and incur no additional runtime cost-they are essentially lightweight function
    //     objects.
    //
    // Even at the cost of reduced readability, these advantages make nested lambda functions worthwhile.
    // In the code below, execution speed is the top priority, which is why this nested-lambda
    // implementation approach is used.
    // A nearly identical structure implemented via class inheritance exists in
    //
    //     mcrt_dataio::telemetry::ScanConvert
    //
    // but the lambda-based implementation is approximately 20% to 30% faster. For understanding the logic,
    // the ScanConvert class and its derived ScanConvertPolygon class provide a clearer, more readable
    // structure, so please refer to those as well.
    //
    auto calcCoverage = [](const unsigned ySubSamples, const unsigned xSubSamples,
                           const unsigned sPix, const unsigned ePix,
                           const std::vector<float> isectTbl[],
                           std::vector<unsigned>& coverageXTbl) {
        auto pixXLoop = [](const unsigned xSubPixReso,
                           const unsigned sPix, const unsigned ePix,
                           const std::vector<float>& isectTbl,
                           std::vector<unsigned>& coverageXTbl) {
#ifdef RUNTIME_SCAN_CONVERT_VERIFY
            auto verifyEdgeCoverage = [](const unsigned xSubPixReso,
                                         const unsigned x, const float xSegmentStart, const float xSegmentEnd,
                                         const unsigned coverage) {
                unsigned verifyCoverage = 0;
                for (unsigned subPix = 0; subPix < xSubPixReso; ++subPix) {
                    const float xPos = static_cast<float>(x) +
                        (static_cast<float>(subPix) + 0.5f) / static_cast<float>(xSubPixReso);
                    if (xSegmentStart >= 0.0f && xSegmentEnd >= 0.0f) {
                        if (xSegmentStart <= xPos && xPos < xSegmentEnd) verifyCoverage++;
                    } else if (xSegmentStart >= 0.0f && xSegmentEnd < 0.0f) {
                        if (xSegmentStart <= xPos) verifyCoverage++;
                    } else if (xSegmentStart < 0.0f && xSegmentEnd >= 0.0f) {
                        if (xPos < xSegmentEnd) verifyCoverage++;
                    }
                }
                if (verifyCoverage != coverage) std::cerr << ">> TelemetryOverlay.cc edgePix verify-ERROR\n";
            }; // verifyEdgeCoverage()
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY

            auto calcSubPixId = [](const float edgeX, const unsigned xSubPixReso) {
                float intPart;
                const float fraction =
                    std::modf((edgeX - std::floor(edgeX)) * static_cast<float>(xSubPixReso), &intPart);
                return static_cast<int>(intPart + (fraction > 0.5f));
            }; // calcSubPixId()

            auto edgePix = [&](const unsigned xSubPixReso, const unsigned x,
                               const unsigned sPix, const float xSegmentStart, const float xSegmentEnd,
                               std::vector<unsigned>& coverageXTbl) {
                const int subPixIdStart = calcSubPixId(xSegmentStart, xSubPixReso);
                const int subPixIdEnd = calcSubPixId(xSegmentEnd, xSubPixReso) - 1;
                const unsigned coverage = subPixIdEnd - subPixIdStart + 1;
#ifdef RUNTIME_SCAN_CONVERT_VERIFY
                verifyEdgeCoverage(xSubPixReso, x, xSegmentStart, xSegmentEnd, coverage);
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY
                coverageXTbl[x - sPix] += coverage;
            }; // edgePix()

            auto edgePixL = [&](const unsigned xSubPixReso, const unsigned x,
                                const unsigned sPix, const float xSegmentStart,
                                std::vector<unsigned>& coverageXTbl) {
                const int subPixIdStart = calcSubPixId(xSegmentStart, xSubPixReso);
                const unsigned coverage = xSubPixReso - subPixIdStart;
#ifdef RUNTIME_SCAN_CONVERT_VERIFY
                verifyEdgeCoverage(xSubPixReso, x, xSegmentStart, -1.0f, coverage);
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY
                coverageXTbl[x - sPix] += coverage;
            }; // edgePixL()

            auto edgePixR = [&](const unsigned xSubPixReso, const unsigned x,
                                const unsigned sPix, const float xSegmentEnd,
                               std::vector<unsigned>& coverageXTbl) {
                const int subPixIdEnd = calcSubPixId(xSegmentEnd, xSubPixReso) - 1;
                const unsigned coverage = subPixIdEnd + 1;
#ifdef RUNTIME_SCAN_CONVERT_VERIFY
                verifyEdgeCoverage(xSubPixReso, x, -1.0f, xSegmentEnd, coverage);
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY
                coverageXTbl[x - sPix] += coverage;
            }; // edgePixR()

            for (size_t i = 0; i < isectTbl.size(); i += 2) {
                const float xSegmentStart = isectTbl[i];
                const float xSegmentEnd = isectTbl[i + 1];
                const unsigned s = static_cast<unsigned>(std::floor(std::max(0.0f, xSegmentStart)));
                const unsigned e = std::min(ePix, static_cast<unsigned>(std::floor(xSegmentEnd)));
                if (e < s) continue;
                if (s == e) {
                    edgePix(xSubPixReso, s, sPix, xSegmentStart, xSegmentEnd, coverageXTbl);
                } else if (s + 1 == e) {
                    edgePixL(xSubPixReso, s, sPix, xSegmentStart, coverageXTbl);
                    edgePixR(xSubPixReso, e, sPix, xSegmentEnd, coverageXTbl);
                } else {
                    edgePixL(xSubPixReso, s, sPix, xSegmentStart, coverageXTbl);
                    const unsigned currMax = std::min(e - 1, ePix);
                    for (unsigned xPix = s + 1; xPix <= currMax; ++xPix) {
                        coverageXTbl[xPix - sPix] += xSubPixReso; 
                    }
                    edgePixR(xSubPixReso, e, sPix, xSegmentEnd, coverageXTbl);
                }
            }
        }; // pixXLoop()

        for (unsigned subY = 0; subY < ySubSamples; ++subY) { 
            if (isectTbl[subY].size() < 2) continue;
            pixXLoop(xSubSamples, sPix, ePix, isectTbl[subY], coverageXTbl);
        }
    }; // calcCoverage()

    auto scanlineFill = [&](const unsigned y, const unsigned sPix, const unsigned ePix, const float invSubSamples,
                            const std::vector<unsigned>& coverageXTbl) {
        for (unsigned xPix = sPix ; xPix <= ePix ; ++xPix) {
            const float coverage = coverageXTbl[xPix - sPix] * invSubSamples;
            if (coverage > 0.0f) {
                const float currAlpha = static_cast<unsigned char>(std::clamp(static_cast<float>(alpha) * coverage, 0.0f, 255.0f));
                pixMaxFill(xPix, y, c3, currAlpha);
            }
        }
    }; // scanlineFill()

    //------------------------------

    float invSubSamples = 1.0f / static_cast<float>(xSubSamples * ySubSamples);

#ifdef SC_TIMELOG
    mScanConvertTimeLog.start();
#endif // end of SC_TIMELOG

    unsigned minY, maxY;
    if (!calcMinMaxY(minY, maxY)) {
#ifdef SC_TIMELOG
        mScanConvertTimeLog.endCalcMinMaxY();
#endif // end of SC_TIMELOG
        return;  // outside window
    }
#ifdef SC_TIMELOG
    mScanConvertTimeLog.endCalcMinMaxY();
#endif // end of SC_TIMELOG

    std::vector<float> iTbl[ySubSamples];
    std::vector<unsigned> cTbl;
    for (unsigned y = minY ; y <= maxY ; ++y) {
        unsigned sPix, ePix;
        if (!xScan(y, ySubSamples, iTbl, sPix, ePix)) {
#ifdef SC_TIMELOG
            mScanConvertTimeLog.endXScan();
#endif // end of SC_TIMELOG
            continue;
        }
#ifdef SC_TIMELOG
        mScanConvertTimeLog.endXScan();
#endif // end of SC_TIMELOG

        setupCoverageTbl(sPix, ePix, cTbl);
#ifdef SC_TIMELOG
        mScanConvertTimeLog.endSetupCoverageTbl();
#endif // end of SC_TIMELOG

        calcCoverage(ySubSamples, xSubSamples, sPix, ePix, iTbl, cTbl);
#ifdef SC_TIMELOG
        mScanConvertTimeLog.endCalcCoverage();
#endif // end of SC_TIMELOG

        scanlineFill(y, sPix, ePix, invSubSamples, cTbl);
#ifdef SC_TIMELOG
        mScanConvertTimeLog.endScanlineFill();
#endif // end of SC_TIMELOG
    }
}

void
Overlay::overlayDrawFontCache(const OverlayCharItemShPtr charItem)
{
    auto flipY = [&](const unsigned freeTypeY) {
        return mHeight - 1 - freeTypeY;
    };

    const C3& bgC3 = charItem->getBgC3();
    if (!bgC3.isBlack()) {

        const unsigned offsetY =
            (charItem->getBgYAdjustScale() > 0.0f)
            ? charItem->getHeight() * charItem->getBgYAdjustScale()
            : 0;
        for (unsigned y = 0; y < charItem->getHeight(); ++y) {
            const unsigned fbSpaceY = flipY(charItem->getBaseY() - y + offsetY);
            for (unsigned x = 0; x < charItem->getWidth(); ++x) {
                const unsigned fbSpaceX = charItem->getBaseX() + x;
                setCol4(bgC3, 255, getPixDataAddr(fbSpaceX, fbSpaceY));                
            }
        }
    }

    const FontCacheItem* fontCacheItem = charItem->getFontCacheItem().get();
    if (fontCacheItem->isSpace()) return;

    const unsigned freeTypeX = charItem->getPosX();
    const unsigned freeTypeY = charItem->getPosY();
    const C3& fgC3 = charItem->getFgC3();
    for (unsigned by = 0; by < fontCacheItem->getRows(); ++by) {
        const unsigned fbY = flipY(freeTypeY + by);
        for (unsigned bx = 0; bx < fontCacheItem->getWidth(); ++bx) {
            const unsigned char alpha = fontCacheItem->get(bx, by);
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
Overlay::drawStrClear()
{
    for (auto itr : mDrawStrArray) {
        itr->resetCharItemArray(*this); 
        mStrItemMemMgr.setMemItem(itr);
    }
    mStrItemMemMgr.updateMax();
    mCharItemMemMgr.updateMax();

    mDrawStrArray.clear();
}

void
Overlay::drawStrFlush(const bool doParallel)
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
        mBoxItemMemMgr.setMemItem(itr);
    }
    for (auto itr : mDrawBoxBarArray) {
        mBoxItemMemMgr.setMemItem(itr);
    }
    mBoxItemMemMgr.updateMax();

    mDrawBoxArray.clear();
    mDrawBoxBarArray.clear();
}

void    
Overlay::drawBoxFlush(const bool doParallel)
{
    auto drawBoxFill = [&](const OverlayBoxItemShPtr boxItem, const bool doParallel) {
        boxFill(boxItem->getBBox(), boxItem->getC(), boxItem->getAlpha(), doParallel);
    };

    auto drawBoxArray = [&](std::vector<OverlayBoxItemShPtr>& boxArray) {
        if (!doParallel) {
            for (size_t id = 0; id < boxArray.size(); ++id) {
                drawBoxFill(boxArray[id], false);
            }
        } else {
            tbb::blocked_range<size_t> range(0, boxArray.size(), 1);
            tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                for (size_t id = range.begin(); id < range.end(); ++id) {
                    drawBoxFill(boxArray[id], true);
                }
            });
        }
    };

    drawBoxArray(mDrawBoxArray);
    drawBoxArray(mDrawBoxBarArray);
}

void
Overlay::drawBoxOutlineClear()
{
    for (auto itr : mDrawBoxOutlineArray) {
        mBoxOutlineItemMemMgr.setMemItem(itr);
    }
    mBoxOutlineItemMemMgr.updateMax();

    mDrawBoxOutlineArray.clear();
}

void    
Overlay::drawBoxOutlineFlush(const bool doParallel)
{
    auto drawBoxOutline = [&](const OverlayBoxOutlineItemShPtr boxItem, const bool doParallel) {
        boxOutline(boxItem->getBBox(), boxItem->getC(), boxItem->getAlpha(), doParallel);
    };

    if (!doParallel) {
        for (size_t id = 0; id < mDrawBoxOutlineArray.size(); ++id) {
            drawBoxOutline(mDrawBoxOutlineArray[id], false);
        }
    } else {
        tbb::blocked_range<size_t> range(0, mDrawBoxOutlineArray.size(), 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
            for (size_t id = range.begin(); id < range.end(); ++id) {
                drawBoxOutline(mDrawBoxOutlineArray[id], true);
            }
        });
    }
}

void
Overlay::drawVLineClear()
{
    for (auto itr : mDrawVLineArray) {
        mVLineItemMemMgr.setMemItem(itr);
    }
    mVLineItemMemMgr.updateMax();

    mDrawVLineArray.clear();
}

void
Overlay::drawVLineFlush(const bool doParallel)
{
    auto drawLine = [&](const size_t id) {
        vLine(mDrawVLineArray[id]->getX(), mDrawVLineArray[id]->getMinY(), mDrawVLineArray[id]->getMaxY(),
              mDrawVLineArray[id]->getC(), mDrawVLineArray[id]->getAlpha());
    };

    if (!doParallel) {
        for (size_t id = 0; id < mDrawVLineArray.size(); ++id) {
            drawLine(id);
        }
    } else {
        tbb::blocked_range<size_t> range(0, mDrawVLineArray.size(), 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
            for (size_t id = range.begin(); id < range.end(); ++id) {
                drawLine(id);
            }
        });
    }
}

void
Overlay::drawHLineClear()
{
    for (auto itr : mDrawHLineArray) {
        mHLineItemMemMgr.setMemItem(itr);
    }
    mHLineItemMemMgr.updateMax();

    mDrawHLineArray.clear();
}

void
Overlay::drawHLineFlush(const bool doParallel)
{
    auto drawLine = [&](const size_t id) {
        hLine(mDrawHLineArray[id]->getMinX(), mDrawHLineArray[id]->getMaxX(), mDrawHLineArray[id]->getY(),
              mDrawHLineArray[id]->getC(), mDrawHLineArray[id]->getAlpha());
    };

    if (!doParallel) {
        for (size_t id = 0; id < mDrawHLineArray.size(); ++id) {
            drawLine(id);
        }
    } else {
        tbb::blocked_range<size_t> range(0, mDrawHLineArray.size(), 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
            for (size_t id = range.begin(); id < range.end(); ++id) {
                drawLine(id);
            }
        });
    }
}

void
Overlay::drawLineClear()
{
    for (auto itr : mDrawLineArray) {
        mLineItemMemMgr.setMemItem(itr);
    }
    mLineItemMemMgr.updateMax();

    mDrawLineArray.clear();
}

void
Overlay::drawLineFlush(const bool doParallel)
{
    auto drawLine = [&](const size_t id) {
        const OverlayLineItemShPtr linePtr = mDrawLineArray[id];
        line(Vec2f(linePtr->getSx(), linePtr->getSy()),
             Vec2f(linePtr->getEx(), linePtr->getEy()),
             linePtr->getWidth(),
             linePtr->getDrawStartPoint(), linePtr->getRadiusStartPoint(),
             linePtr->getDrawEndPoint(), linePtr->getRadiusEndPoint(),
             linePtr->getC(), linePtr->getAlpha());
    };

    if (!mDrawLineArray.size()) return;

    if (!doParallel) {
        for (size_t id = 0; id < mDrawLineArray.size(); ++id) {
            drawLine(id);
        }
    } else {
        //
        // Strictly speaking, we don't have an atomic operation of frame buffer I/O, and
        // multiple threads might access the same pixel at the same time during line segment
        // drawing. This situation might create the wrong color of that pixel. However, it
        // might be acceptable in most cases, and the important point is that this is not a
        // crash issue.
        // Considering the runtime performance, I'm going to keep this code as is. In the future,
        // if this wrong pixel color regarding the multi-thread causes the issue, we should
        // consider more deeply.
        //
        tbb::blocked_range<size_t> range(0, mDrawLineArray.size(), 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
            for (size_t id = range.begin(); id < range.end(); ++id) {
                drawLine(id);
            }
        });
    }
}

void
Overlay::drawCircleClear()
{
    for (auto itr : mDrawCircleArray) {
        mCircleItemMemMgr.setMemItem(itr);
    }
    mCircleItemMemMgr.updateMax();

    mDrawCircleArray.clear();
}

void
Overlay::drawCircleFlush(const bool doParallel)
{
    auto drawCircle = [&](const size_t id) {
        const OverlayCircleItemShPtr circlePtr = mDrawCircleArray[id];
        circle(Vec2f(circlePtr->getX(), circlePtr->getY()),
               circlePtr->getRadius(),
               circlePtr->getWidth(),
               circlePtr->getC(),
               circlePtr->getAlpha());
    };

    if (!mDrawCircleArray.size()) return;

    if (!doParallel) {
        for (size_t id = 0; id < mDrawCircleArray.size(); ++id) {
            drawCircle(id);
        }
    } else {
        tbb::blocked_range<size_t> range(0, mDrawCircleArray.size(), 1);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
            for (size_t id = range.begin(); id < range.end(); ++id) {
                drawCircle(id);
            }
        });
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
    auto adjustSrcRange = [](const unsigned fgSize, const unsigned bgSize, const Align align,
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
    auto adjustDstStartPos = [](const unsigned fgSize, const unsigned bgSize, const Align align, const bool flip) {
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
    auto calcFgDataAddr = [&](const unsigned x, const unsigned y) { return &fgFrameRGBA[(y * fgWidth + x) * 4]; };
    auto calcBgDataAddr = [&](const unsigned x, const unsigned y) { return &bgFrameRGB[(y * bgWidth + x) * 3]; };

    unsigned fgXmin, fgXmax, fgYmin, fgYmax;
    adjustSrcRange(fgWidth, bgWidth, hAlign, fgXmin, fgXmax);
    adjustSrcRange(fgHeight, bgHeight, vAlign, fgYmin, fgYmax);

    if (!doParallel) {
        // single thread execution
        unsigned bgY = adjustDstStartPos(fgHeight, bgHeight, vAlign, top2bottomFlag);
        const unsigned bgX = adjustDstStartPos(fgWidth, bgWidth, hAlign, false);

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
        const unsigned fgYsize = fgYmax - fgYmin + 1;
        constexpr size_t grainSize = 2;
        tbb::blocked_range<size_t> range(0, fgYsize, grainSize);
        tbb::parallel_for(range, [&](const tbb::blocked_range<size_t>& range) {
                unsigned bgY = adjustDstStartPos(fgHeight, bgHeight, vAlign, top2bottomFlag);
                bgY += ((top2bottomFlag) ? - range.begin() : range.begin());
                const unsigned bgX = adjustDstStartPos(fgWidth, bgWidth, hAlign, false);
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
        ostr << "(r:" << static_cast<int>(pix[0])
             << ",g:" << static_cast<int>(pix[1])
             << ",b:" << static_cast<int>(pix[2])
             << ",a:" << static_cast<int>(pix[3]) << ')';
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "pix " << showPos() << ' ' << showPixCol(&frameRGBA[(pixY * frameWidth + pixX) * 4]);
    return ostr.str();
}

void
Overlay::parserConfigure()
{
    mParser.description("telemetry overlay command");

    mParser.opt("showMemPoolSize", "", "show memory pool information",
                [&](Arg& arg) { return arg.msg(showMemPoolSize() + '\n'); });
    mParser.opt("lineDrawType", "<bresenham|dda|wu|wuHQ|ol|ol2|hybrid|show>", "set line drawing logic type",
                [&](Arg& arg) {
                    if (arg() != "show") {
                        if (arg() == "bresenham") setLineDrawFunc(LineDrawType::BRESENHAM);
                        else if (arg() == "dda") setLineDrawFunc(LineDrawType::DDA);
                        else if (arg() == "wu") setLineDrawFunc(LineDrawType::WU);
                        else if (arg() == "wuHQ") setLineDrawFunc(LineDrawType::WU_HQ);
                        else if (arg() == "ol") setLineDrawFunc(LineDrawType::OUTLINE_STROKE);
                        else if (arg() == "ol2") setLineDrawFunc(LineDrawType::OUTLINE_STROKE2);
                        else if (arg() == "hybrid") setLineDrawFunc(LineDrawType::HYBRID);
                        else {
                            std::ostringstream ostr;
                            ostr << "unknown lineDrawType:" << arg();
                            return arg.msg(ostr.str() + '\n');
                        }
                    }
                    arg++;
                    return arg.msg(showLineDrawType() + '\n');
                });
}

std::string
Overlay::showMemPoolSize() const
{
    std::ostringstream ostr;
    ostr << "memPool {\n"
         << "  mStrItemMemMgr: " << mStrItemMemMgr.showSize() << '\n'
         << "  mCharItemMemMgr: " << mCharItemMemMgr.showSize() << '\n'
         << "  mBoxItemMemMgr: " << mBoxItemMemMgr.showSize() << '\n'
         << "  mBoxOutlineItemMemMgr: " << mBoxOutlineItemMemMgr.showSize() << '\n'
         << "  mVLineItemMemMgr: " << mVLineItemMemMgr.showSize() << '\n'
         << "  mHLineItemMemMgr: " << mHLineItemMemMgr.showSize() << '\n'
         << "  mLineItemMemMgr: " << mLineItemMemMgr.showSize() << '\n'
         << "  mCircleitemMemMgr: " << mCircleItemMemMgr.showSize() << '\n'
         << "}";
    return ostr.str();
}

std::string
Overlay::showLineDrawType() const
{
    std::ostringstream ostr;
    ostr << "mLineDrawType:" << lineDrawStr(mLineDrawType);
    return ostr.str();
}

std::string
Overlay::lineDrawStr(const LineDrawType type)
{
    switch (type) {
    case LineDrawType::BRESENHAM : return "BRESENHAM";
    case LineDrawType::DDA : return "DDA";
    case LineDrawType::WU : return "WU";
    case LineDrawType::WU_HQ : return "WU_HQ";
    case LineDrawType::OUTLINE_STROKE : return "OUTLINE_STROKE";
    case LineDrawType::OUTLINE_STROKE2 : return "OUTLINE_STROKE2";
    case LineDrawType::HYBRID : return "HYBRID";
    default : return "?";
    }
}

} // namespace telemetry
} // namespace mcrt_dataio
