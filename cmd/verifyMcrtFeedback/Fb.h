// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/common/math/Vec3.h>

#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace verifyFeedback {

class Fb
{
public:
    using Pix = scene_rdl2::math::Vec3f;
    using MessageOutFunc = std::function<bool(const std::string& msg)>;

    Fb() = default;
    Fb(unsigned w, unsigned h)
        : mWidth(w)
        , mHeight(h)
        , mFb(w * h)
    {}
    explicit Fb(const std::string& filename, bool isBeauty = true); // throw std::runtime_error if error

    void resize(unsigned w, unsigned h);
    unsigned getWidth() const { return mWidth; }
    unsigned getHeight() const { return mHeight; }
    bool isSameSize(const Fb& src) const
    {
        return (getWidth() == src.getWidth() && getHeight() == src.getHeight());
    }

    Pix& getPix(unsigned x, unsigned y) { return mFb[pixOffset(x, y)]; }
    const Pix& getPix(unsigned x, unsigned y) const { return mFb[pixOffset(x, y)]; }

    void clear() { for (auto& pix : mFb) { pix[0] = 0.0f; pix[1] = 0.0f; pix[2] = 0.0f; } }

    bool isSame(const Fb& src) const;
    bool isZero() const; // return true if all pix is zero

    void add(const Fb& v); // this += src
    void sub(const Fb& v); // this -= src
    void mul(const Fb& v); // this *= src
    void div(const Fb& v); // this /= src

    static bool merge(Fb& beautyOut, Fb& beautyNumSampleOut,
                      const Fb& currBeauty, const Fb& currBeautyNumSample);

    void abs();
    void normalize();
    Pix getMaxPix() const;

    void testFill();

    bool readBeautyPPM(const std::string& filename, const MessageOutFunc& msgOutFunc);
    bool writeBeautyPPM(const std::string& filename, const MessageOutFunc& msgOutFunc) const;
    bool readBeautyFBD(const std::string& filename, const MessageOutFunc& msgOutFunc);
    bool writeBeautyFBD(const std::string& filename, const MessageOutFunc& msgOutFunc) const;
    bool readBeautyNumSamplePPM(const std::string& filename, const MessageOutFunc& msgOutFunc);
    bool writeBeautyNumSamplePPM(const std::string& filename, const MessageOutFunc& msgOutFunc) const;
    bool readBeautyNumSampleFBD(const std::string& filename, const MessageOutFunc& msgOutFunc);
    bool writeBeautyNumSampleFBD(const std::string& filename, const MessageOutFunc& msgOutFunc) const;

private:

    template <typename PixFunc> bool crawlAllPix(PixFunc pixFunc) const;

    unsigned pixOffset(unsigned x, unsigned y) const { return (y * mWidth + x); }

    bool isZeroPix(Pix& p) { return (p[0] == 0.0f && p[1] == 0.0f && p[2] == 0.0f); }

    void setPix(unsigned x, unsigned y, Pix col) { mFb[pixOffset(x, y)] = col; }
    void addPix(unsigned x, unsigned y, Pix col) { mFb[pixOffset(x, y)] += col; }
    void subPix(unsigned x, unsigned y, Pix col) { mFb[pixOffset(x, y)] -= col; }
    void mulPix(unsigned x, unsigned y, Pix col) { mFb[pixOffset(x, y)] = mFb[pixOffset(x, y)] * col; }
    void divPix(unsigned x, unsigned y, Pix col)
    {
        Pix& currPix = mFb[pixOffset(x, y)];
        if (!isZeroPix(currPix)) currPix = currPix / col;
    }
    void absPix(unsigned x, unsigned y)
    {
        Pix& currPix = mFb[pixOffset(x, y)];
        currPix[0] = static_cast<float>(std::abs(static_cast<float>(currPix[0])));
        currPix[1] = static_cast<float>(std::abs(static_cast<float>(currPix[1])));
        currPix[2] = static_cast<float>(std::abs(static_cast<float>(currPix[2])));
    }

    template <typename GetPixFunc, typename MsgOutFunc>
    bool savePPMMain(const std::string& msg,
                     const std::string& filename,
                     GetPixFunc getPixFunc,
                     MsgOutFunc msgOutFunc) const;
    template <typename GetPixFunc, typename MsgOutFunc>
    bool saveFBDMain(const std::string& msg,
                     const std::string& filename,
                     GetPixFunc getPixFunc,
                     MsgOutFunc msgOutFunc) const;
    template <typename SetPixFunc, typename MsgOutFunc>
    bool loadPPMMain(const std::string& msg,
                     const std::string& filename,
                     SetPixFunc setPixFunc,
                     MsgOutFunc msgOutFunc);
    template <typename SetPixFunc, typename MsgOutFunc>
    bool loadFBDMain(const std::string& msg,
                     const std::string& filename,
                     SetPixFunc setPixFunc,
                     MsgOutFunc msgOutFunc);

    uint8_t f2c255Gamma22(const float f) const;
    float c2552fReGamma22(const uint8_t uc) const;

    //------------------------------

    unsigned mWidth {0};
    unsigned mHeight {0};
    std::vector<scene_rdl2::math::Vec3f> mFb;
};

inline bool operator ==(const Fb& a, const Fb& b) { return a.isSame(b); }
inline bool operator !=(const Fb& a, const Fb& b) { return !a.isSame(b); }

inline Fb operator +(const Fb& a, const Fb& b) { Fb c = a; c.add(b); return c; }
inline Fb operator -(const Fb& a, const Fb& b) { Fb c = a; c.sub(b); return c; }
inline Fb operator *(const Fb& a, const Fb& b) { Fb c = a; c.mul(b); return c; }
inline Fb operator /(const Fb& a, const Fb& b) { Fb c = a; c.div(b); return c; }

inline Fb operator +=(Fb& a, const Fb& b) { a.add(b); return a; }

} // namespace verifyFeedback
