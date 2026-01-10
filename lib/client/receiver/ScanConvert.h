// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cmath> // modf
#include <vector>

namespace mcrt_dataio {
namespace telemetry {

class Overlay;
class C3;

class ScanConvert
//
// This class is a base class for performing scan-conversion processing required to render filled 2D shapes.
// Derived classes-such as those for polygons or circles-implement their specific scan-conversion logic by
// inheriting from this class. After defining two virtual functions and then calling scanConvert(),
// the class performs scan conversion at the specified sub-sample resolution. It then writes the per-pixel
// coverage information, including anti-aliasing results, into the framebuffer.
// The two virtual functions that must be implemented are calcMinMaxY(), which defines the range in the Y
// direction, and xScan(), which defines the left and right positions on each scanline. As long as these
// functions are implemented correctly, it is possible to perform scan conversion not only for polygons but
// for any kind of primitive, and to produce high-quality 2D rendering with anti-aliasing.
//
// For a concrete usage example, please refer to mcrt_dataio::telemetry::ScanConvertPolygon.
//
{
public:
    ScanConvert(Overlay& overlay, const unsigned xSubSample, const unsigned ySubSample)
        : mOverlay {overlay}
    {}
    virtual ~ScanConvert() {}

protected:
    // The main function that performs the rendering of this primitive using the scan-conversion method.
    void scanConvert(const unsigned xSubSamples, const unsigned ySubSamples, const C3& c3, const unsigned char alpha);

    // Compute the pixel coverage values using a brute-force method and perform verification.
    virtual bool calcMinMaxY(unsigned& min, unsigned& max) = 0;

    // Compute the active edges for the current scanline Y.
    // For the current scanline Y, compute all intersection points between the edges and each sub-scanline,
    // and store the results in mIsectTbl[subY] for each corresponding sub-scanline.
    // The left (start) and right (end) intersection points produced by an edge define an active segment on
    // a given sub-scanline. Each active segment is stored in mIsectTbl[subY] as a pair consisting of the
    // start and end values. Therefore, the number of entries in each mIsectTbl[subY] will always be even.
    // If multiple active segments exist, they are assumed to be non-overlapping. (If active segments overlap,
    // the current logic will not render the primitive correctly.)
    // The endpoints of each active segment must be sorted in the X direction.
    virtual bool xScan(const unsigned y, unsigned& sPix, unsigned& ePix) = 0;

    // Allocate the required amount of memory for the pixel coverage table.
    void setupCoverageTbl(const unsigned sPix, const unsigned ePix);

    // Process all sub-scanlines within a single scanline.
    void calcCoverage(const unsigned sPix, const unsigned ePix);

    // Update the coverage information for a single sub-scanline based on all active edge segments.
    void pixXLoop(const unsigned sPix, const unsigned ePix, const std::vector<float>& isectTbl);

#ifdef RUNTIME_SCAN_CONVERT_VERIFY
    // Compute the pixel coverage values using a brute-force method and perform verification.
    void verifyEdgeCoverage(const unsigned x, const float xSegmentStart, const float xSegmentEnd, const unsigned coverage);
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY

    inline int calcSubPixId(const float edgeX) const; // Calculate subpixel ID regarding edge's float position X

    // Update the pixel's coverage information based on the range of active subpixels within the specified pixel,
    // in cases where the active segment's start and end both lie inside a single pixel.
    inline void edgePix(const unsigned x, const unsigned sPix, const float xSegmentStart, const float xSegmentEnd);

    // Update the pixel's coverage information based on the range of active subpixels within the specified pixel,
    // in cases where only the start of the active segment lies inside a single pixel.
    inline void edgePixL(const unsigned x, const unsigned sPix, const float xSegmentStart);

    // Update the pixel's coverage information based on the range of active subpixels within the specified pixel,
    // in cases where only the end of the active segment lies inside a single pixel.
    inline void edgePixR(const unsigned x, const unsigned sPix, const float xSegmentEnd);
    
    // Write the pixel color and alpha values for the scanline according to the coverage.
    void scanlineFill(const unsigned y, const unsigned sPix, const unsigned ePix,
                      const C3& col, const unsigned char alpha);

    //------------------------------

    Overlay& mOverlay;

    unsigned mXSubSamples {0};
    unsigned mYSubSamples {0};
    float mInvYSubSamples {0.0f};
    float mInvSubSamples {0.0f};

    std::vector<std::vector<float>> mIsectTbl; // mIsectTbl[subY] : sorted
    std::vector<unsigned> mCoverageTbl;
};

inline int
ScanConvert::calcSubPixId(const float edgeX) const
//
// Calculate subpixel ID regarding the edge's float position X (= edgeX)
//
{
    float intPart;
    const float fraction =
        std::modf((edgeX - std::floor(edgeX)) * static_cast<float>(mXSubSamples), &intPart);
    return static_cast<int>(intPart + (fraction > 0.5f));
}

inline void
ScanConvert::edgePix(const unsigned x, const unsigned sPix, const float xSegmentStart, const float xSegmentEnd)
//
// Update the pixel's coverage information based on the range of active subpixels within the specified pixel,
// in cases where the active segment's start and end both lie inside a single pixel.
//
//   x : Pixel position
//   sPix : The leftmost (start) pixel of the current scanline
//   xSegmentStart : The left position of the active segment within this pixel
//   xSegmentEnd : The right position of the active segment within this pixel
//
{
    const int subPixIdStart = calcSubPixId(xSegmentStart);
    const int subPixIdEnd = calcSubPixId(xSegmentEnd) - 1;
    const unsigned coverage = subPixIdEnd - subPixIdStart + 1;
#ifdef RUNTIME_SCAN_CONVERT_VERIFY
    verifyEdgeCoverage(x, xSegmentStart, xSegmentEnd, coverage);
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY
    mCoverageTbl[x - sPix] += coverage;
}
    
inline void
ScanConvert::edgePixL(const unsigned x, const unsigned sPix, const float xSegmentStart)
//
// Update the pixel's coverage information based on the range of active subpixels within the specified pixel,
// in cases where only the start of the active segment lies inside a single pixel.
//
//   x : Pixel position
//   sPix : The leftmost (start) pixel of the current scanline
//   xSegmentStart : The left position of the active segment within this pixel
//
{
    const int subPixIdStart = calcSubPixId(xSegmentStart);
    const unsigned coverage = mXSubSamples - subPixIdStart;
#ifdef RUNTIME_SCAN_CONVERT_VERIFY
    verifyEdgeCoverage(x, xSegmentStart, -1.0f, coverage);
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY
    mCoverageTbl[x - sPix] += coverage;
}
    
inline void
ScanConvert::edgePixR(const unsigned x, const unsigned sPix, const float xSegmentEnd)
//
// Update the pixel's coverage information based on the range of active subpixels within the specified pixel,
// in cases where only the end of the active segment lies inside a single pixel.
//
//   x : Pixel position
//   sPix : The leftmost (start) pixel of the current scanline
//   xSegmentEnd : The right position of the active segment within this pixel
//
{
    const int subPixIdEnd = calcSubPixId(xSegmentEnd) - 1;
    const unsigned coverage = subPixIdEnd + 1;
#ifdef RUNTIME_SCAN_CONVERT_VERIFY
    verifyEdgeCoverage(x, -1.0f, xSegmentEnd, coverage);
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY
    mCoverageTbl[x - sPix] += coverage;
}

} // namespace telemetry
} // namespace mcrt_dataio
