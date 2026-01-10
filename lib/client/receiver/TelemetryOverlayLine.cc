// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "TelemetryOverlay.h"

namespace mcrt_dataio {
namespace telemetry {

void
Overlay::setLineDrawFunc(const LineDrawType type)
{
    mLineDrawType = type;

    switch (mLineDrawType) {
    case LineDrawType::BRESENHAM : mLineDrawFunc = &Overlay::lineDrawBresenham; break;
    case LineDrawType::DDA : mLineDrawFunc = &Overlay::lineDrawDDA; break;
    case LineDrawType::WU : mLineDrawFunc = &Overlay::lineDrawWu; break;
    case LineDrawType::WU_HQ : mLineDrawFunc = &Overlay::lineDrawWuHQ; break;
    case LineDrawType::OUTLINE_STROKE : mLineDrawFunc = &Overlay::lineDrawOutlineStroke; break;
    case LineDrawType::OUTLINE_STROKE2 : mLineDrawFunc = &Overlay::lineDrawOutlineStroke2; break;
    case LineDrawType::HYBRID :
    default :
        mLineDrawFunc = &Overlay::lineDrawHybrid; break;
    }
}

void
Overlay::lineDrawBresenham(const Vec2f& s,
                           const Vec2f& e,
                           const float width, // not used
                           const C3& c,
                           const unsigned char alpha)
{
    Vec2i si(static_cast<int>(s[0]), static_cast<int>(s[1]));
    const Vec2i ei(static_cast<int>(e[0]), static_cast<int>(e[1]));
    const Vec2i delta = abs(ei - si);
    const Vec2i dir((si[0] < ei[0]) ? 1 : -1, (si[1] < ei[1]) ? 1 : -1);

    int err = delta[0] - delta[1];
    while (true) {
        if (isPixInside(si[0], si[1])) pixFill(si[0], si[1], c, alpha);
        if (si == ei) break;

        int e2 = 2 * err;
        if (e2 > -delta[1]) {
            err -= delta[1];
            si[0] += dir[0];
        }
        if (e2 < delta[0]) {
            err += delta[0];
            si[1] += dir[1];
        }
    }
}

void
Overlay::lineDrawDDA(const Vec2f& s,
                     const Vec2f& e,
                     const float width, // not used
                     const C3& c,
                     const unsigned char alpha)
{
    const Vec2i si(static_cast<int>(s[0]), static_cast<int>(s[1]));
    const Vec2i ei(static_cast<int>(e[0]), static_cast<int>(e[1]));
    const Vec2i delta = ei - si;
    const int steps = std::max(std::abs(delta[0]), std::abs(delta[1]));
    const Vec2f posInc(static_cast<float>(delta[0]) / static_cast<float>(steps),
                       static_cast<float>(delta[1]) / static_cast<float>(steps));
    Vec2f pos(s);
    for (int i = 0; i <= steps; ++i) {
        unsigned sx = static_cast<unsigned>(pos[0] + 0.5f);
        unsigned sy = static_cast<unsigned>(pos[1] + 0.5f);
        if (isPixInside(sx, sy)) pixFill(sx, sy, c, alpha);
        pos += posInc;
    }
}

void
Overlay::lineDrawWu(const Vec2f& s,
                    const Vec2f& e,
                    const float width,
                    const C3& c3,
                    const unsigned char alpha)
{
    const float halfWidth = width * 0.5f;

    const Vec2f delta = e - s;
    const float length = std::hypot(delta[0], delta[1]);
    if (length == 0.0f) {
        lineDrawWuCap(s, halfWidth, c3, alpha);
        return;
    }

    lineDrawWuBody(s, e, halfWidth, c3, alpha);

    // No start/end CAP
}

void
Overlay::lineDrawWuBody(const Vec2f& s,
                        const Vec2f& e,
                        const float halfWidth,
                        const C3& c3,
                        const unsigned char alpha)
{
    const int inX0 = static_cast<int>(s[0] + 0.5f);
    const int inY0 = static_cast<int>(s[1] + 0.5f);
    const int inX1 = static_cast<int>(e[0] + 0.5f);
    const int inY1 = static_cast<int>(e[1] + 0.5f);

    const int dx = abs(inX1 - inX0);          // change in x
    const int dy = abs(inY1 - inY0);          // change in y

    if (dx == 0 && dy == 0) { return; }
    
    int sx = inX0 < inX1 ? 1 : -1;      // step in the x-direction
    int sy = inY0 < inY1 ? 1 : -1;      // step in the y-direction

    bool steep = false;                 // is the slope > 1 ?
    int x0 = inX0;                      // starting point x
    int x1 = inX1;                      // ending point x
    int y0 = inY0;                      // starting point y
    int y1 = inY1;                      // ending point y
    double slope;                       // slope/gradient of line  

    // We always want to be drawing a line with
    // a gradual slope, so if it's a steep line, swap the x and y
    // coordinates so we can draw a line with slope < 1
    if (dy > dx) {
        steep = true;
        slope = (float) dx / dy;
        std::swap(x0, y0);
        std::swap(x1, y1);
        std::swap(sx, sy);
    } else {
        slope = (double) dy / dx; 
    }

    // Create a function to write to the buffer
    auto write = [&] (int x, int y, const float a) {
        // if slope is steep, must swap x and y back before writing
        if (steep) { 
            std::swap(x, y); 
        }

        if (x < 0 || getWidth() <= x || y < 0 || getHeight() <= y) return;

        if (a <= 0.0f) return;
        float currAlpha = static_cast<unsigned char>(std::clamp(static_cast<float>(alpha) * a, 0.0f, 255.0f));
        pixMaxFill(x, y, c3, currAlpha);
    };
    
    // The integer part of half the line width
    const int lineWidth = static_cast<int>(halfWidth * 2.0f + 0.5f);
    const int halfWidthInt = lineWidth * 0.5;

    double yIntersect = y0;
    // This is a gradually increasing line, so
    // we always increase in x, and conditionally increase in y
    for (int x = x0; x != x1; x += sx) {

        // find the integer part of yIntersect
        // which is the new y coordinate
        const int yIntersectInt = (int) yIntersect;

        // find the fractional part of yIntersect
        // which represents how far off the line we are,
        // and helps us calculate the alpha value
        const double yIntersectFract = yIntersect - yIntersectInt;

        // Create width by extending on either side in the y-direction
        const int minY = yIntersectInt - halfWidthInt;
        const int maxY = yIntersectInt + (lineWidth - halfWidthInt);
        for (int y = minY; y <= maxY; ++y) {
            if (y == minY) {
                // lowest, anti-aliased pixel
                write(x, y, 1.f - yIntersectFract);
            } else if (y == maxY) {
                // highest, anti-aliased pixel
                write(x, y, yIntersectFract);
            } else {
                // any middle pixels don't factor in AA
                write(x, y, 1.f);
            }
        }   
        yIntersect += slope * sy;
    }
}

void
Overlay::lineDrawWuHQ(const Vec2f& s,
                      const Vec2f& e,
                      const float width,
                      const C3& c3,
                      const unsigned char alpha)
{
    const float halfWidth = width * 0.5f;

    const Vec2f delta = e - s;
    const float length = std::hypot(delta[0], delta[1]);
    if (length == 0.0f) {
        lineDrawWuCap(s, halfWidth, c3, alpha);
        return;
    }

    const Vec2f unit = delta / length;
    const Vec2f offset = unit * halfWidth;
    const Vec2f sB = s + offset;
    const Vec2f eB = e - offset;
    lineDrawWuHQBody(sB, eB, halfWidth, c3, alpha);

    if (width > 1.0f) {
        lineDrawWuCap(s, halfWidth, c3, alpha);
        lineDrawWuCap(e, halfWidth, c3, alpha);
    }
}

void
Overlay::lineDrawWuHQBody(const Vec2f& s,
                          const Vec2f& e,
                          const float halfWidth,
                          const C3& c3,
                          const unsigned char alpha)
{
    const Vec2f delta = e - s;
    const float length = std::hypot(delta[0], delta[1]);
    if (length == 0.0f) {
        return;
    }
    const Vec2f unit = delta / length;
    const Vec2f nv(-unit[1], unit[0]); 

    const int steps = static_cast<int>(std::ceil(length));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const Vec2f center = s + t * delta;

        const int ix0 = static_cast<int>(std::floor(center[0] - halfWidth));
        const int ix1 = static_cast<int>(std::ceil (center[0] + halfWidth));
        const int iy0 = static_cast<int>(std::floor(center[1] - halfWidth));
        const int iy1 = static_cast<int>(std::ceil (center[1] + halfWidth));
        for (int iy = iy0; iy <= iy1; ++iy) {
            if (iy < 0 || iy >= getHeight()) continue;
            for (int ix = ix0; ix <= ix1; ++ix) {
                if (ix < 0 || ix >= getWidth()) continue;
                
                const Vec2f deltaPos = Vec2f(static_cast<float>(ix) + 0.5f, static_cast<float>(iy) + 0.5f) - center;
                const float distance = std::abs(dot(deltaPos, nv));
                if (distance <= halfWidth + 1.0f) {
                    const float coverage = std::clamp(1.0f - (distance - halfWidth), 0.0f, 1.0f);
                    if (coverage <= 0.0f) continue;
                    const float currAlpha =
                        static_cast<unsigned char>(std::clamp(static_cast<float>(alpha) * coverage, 0.0f, 255.0f));
                    pixMaxFill(ix, iy, c3, currAlpha);
                }
            }
        }
    }
}

void
Overlay::lineDrawWuCap(const Vec2f& p,
                       const float halfWidth,
                       const C3& c3,
                       const unsigned char alpha)
{
    const int ix0 = static_cast<int>(std::floor(p[0] - halfWidth));
    const int ix1 = static_cast<int>(std::ceil (p[0] + halfWidth));
    const int iy0 = static_cast<int>(std::floor(p[1] - halfWidth));
    const int iy1 = static_cast<int>(std::ceil (p[1] + halfWidth));
    for (int iy = iy0; iy <= iy1; ++iy) {
        if (iy < 0 || iy >= getHeight()) continue;
        for (int ix = ix0; ix <= ix1; ++ix) {
            if (ix < 0 || ix >= getWidth()) continue;

            const Vec2f deltaPos = Vec2f(static_cast<float>(ix) + 0.5f, static_cast<float>(iy) + 0.5f) - p;
            const float distance = std::hypot(deltaPos[0], deltaPos[1]);
            if (distance <= halfWidth + 1.0f) {
                const float coverage = std::clamp(1.0f - (distance - halfWidth), 0.0f, 1.0f);
                if (coverage <= 0.0f) continue;
                const float currAlpha =
                    static_cast<unsigned char>(std::clamp(static_cast<float>(alpha) * coverage, 0.0f, 255.0f));
                pixMaxFill(ix, iy, c3, currAlpha);
            }
        }
    }
}

void
Overlay::lineDrawOutlineStrokeMain(const Vec2f& s,
                                   const Vec2f& e,
                                   const float width,
                                   const int mode,
                                   const C3& c3,
                                   const unsigned char alpha)
{
    if (width <= 0.0f) return;

    const float halfWidth = width * 0.5f;

    const Vec2f delta = e - s;
    const float length = std::hypot(delta[0], delta[1]);
    if (length == 0.0f) {
        if (width > 1.0f) circleFill(s, halfWidth, c3, alpha);
        return;
    }

    const Vec2f unit = delta / length;
    const Vec2f nv(-unit[1], unit[0]);

    const Vec2f offset = nv * halfWidth;
    const Vec2f sA = s + offset;
    const Vec2f sB = s - offset;
    const Vec2f eA = e + offset;
    const Vec2f eB = e - offset;

    const std::vector<Vec2f> polygon = {sA, eA, eB, sB};

    //
    // adaptive subSample control
    //
    constexpr float maxSamples = 6.0f; // heuristically defined

    const float ax = std::abs(delta[0]);
    const float ay = std::abs(delta[1]);
    const float totalDelta = ax + ay;
    const unsigned ySubSamples = static_cast<unsigned>(std::max(1.0f, maxSamples * ax / totalDelta));
    const unsigned xSubSamples = static_cast<unsigned>(std::max(1.0f, maxSamples * ay / totalDelta));

    if (mode == 0) {
        polygonFill(polygon, xSubSamples, ySubSamples, c3, alpha);
    } else if (mode == 1) {
        // For the performance comparison purposes
        // This is a naive class inheritance type implementation of scan conversion.
        // Compared with the lambda version, the class inheritance version is always slow.
        // So we just use the lambda version. But keep the class inheritance version for
        // performance comparison purposes.
        mScanConvertPolygon->main(polygon, xSubSamples, ySubSamples, c3, alpha);
    }

    if (width > 1.0f) {
        circleFill(s, halfWidth, c3, alpha);
        circleFill(e, halfWidth, c3, alpha);
    } 
}

} // namespace telemetry
} // namespace mcrt_dataio
