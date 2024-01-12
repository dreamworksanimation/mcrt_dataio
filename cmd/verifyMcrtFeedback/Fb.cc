// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "Fb.h"

#include <scene_rdl2/common/fb_util/GammaF2C.h>
#include <scene_rdl2/common/fb_util/ReGammaC2F.h>

#include <fstream>
#include <iomanip>

namespace verifyFeedback {

Fb::Fb(const std::string& filename, bool isBeauty)
{
    std::string msgBuff;
    auto msgOut = [&](const std::string &msg) -> bool {
        if (!msgBuff.empty()) msgBuff += '\n';
        msgBuff += msg;
        return true;
    };

    bool error = false;
    if (isBeauty) {
        if (!readBeautyPPM(filename, msgOut)) error = true;
    } else {
        if (!readBeautyNumSamplePPM(filename, msgOut)) error = true;
    }

    if (error) throw std::runtime_error(msgBuff);
    if (!msgBuff.empty()) std::cerr << msgBuff << '\n';
}

void
Fb::resize(unsigned w, unsigned h)
{
    mWidth = w;
    mHeight = h;
    mFb.resize(w * h);
}

bool
Fb::isSame(const Fb& src) const
{
    if (mWidth != src.mWidth || mHeight != src.mHeight) return false;

    auto isSamePix = [](const Pix& pixA, const Pix& pixB) -> bool {
        constexpr float maxThresh = 0.05f / 255.0f;
        // constexpr float maxThresh = 0.01f / 255.0f; // too small tolerance.
        for (int i = 0; i < 3; ++i) {
            float delta = fabs(static_cast<float>(pixA[i] - pixB[i]));
            if (delta > maxThresh) return false;
        }
        return true;
    };

    return crawlAllPix([&](unsigned x, unsigned y) -> bool { return isSamePix(getPix(x, y), src.getPix(x, y)); });
}

bool
Fb::isZero() const
{
    return crawlAllPix([&](unsigned x, unsigned y) -> bool {
            return (getPix(x, y)[0] == 0.0f && getPix(x, y)[1] == 0.0f && getPix(x, y)[2] == 0.0f);
        });
}

void
Fb::add(const Fb& v)
{
    crawlAllPix([&](unsigned x, unsigned y) -> bool { addPix(x, y, v.getPix(x, y)); return true; });
}

void
Fb::sub(const Fb& v)
{
    crawlAllPix([&](unsigned x, unsigned y) -> bool { subPix(x, y, v.getPix(x, y)); return true; });
}

void
Fb::mul(const Fb& v)
{
    crawlAllPix([&](unsigned x, unsigned y) -> bool { mulPix(x, y, v.getPix(x, y)); return true; });
}

void
Fb::div(const Fb& v)
{
    crawlAllPix([&](unsigned x, unsigned y) -> bool { divPix(x, y, v.getPix(x, y)); return true; });
}

// static function
bool
Fb::merge(Fb& beautyOut, Fb& beautyNumSampleOut,
          const Fb& srcBeauty, const Fb& srcBeautyNumSample)
{
    if (!beautyOut.isSameSize(beautyNumSampleOut) ||
        !beautyOut.isSameSize(srcBeauty) || !beautyOut.isSameSize(srcBeautyNumSample)) {
        std::cerr << "merge fb size mismatch.\n"
                  << "  outBeauty(w:" << beautyOut.getWidth() << " h:" << beautyOut.getHeight() << ")\n"
                  << "  outBeautyNumSample(w:" << beautyNumSampleOut.getWidth() << " h:" << beautyNumSampleOut.getHeight() << ")\n"
                  << "  srcBeauty(w:" << srcBeauty.getWidth() << " h:" << srcBeauty.getHeight() << ")\n"
                  << "  srcBeautyNumSample(w:" << srcBeautyNumSample.getWidth() << " h:" << srcBeautyNumSample.getWidth() << ")\n";
        return false;
    }

    for (unsigned y = 0; y < srcBeauty.getHeight(); ++y) {
        for (unsigned x = 0; x < srcBeauty.getWidth(); ++x) {
            Fb::Pix& allC = beautyOut.getPix(x, y);
            Fb::Pix& allN = beautyNumSampleOut.getPix(x, y);
            const Fb::Pix& currC = srcBeauty.getPix(x, y);
            const Fb::Pix& currN = srcBeautyNumSample.getPix(x, y);

            float totalN = allN[0] + currN[0];
            if (totalN > 0.0f) {
                float scaleA = allN[0] / totalN;
                float scaleB = currN[0] / totalN;

                Fb::Pix newC;
                newC = allC * scaleA + currC * scaleB;

                allC = newC;
                allN = {totalN, totalN, totalN};
            } else {
                allC = {0.0f, 0.0f, 0.0f};
                allN = {0.0f, 0.0f, 0.0f};
            }
        }
    }
    return true;
}

void
Fb::abs()
{
    crawlAllPix([&](unsigned x, unsigned y) -> bool { absPix(x, y); return true; });
}

void
Fb::normalize()
{
    Pix scale = 1.0f / getMaxPix();
    crawlAllPix([&](unsigned x, unsigned y) -> bool {
            Pix& currPix = getPix(x, y);
            currPix = currPix * scale;
            return true;
        });
}

Fb::Pix
Fb::getMaxPix() const
{
    Pix max(0.0f, 0.0f, 0.0f);
    crawlAllPix([&](unsigned x, unsigned y) -> bool {
            const Pix& currPix = getPix(x, y);
            for (int i = 0; i < 3; ++i) {
                if (max[i] < currPix[i]) max[i] = currPix[i];
            }
            return true;
        });
    return max;
}

void
Fb::testFill()
{
    Pix lu(1.0f, 1.0f, 1.0f);
    Pix ld(0.0f, 0.0f, 0.0f);
    Pix ru(1.0f, 1.0f, 0.0f);
    Pix rd(0.0f, 0.0f, 1.0f);

    crawlAllPix([&](unsigned x, unsigned y) -> bool {
            float ratioY = static_cast<float>(y) / static_cast<float>(mHeight - 1);
            float ratioX = static_cast<float>(x) / static_cast<float>(mWidth - 1);
            float A = ratioX * ratioY;
            float B = (1.0f - ratioX) * ratioY;
            float C = ratioX * (1.0f - ratioY);
            float D = (1.0f - ratioX) * (1.0f - ratioY);
            setPix(x, y, lu * B + ld * D + ru * A + rd * C);
            return true;
        });
}

bool
Fb::readBeautyPPM(const std::string& filename, const MessageOutFunc& messageOutFunc)
{
    return loadPPMMain("beauty", filename,
                       [&](int x, int y, unsigned char c[3]) { // setPixFunc
                           setPix(x, y,
                                  Pix(c2552fReGamma22(c[0]), c2552fReGamma22(c[1]), c2552fReGamma22(c[2])));
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

bool
Fb::writeBeautyPPM(const std::string& filename, const MessageOutFunc& messageOutFunc) const
{
    return savePPMMain("beauty", filename,
                       [&](int x, int y, unsigned char c[3]) { // getPixFunc
                           Pix col = getPix(x, y);
                           c[0] = f2c255Gamma22(col[0]);
                           c[1] = f2c255Gamma22(col[1]);
                           c[2] = f2c255Gamma22(col[2]);
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

bool
Fb::readBeautyFBD(const std::string& filename, const MessageOutFunc& messageOutFunc)
{
    return loadFBDMain("beauty", filename,
                       [&](int x, int y, float c[3]) { // setPixFunc
                           setPix(x, y, Pix(c[0], c[1], c[2]));
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

bool
Fb::writeBeautyFBD(const std::string& filename, const MessageOutFunc& messageOutFunc) const
{
    return saveFBDMain("beauty", filename,
                       [&](int x, int y, float c[3]) { // getPixFunc
                           Pix col = getPix(x, y);
                           c[0] = col[0];
                           c[1] = col[1];
                           c[2] = col[2];
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

bool
Fb::readBeautyNumSamplePPM(const std::string& filename, const MessageOutFunc& messageOutFunc)
{
    return loadPPMMain("beautyNumSample", filename,
                       [&](int x, int y, unsigned char c[3]) { // setPixFunc
                           float v = static_cast<float>(c[0]);
                           setPix(x, y, Pix(v, v, v));
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

bool
Fb::writeBeautyNumSamplePPM(const std::string& filename, const MessageOutFunc& messageOutFunc) const
{
    unsigned int maxN = 0;
    for (unsigned y = 0; y < mHeight; ++y) {
        for (unsigned x = 0; x < mWidth; ++x) {
            unsigned n = static_cast<unsigned>(getPix(x, y)[0]);
            if (maxN < n) maxN = n;
        }
    }
    float scale = 255.0f / static_cast<float>(maxN);

    return savePPMMain("beautyNumSample", filename,
                       [&](int x, int y, unsigned char c[3]) { // getPixFunc
                           unsigned n = static_cast<unsigned>(getPix(x, y)[0]);
                           unsigned nn = static_cast<unsigned>(static_cast<float>(n) * scale);
                           c[0] = static_cast<unsigned char>(n); // original value
                           c[1] = static_cast<unsigned char>(nn); // normalized value
                           c[2] = 0;
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

bool
Fb::readBeautyNumSampleFBD(const std::string& filename, const MessageOutFunc& messageOutFunc)
{
    return loadFBDMain("beautyNumSample", filename,
                       [&](int x, int y, float c[3]) { // setPixFunc
                           float v = c[0];
                           setPix(x, y, Pix(v, v, v));
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

bool
Fb::writeBeautyNumSampleFBD(const std::string& filename, const MessageOutFunc& messageOutFunc) const
{
    unsigned int maxN = 0;
    for (unsigned y = 0; y < mHeight; ++y) {
        for (unsigned x = 0; x < mWidth; ++x) {
            unsigned n = static_cast<unsigned>(getPix(x, y)[0]);
            if (maxN < n) maxN = n;
        }
    }
    float scale = 1.0f / static_cast<float>(maxN);

    return saveFBDMain("beautyNumSample", filename,
                       [&](int x, int y, float c[3]) { // getPixFunc
                           unsigned n = static_cast<unsigned>(getPix(x, y)[0]);
                           float normalized = static_cast<float>(n) * scale;
                           c[0] = static_cast<float>(n); // original value
                           c[1] = normalized; // normalized value
                           c[2] = 0.0f;
                       },
                       [&](const std::string& msg) -> bool { // msgOutFunc
                           return messageOutFunc(msg);
                       });
}

//------------------------------------------------------------------------------------------

template <typename PixFunc>
bool Fb::crawlAllPix(PixFunc pixFunc) const
{
    for (unsigned y = 0; y < mHeight; ++y) {
        for (unsigned x = 0; x < mWidth; ++x) {
            if (!pixFunc(x, y)) return false;
        }
    }
    return true;
}

template <typename GetPixFunc, typename MsgOutFunc>
bool
Fb::savePPMMain(const std::string& msg,
                const std::string& filename,
                GetPixFunc getPixFunc,
                MsgOutFunc msgOutFunc) const
{
    if (!msg.empty()) {
        if (!msgOutFunc("save " + msg + " filename:" + filename)) return false;
    }

    std::ofstream ofs(filename);
    if (!ofs) {
        msgOutFunc("create open filed. filename:" + filename);
        return false;
    }

    constexpr int valReso = 256;

    {
        std::ostringstream ostr;
        ostr << "w:" << mWidth << " h:" << mHeight;
        if (!msgOutFunc(ostr.str())) return false;
    }
    
    ofs << "P3\n" << mWidth << ' ' << mHeight << '\n'
        << (valReso - 1) << '\n';
    for (int v = static_cast<int>(mHeight) - 1; v >= 0; --v) {
        for (int u = 0; u < static_cast<int>(mWidth); ++u) {
            unsigned char c[3];
            getPixFunc(u, v, c);
            ofs << static_cast<int>(c[0]) << ' '
                << static_cast<int>(c[1]) << ' '
                << static_cast<int>(c[2]) << ' ';
        }
    }

    ofs.close();

    if (!msgOutFunc("done")) return false;

    return true;
}

template <typename GetPixFunc, typename MsgOutFunc>
bool
Fb::saveFBDMain(const std::string& msg,
                const std::string& filename,
                GetPixFunc getPixFunc,
                MsgOutFunc msgOutFunc) const
{
    auto toHexF = [](const float f) -> std::string {
        union {
            float f;
            unsigned char uc[4];
        } uni;
        uni.f = f;
        int a = static_cast<int>(uni.uc[0]);
        int b = static_cast<int>(uni.uc[1]);
        int c = static_cast<int>(uni.uc[2]);
        int d = static_cast<int>(uni.uc[3]);

        std::ostringstream ostr;
        ostr
        << std::hex << std::setw(2) << std::setfill('0') << a
        << std::hex << std::setw(2) << std::setfill('0') << b
        << std::hex << std::setw(2) << std::setfill('0') << c
        << std::hex << std::setw(2) << std::setfill('0') << d;
        return ostr.str();
    };

    if (!msg.empty()) {
        if (!msgOutFunc("save " + msg + " filename:" + filename)) return false;
    }

    std::ofstream ofs(filename);
    if (!ofs) {
        msgOutFunc("create open filed. filename:" + filename);
        return false;
    }

    {
        std::ostringstream ostr;
        ostr << "w:" << mWidth << " h:" << mHeight;
        if (!msgOutFunc(ostr.str())) return false;
    }
    
    ofs << "FbDump\n" << mWidth << ' ' << mHeight << '\n';
    for (int v = static_cast<int>(mHeight) - 1; v >= 0; --v) {
        for (int u = 0; u < static_cast<int>(mWidth); ++u) {
            float c[3];
            getPixFunc(u, v, c);
            ofs << toHexF(c[0]) << ' ' << toHexF(c[1]) << ' ' << toHexF(c[2]) << ' ';
        }
    }

    ofs.close();

    if (!msgOutFunc("done")) return false;

    return true;
}

template <typename SetPixFunc, typename MsgOutFunc>
bool
Fb::loadPPMMain(const std::string& msg,
                const std::string& filename,
                SetPixFunc setPixFunc,
                MsgOutFunc msgOutFunc)
{
    if (!msg.empty()) {
        if (!msgOutFunc("load " + msg + " filename:" + filename)) return false;
    }

    std::ifstream ifs(filename);
    if (!ifs) {
        msgOutFunc("read open filed. filename:" + filename);
        return false;
    }

    std::string line;
    {
        getline(ifs, line);
        if (line != "P3") {
            msgOutFunc("not support format. " + line);
            return false;
        }
    }
    {
        getline(ifs, line);
        std::istringstream istr(line);
        unsigned width, height;
        istr >> width >> height;
        /* useful debug message
        std::ostringstream ostr;
        ostr << "w:" << width << " h:" << height;
        msgOutFunc(ostr.str());
        */
        resize(width, height);
    }
    unsigned colReso;
    {
        getline(ifs, line);
        colReso = std::stoi(line);
        if (colReso != 255) {
            msgOutFunc("not supported color reso. " + line);
            return false;
        }
    }

    for (int v = static_cast<int>(mHeight) - 1; v >= 0; --v) {
        for (int u = 0; u < static_cast<int>(mWidth); ++u) {
            int r, g, b;
            ifs >> r >> g >> b;
            unsigned char c[3] = {static_cast<unsigned char>(r),
                                  static_cast<unsigned char>(g),
                                  static_cast<unsigned char>(b)};
            setPixFunc(u, v, c);
        }
    }

    ifs.close();

    if (!msgOutFunc("done")) return false;

    return true;
}

template <typename SetPixFunc, typename MsgOutFunc>
bool
Fb::loadFBDMain(const std::string& msg,
                const std::string& filename,
                SetPixFunc setPixFunc,
                MsgOutFunc msgOutFunc)
{
    if (!msg.empty()) {
        if (!msgOutFunc("load " + msg + " filename:" + filename)) return false;
    }

    std::ifstream ifs(filename);
    if (!ifs) {
        msgOutFunc("read open filed. filename:" + filename);
        return false;
    }

    std::string line;
    {
        getline(ifs, line);
        if (line != "FbDump") {
            msgOutFunc("not support format. " + line);
            return false;
        }
    }
    {
        getline(ifs, line);
        std::istringstream istr(line);
        unsigned width, height;
        istr >> width >> height;
        /* useful debug message
        std::ostringstream ostr;
        ostr << "w:" << width << " h:" << height;
        msgOutFunc(ostr.str());
        */
        resize(width, height);
    }

    auto hexFloat2f = [](const std::string& hexFloat) -> float {
        auto hex2uc = [](char hexA, char hexB) -> unsigned char {
            char buff[3] = {hexA, hexB, 0x0};
            // buff[0] = hexA; buff[1] = hexB; buff[2] = 0x0;
            return static_cast<unsigned char>(std::strtol(buff, nullptr, 16));
        };
        union {
            float f;
            unsigned char uc[4];
        } uni;
        uni.uc[0] = hex2uc(hexFloat[0], hexFloat[1]);
        uni.uc[1] = hex2uc(hexFloat[2], hexFloat[3]);
        uni.uc[2] = hex2uc(hexFloat[4], hexFloat[5]);
        uni.uc[3] = hex2uc(hexFloat[6], hexFloat[7]);
        return uni.f;
    };

    for (int v = static_cast<int>(mHeight) - 1; v >= 0; --v) {
        for (int u = 0; u < static_cast<int>(mWidth); ++u) {
            std::string r, g, b;
            ifs >> r >> g >> b;
            float c[3] = {hexFloat2f(r), hexFloat2f(g), hexFloat2f(b)};
            setPixFunc(u, v, c);
        }
    }

    ifs.close();

    // if (!msgOutFunc("done")) return false; // useful debug message

    return true;
}

uint8_t Fb::f2c255Gamma22(const float f) const
{
    return (f <= 0.0f) ? 0 : scene_rdl2::fb_util::GammaF2C::g22(f);
}

float Fb::c2552fReGamma22(const uint8_t uc) const
{
    return scene_rdl2::fb_util::ReGammaC2F::rg22(uc);
}

} // namespace verifyFeedback
