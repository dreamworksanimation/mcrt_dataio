// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "ScanConvertPolygon.h"
#include "TelemetryOverlay.h"

namespace mcrt_dataio {
namespace telemetry {

void
ScanConvertPolygon::main(const std::vector<Vec2f>& polygon,
                         const unsigned xSubSamples,
                         const unsigned ySubSamples,
                         const C3& c3,
                         const unsigned char alpha)
{
    mPolygon = &polygon;

    mEdgeTbl.clear();
    for (size_t i = 0; i < polygon.size(); ++i) {
        const Vec2f& a = polygon[i];
        const Vec2f& b = polygon[(i + 1) % polygon.size()];
        if (a[1] < b[1]) mEdgeTbl.emplace_back(std::make_unique<ScanConvertPolygonEdge>(a, b));
        else             mEdgeTbl.emplace_back(std::make_unique<ScanConvertPolygonEdge>(b, a));
    }
    std::sort(mEdgeTbl.begin(), mEdgeTbl.end(),
              [](const std::unique_ptr<ScanConvertPolygonEdge>& a,
                 const std::unique_ptr<ScanConvertPolygonEdge>& b) {
                  return a->mLeftX < b->mLeftX;
              });

    scanConvert(xSubSamples, ySubSamples, c3, alpha);
}

bool
ScanConvertPolygon::calcMinMaxY(unsigned& min, unsigned& max)
{
    float currMin = (*mPolygon)[0][1];
    float currMax = currMin;
    for (const auto& p : (*mPolygon)) {
        currMin = std::min(currMin, p[1]);
        currMax = std::max(currMax, p[1]);
    }

    if (currMax <= 0.0f || currMin >= static_cast<float>(mOverlay.getHeight())) {
        return false; // outside window
    }

    min = static_cast<unsigned>(std::max(0, static_cast<int>(std::floor(currMin))));
    max = std::min(static_cast<unsigned>(mOverlay.getHeight()) - 1,
                   static_cast<unsigned>(std::floor(currMax)));
    return true;
}

bool
ScanConvertPolygon::xScan(const unsigned y,
                          unsigned& sPix,
                          unsigned& ePix)
{
    float xSegmentStart = static_cast<float>(mOverlay.getWidth());
    float xSegmentEnd = 0.0f;
    for (unsigned subY = 0; subY < mYSubSamples; ++subY) {
        const float scanlineY = static_cast<float>(y) + mInvYSubSamples * (static_cast<float>(subY) + 0.5f);
        std::vector<float>& isectTbl = mIsectTbl[subY];

        isectTbl.clear();
        calcIsectTbl(isectTbl, scanlineY);
        if (isectTbl.size() >= 2) {
            std::sort(isectTbl.begin(), isectTbl.end());
            xSegmentStart = std::min(xSegmentStart, isectTbl.front());
            xSegmentEnd = std::max(xSegmentEnd, isectTbl.back());
        }
    }
    if (xSegmentStart >= xSegmentEnd) return false; // no intersection -> skip this scanline.
    if (xSegmentStart >= static_cast<float>(mOverlay.getWidth()) || xSegmentEnd <= 0.0f) return false;

    sPix = static_cast<unsigned>(std::floor(std::max(0.0f, xSegmentStart)));
    ePix = static_cast<unsigned>(std::floor(std::min(static_cast<float>(mOverlay.getWidth() - 1), xSegmentEnd)));
    return true;
}

void
ScanConvertPolygon::calcIsectTbl(std::vector<float>& isectTbl, const float scanlineY) const
{
    for (size_t i = 0; i < (*mPolygon).size(); ++i) {
        const Vec2f& s = (*mPolygon)[i];
        const Vec2f& e = (*mPolygon)[(i + 1) % (*mPolygon).size()];
        if ((s[1] <= scanlineY && scanlineY < e[1]) || (e[1] <= scanlineY && scanlineY < s[1])) {
            isectTbl.push_back(s[0] + ((scanlineY - s[1]) / (e[1] - s[1])) * (e[0] - s[0]));
        }
    }
}

} // namespace telemetry
} // namespace mcrt_dataio
