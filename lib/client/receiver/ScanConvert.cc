// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "ScanConvert.h"
#include "TelemetryOverlay.h"

//#define RUNTIME_SCAN_CONVERT_VERIFY

namespace mcrt_dataio {
namespace telemetry {

void
ScanConvert::scanConvert(const unsigned xSubSamples,
                         const unsigned ySubSamples,
                         const C3& c3,
                         const unsigned char alpha)
{
    mXSubSamples = xSubSamples;
    mYSubSamples = ySubSamples;
    mInvYSubSamples = 1.0f / static_cast<float>(mYSubSamples);
    mInvSubSamples = 1.0f / static_cast<float>(mXSubSamples * mYSubSamples);
    if (mIsectTbl.size() < mYSubSamples) {
        mIsectTbl.resize(mYSubSamples);
    }

#ifdef SC_TIMELOG
    mOverlay.getScTimeLog().start();
#endif // end of SC_TIMELOG

    unsigned minY, maxY;
    if (!calcMinMaxY(minY, maxY)) {
#ifdef SC_TIMELOG
        mOverlay.getScTimeLog().endCalcMinMaxY();
#endif // end of SC_TIMELOG
        return;  // outside window
    }
#ifdef SC_TIMELOG
    mOverlay.getScTimeLog().endCalcMinMaxY();
#endif // end of SC_TIMELOG

    for (unsigned y = minY ; y <= maxY ; ++y) {
        unsigned sPix, ePix;
        if (!xScan(y, sPix, ePix)) {
#ifdef SC_TIMELOG
            mOverlay.getScTimeLog().endXScan();
#endif // end of SC_TIMELOG
            continue;
        }
#ifdef SC_TIMELOG
        mOverlay.getScTimeLog().endXScan();
#endif // end of SC_TIMELOG

        setupCoverageTbl(sPix, ePix);
#ifdef SC_TIMELOG
        mOverlay.getScTimeLog().endSetupCoverageTbl();
#endif // end of SC_TIMELOG

        calcCoverage(sPix, ePix);
#ifdef SC_TIMELOG
        mOverlay.getScTimeLog().endCalcCoverage();
#endif // end of SC_TIMELOG

        scanlineFill(y, sPix, ePix, c3, alpha);
#ifdef SC_TIMELOG
        mOverlay.getScTimeLog().endScanlineFill();
#endif // end of SC_TIMELOG
    }
}

void
ScanConvert::setupCoverageTbl(const unsigned sPix, const unsigned ePix)
//
// Allocate the required amount of memory for the pixel coverage table.
//
{
    const size_t tblSize = ePix - sPix + 1;
    if (mCoverageTbl.size() < tblSize) mCoverageTbl.resize(tblSize);
    std::memset(mCoverageTbl.data(), 0, mCoverageTbl.size() * sizeof(unsigned));
}

void
ScanConvert::calcCoverage(const unsigned sPix, const unsigned ePix)
//
// Process all sub-scanlines within a single scanline.
//
{
    for (unsigned subY = 0; subY < mYSubSamples; ++subY) { 
        if (mIsectTbl[subY].size() < 2) continue;
        pixXLoop(sPix, ePix, mIsectTbl[subY]);
    }
}

void
ScanConvert::pixXLoop(const unsigned sPix,
                      const unsigned ePix,
                      const std::vector<float>& isectTbl)
//
// Update the coverage information for a single sub-scanline based on all active edge segments.
//
{
    for (size_t i = 0; i < isectTbl.size(); i += 2) {
        const float xSegmentStart = isectTbl[i];
        const float xSegmentEnd = isectTbl[i + 1];
        const unsigned s = static_cast<unsigned>(std::floor(std::max(0.0f, xSegmentStart)));
        const unsigned e = std::min(ePix, static_cast<unsigned>(std::floor(xSegmentEnd)));
        if (e < s) continue;
        if (s == e) {
            edgePix(s, sPix, xSegmentStart, xSegmentEnd);
        } else if (s + 1 == e) {
            edgePixL(s, sPix, xSegmentStart);
            edgePixR(e, sPix, xSegmentEnd);            
        } else {
            edgePixL(s, sPix, xSegmentStart);
            const unsigned currMax = std::min(e - 1, ePix);
            for (unsigned x = s + 1; x <= currMax; ++x) {
                mCoverageTbl[x - sPix] += mXSubSamples; 
            }
            edgePixR(e, sPix, xSegmentEnd);
        }
    }
}

#ifdef RUNTIME_SCAN_CONVERT_VERIFY
void
ScanConvert::verifyEdgeCoverage(const unsigned x,
                                const float xSegmentStart,
                                const float xSegmentEnd,
                                const unsigned coverage)
//
// Compute the pixel coverage values using a brute-force method and perform verification.
//
{
    unsigned verifyCoverage = 0;
    for (unsigned subPix = 0; subPix < mXSubSamples; ++subPix) {
        const float xPos =
            static_cast<float>(x) + (static_cast<float>(subPix) + 0.5f) / static_cast<float>(mXSubSamples);
        if (xSegmentStart >= 0.0f && xSegmentEnd >= 0.0f) {
            if (xSegmentStart <= xPos && xPos < xSegmentEnd) verifyCoverage++;
        } else if (xSegmentStart >= 0.0f && xSegmentEnd < 0.0f) {
            if (xSegmentStart <= xPos) verifyCoverage++;
        } else if (xSegmentStart < 0.0f && xSegmentEnd >= 0.0f) {
            if (xPos < xSegmentEnd) verifyCoverage++;
        }
    }
    if (verifyCoverage != coverage) {
        std::cerr << ">> TelemetryOverlay.cc edgePix verify-ERROR\n";
    }
}
#endif // end of RUNTIME_SCAN_CONVERT_VERIFY

void
ScanConvert::scanlineFill(const unsigned y,
                          const unsigned sPix, const unsigned ePix,
                          const C3& col,
                          const unsigned char alpha)
//
// Write the pixel color and alpha values for the scanline according to the coverage.
//
{
    for (unsigned x = sPix ; x <= ePix ; ++x) {
        const float coverage = mCoverageTbl[x - sPix] * mInvSubSamples;
        if (coverage > 0.0f) {
            const float currAlpha =
                static_cast<unsigned char>(std::clamp(static_cast<float>(alpha) * coverage, 0.0f, 255.0f));
            mOverlay.pixMaxFill(x, y, col, currAlpha);
        }
    }
}

} // namespace telemetry
} // namespace mcrt_dataio
