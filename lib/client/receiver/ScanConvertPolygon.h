// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ScanConvert.h"

#include <scene_rdl2/common/math/Vec2.h>

namespace mcrt_dataio {
namespace telemetry {

class ScanConvertPolygonEdge
{
public:
    using Vec2f = scene_rdl2::math::Vec2f;

    ScanConvertPolygonEdge(const Vec2f& btm, const Vec2f& top)
        : mBtm {btm}
        , mTop {top}
        , mLeftX {(btm[0] < top[0]) ? btm[0] : top[0]}
    {}

    const Vec2f& mBtm;
    const Vec2f& mTop;
    const float mLeftX {0.0f};
};

class ScanConvertPolygon : public ScanConvert
{
public:
    using Vec2f = scene_rdl2::math::Vec2f;

    ScanConvertPolygon(Overlay& overlay,
                       const unsigned xSubSample, const unsigned ySubSample)
        : ScanConvert(overlay, xSubSample, ySubSample)
    {}

    void main(const std::vector<Vec2f>& polygon,
              const unsigned xSubSamples, const unsigned ySubSamples,
              const C3& c3, const unsigned char alpha);

private:    
    virtual bool calcMinMaxY(unsigned& min, unsigned& max) override;
    virtual bool xScan(const unsigned y, unsigned& sPIx, unsigned& ePix) override;

    void calcIsectTbl(std::vector<float>& isectTbl, const float scanlineY) const;

    //------------------------------

    const std::vector<Vec2f>* mPolygon {nullptr};
    std::vector<std::unique_ptr<ScanConvertPolygonEdge>> mEdgeTbl;
};

} // namespace telemetry
} // namespace mcrt_dataio
