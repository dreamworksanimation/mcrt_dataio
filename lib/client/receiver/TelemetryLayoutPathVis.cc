// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "TelemetryDisplay.h"
#include "TelemetryLayout.h"

#include "VectorPacketManager.h"

#include <scene_rdl2/common/rec_time/RecTime.h>
#include <scene_rdl2/render/util/StrUtil.h>

namespace mcrt_dataio {
namespace telemetry {

void
LayoutPathVis::drawMain(const DisplayInfo& info)
{
    if (!mLineDrawOnly) {
        subPanelTitle(info);
        subPanelGlobalInfo(10, mBBoxTitle.lower.y - 10 - mStepPixY, info, mBBoxGlobalInfo);
        drawGlobalProgressBar(info);
    }

    if (!mTestMode) {
        drawVector(info);
        if (!mLineDrawOnly) {
            drawPathVisCtrlInfo(info);
            drawPathVisClientInfo();
            drawPathVisCurrInfo(info);
            drawPathVisHotKeyHelp();
        }
    } else {
        //
        // Test mode
        //
        drawVectorProfile([&]() {
            switch (mTestType) {
            case 0 : drawVectorTest0(); break; // drawVectorTestPatternA() w=1
            case 1 : drawVectorTest1(); break; // drawVectorTestPatternA() w=0~16
            case 2 : drawVectorTest2(); break; // drawVectorTestPatternA() w=0~16, len=change
            case 3 : drawVectorTest3(); break; // clipping version of 1
            case 4 : drawVectorTest4(); break; // clipping version of 2
            case 5 : drawVectorTest5(); break; // circleFill test
            case 6 : drawVectorTest6(); break; // vLine/hLine/boxOutline test
            default : break;
            }
        });
    }
}

std::string
LayoutPathVis::show() const
{
    std::ostringstream ostr;
    ostr << "LayoutPathVis {\n"
         << "  mTestMode:" << scene_rdl2::str_util::boolStr(mTestMode) << '\n'
         << "  mTestType:" << mTestType << '\n'
         << "  mTestCounter:" << mTestCounter << '\n'
         << "  mProfileLoopMax:" << mProfileLoopMax << '\n'
         << "  mLineDrawOnly:" << scene_rdl2::str_util::boolStr(mLineDrawOnly) << '\n'
         << "}";
    return ostr.str();
}

void
LayoutPathVis::parserConfigure()
{
    mParser.opt("testMode", "<on|off|show>", "set testMode",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else mTestMode = (arg++).as<bool>(0);
                    std::ostringstream ostr;
                    ostr << "mTestMode:" << scene_rdl2::str_util::boolStr(mTestMode);
                    return arg.msg(ostr.str() + '\n');
                });
    mParser.opt("profile", "<maxLoop|show>",
                "Set the maxLoop of the profile run. 0 disables profile run. The big maxLoop number "
                "might cause the huge latency of the draw event. But the profile itself would be accurate.",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else mProfileLoopMax = (arg++).as<unsigned>(0);
                    std::ostringstream ostr;
                    ostr << "mProfileLoopMax:" << mProfileLoopMax;
                    return arg.msg(ostr.str() + '\n');
                });
    mParser.opt("testType", "<typeId|show>", "set testType (0~5)",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else mTestType = (arg++).as<unsigned>(0);
                    std::ostringstream ostr;
                    ostr << "mTestType:" << mTestType;
                    return arg.msg(ostr.str() + '\n');
                });
    mParser.opt("show", "", "show all",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
    mParser.opt("lineDrawOnly", "<on|off|show>", "set condition of lineDrawOnly mode",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else mLineDrawOnly = (arg++).as<bool>(0);
                    return arg.msg(scene_rdl2::str_util::boolStr(mLineDrawOnly) + '\n');
                });
    mParser.opt("lineDrawOnlyToggle", "", "toggle condition of lineDrawOnly",
                [&](Arg& arg) {
                    mLineDrawOnly = !mLineDrawOnly;
                    return arg.msg(scene_rdl2::str_util::boolStr(mLineDrawOnly) + '\n');
                });
    mParser.opt("hotKeyHelp", "<on|off|show>", "display hot key help",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else mHotKeyHelp = (arg++).as<bool>(0);
                    return arg.msg(scene_rdl2::str_util::boolStr(mHotKeyHelp) + '\n');
                });
}

void
LayoutPathVis::drawGlobalProgressBar(const DisplayInfo& info)
{
    const unsigned gapWidth = 10;
    const unsigned barLeftBottomX = mBBoxGlobalInfo.upper.x + gapWidth;
    const unsigned barLeftBottomY = mBBoxTitle.lower.y - 10 - mStepPixY;
    const unsigned barWidth = mOverlay->getWidth() - barLeftBottomX - gapWidth;
    subPanelGlobalProgressBar(barLeftBottomX, barLeftBottomY, barWidth, info, mBBoxGlobalProgressBar);
}

void
LayoutPathVis::drawVector(const DisplayInfo& info)
{
    vectorPacket::Manager* packetMgrObsrPtr = info.mVectorPacketManagerObsrPtr;
    if (!packetMgrObsrPtr) return;

    try {
        packetMgrObsrPtr->decodeAll();
    }
    catch (const std::string& err) {
        std::cerr << "vectorPacket decodeAll() failed\n";
    }
}

void
LayoutPathVis::drawVectorTest0()
{
    drawVectorTestPatternA(60, // segmentTotal
                           0.0f, 360.0f, // angleStart, angleEnd
                           0.9f, // lenScale
                           1.0f, // widthMax
                           true, // constLen
                           true); // constWidth
}

void
LayoutPathVis::drawVectorTest1()
{
    drawVectorTestPatternA(60, // segmentTotal
                           0.0f, 360.0f, // angleStart, angleEnd
                           0.9f, // lenScale
                           16.0f, // widthMax
                           true, // constLen
                           false); // constWidth
}

void
LayoutPathVis::drawVectorTest2()
{
    drawVectorTestPatternA(60, // segmentTotal
                           0.0f, 360.0f, // angleStart, angleEnd
                           0.9f, // lenScale
                           16.0f, // widthMax
                           false, // constLen
                           false); // constWidth
}

void
LayoutPathVis::drawVectorTest3()
{
    constexpr float angleStart = 0.0f;
    constexpr float angleEnd = 360.0f;

    drawVectorTestPatternA(60, // segmentTotal
                           angleStart, angleEnd,
                           2.0f, // lenScale
                           16.0f, // widthMax
                           true, // constLen
                           false); // constWidth
}

void
LayoutPathVis::drawVectorTest4()
{
    // constexpr float angleStart = 70.0f; // for test
    // constexpr float angleEnd = 110.0f; // for test
    constexpr float angleStart = 0.0f;
    constexpr float angleEnd = 360.0f;

    drawVectorTestPatternA(60, // segmentTotal
                           angleStart, angleEnd,
                           2.0f, // lenScale
                           16.0f, // widthMax
                           false, // constLen
                           false); // constWidth
}

void
LayoutPathVis::drawVectorTest5()
//
// circleFill test
//
{
    using Vec2f = scene_rdl2::math::Vec2f;

    std::vector<Vec2f> posTbl;

    const float w = static_cast<float>(mOverlay->getWidth());
    const float h = static_cast<float>(mOverlay->getHeight());
    constexpr float radiusMax = 24.0f;
    constexpr float radiusStep = radiusMax / 4;
    constexpr int subdivTotalX = 20;
    constexpr int subdivTotalY = 10;

    for (int i = 0; i < 5; ++i) {
        const float offR = - radiusStep + radiusStep * static_cast<float>(i);
        const float xMin = offR;
        const float xMax = w - offR;
        const float yMin = offR;
        const float yMax = h - offR;
        const float stepX = (xMax - xMin) / static_cast<float>(subdivTotalX);
        const float stepY = (yMax - yMin) / static_cast<float>(subdivTotalY);
        for (int y = subdivTotalY; y > 0; --y) posTbl.push_back(Vec2f(xMin, static_cast<float>(y) * stepY + yMin));
        for (int x = 0; x < subdivTotalX; ++x) posTbl.push_back(Vec2f(static_cast<float>(x) * stepX + xMin, yMin));
        for (int y = 0; y < subdivTotalY; ++y) posTbl.push_back(Vec2f(xMax, static_cast<float>(y) * stepY + yMin));
        for (int x = subdivTotalX; x > 0; --x) posTbl.push_back(Vec2f(static_cast<float>(x) * stepX + xMin, yMax));
    }

    size_t totalPos = (subdivTotalX + subdivTotalY) * 2;
    for (size_t i = 0; i < totalPos; ++i) {
        const size_t currId = (i - mTestCounter) % posTbl.size();
        const float alphaRatio = 1.0f - static_cast<float>(i) / static_cast<float>(totalPos - 1);
        const float radiusRatio = alphaRatio;

        const unsigned char a = static_cast<unsigned char>(alphaRatio * 255.0f);
        const C3 c = (i == 0) ? C3(255, 0, 0) : C3(255, 255, 255);
        const float r = radiusMax * radiusRatio;

        mOverlay->circleFill(posTbl[currId], r, c, a);
    }
    mTestCounter++;
}

void
LayoutPathVis::drawVectorTest6()
{
    drawVectorTestPatternB(60, 0.2f);
}

void
LayoutPathVis::drawVectorTestPatternA(const unsigned segmentTotal,
                                      const float angleStart,
                                      const float angleEnd,
                                      const float lenScale,
                                      const float widthMax,
                                      const bool constLen,
                                      const bool constWidth)
//
// Draw spinner test pattern based on the mTestCounter value.
// mTestCounter is automatically incremented if this function is called.
//
// angleStart, angleEnd : Top is 0 degree and angle increases CCW.
//
{
    using Vec2f = scene_rdl2::math::Vec2f;

    auto degToRad = [](const float deg) { return deg / 180.0f * 3.14159265358979f; };

    const float w = static_cast<float>(mOverlay->getWidth());
    const float h = static_cast<float>(mOverlay->getHeight());
    const Vec2f center(w * 0.5f, h * 0.5f);
    const float len = std::min(w, h) * lenScale / 2.0f;

    const float angleDelta = angleEnd - angleStart;
    const float angleStep = (angleEnd - angleStart) / static_cast<float>(segmentTotal);

    for (int segId = segmentTotal - 1; segId >= 0; --segId) {
        const int angleId = (segId + mTestCounter) % segmentTotal;
        const float angleRatio = 1.0f - static_cast<float>(angleId) / static_cast<float>(segmentTotal);
        const float alphaRatio = static_cast<float>(segId) / static_cast<float>(segmentTotal - 1);
        const float widthRatio = (constWidth) ? 1.0f : alphaRatio;

        const unsigned char a = static_cast<unsigned char>(alphaRatio * 255.0f);
        const C3 c = (segId == segmentTotal - 1) ? C3(255, 0, 0) : C3(255, 255, 255);
        const float currW = widthMax * widthRatio;

        const float rad = degToRad(angleRatio * angleDelta + angleStart);
        const Vec2f dir(-1.0f * sin(rad), cos(rad));
        if (constLen) {
            mOverlay->line(center, len * dir + center, currW, false, 0.0f, false, 0.0f, c, a);
        } else {
            const Vec2f mid = len * 0.5f * dir + center;
            const Vec2f delta = (len * 0.5f * widthRatio) * dir;
            mOverlay->line(mid - delta, mid + delta, currW, false, 0.0f, false, 0.0f, c, a);
        }
    }
    mTestCounter++;
}

void
LayoutPathVis::drawVectorTestPatternB(const unsigned segmentTotal,
                                      const float lineRatio)
//
// Draw test pattern for VLine, HLine, and BoxOutline based on the mTestCounter value.
// mTestCounter is automatically incremented if this function is called.
//
{
    using BBox2i = scene_rdl2::math::BBox2i;
    using Vec2ui = scene_rdl2::math::Vec2<unsigned>;
    using Vec2i = scene_rdl2::math::Vec2i;
    using Vec2f = scene_rdl2::math::Vec2f;

    auto segIdDistRatio = [&](const unsigned i, const unsigned segId) {
        if (i < segId) return 1.0f - static_cast<float>(segId - i) / static_cast<float>(segmentTotal - 1);
        else if (i > segId) return static_cast<float>(i - segId) / static_cast<float>(segmentTotal - 1);
        else return 1.0f;
    };

    auto drawVLine = [&](const Vec2ui& wMin, const Vec2ui& wMax) {
        const unsigned segId = mTestCounter % segmentTotal;
        const float xStep = static_cast<float>(wMax[0] - wMin[0]) / static_cast<float>(segmentTotal);
        for (unsigned i = 0; i < segmentTotal; ++i) {
            mOverlay->vLine(static_cast<unsigned>(xStep * static_cast<float>(i) + wMin[0]),
                            wMin[1], wMax[1],
                            ((i == segId) ? C3(255, 0, 0) : C3(255, 255, 255)),
                            static_cast<unsigned char>(segIdDistRatio(i, segId) * 255.0f));
        }
    };
    auto drawHLine = [&](const Vec2ui& wMin, const Vec2ui& wMax) {
        const unsigned segId = mTestCounter % segmentTotal;
        const float yStep = static_cast<float>(wMax[1] - wMin[1]) / static_cast<float>(segmentTotal);
        for (unsigned i = 0; i < segmentTotal; ++i) {
            mOverlay->hLine(wMin[0], wMax[0],
                            static_cast<unsigned>(wMax[1] - yStep * static_cast<float>(i)),
                            (i == segId) ? C3(255, 0, 0) : C3(255, 255, 255),
                            static_cast<unsigned char>(segIdDistRatio(i, segId) * 255.0f));
        }
    };
    auto drawBox = [&](const Vec2ui& wMin, const Vec2ui& wMax, const float boxSizeScale,
                       const float angleStart, const float angleEnd) {
        auto degToRad = [](const float deg) { return deg / 180.0f * 3.14159265358979f; };

        const Vec2f center = Vec2f(wMin + wMax) / 2.0f;
        const Vec2f wSize = wMax - wMin;
        const float len = ((wSize[0] < wSize[1]) ? wSize[0] : wSize[1]) * 0.5f;
        const float halfBoxSize = len * boxSizeScale * 0.5f;
        const float radius = len - halfBoxSize;

        const float angleDelta = angleEnd - angleStart;
        const float angleStep = (angleEnd - angleStart) / static_cast<float>(segmentTotal);
        for (int segId = segmentTotal - 1; segId >= 0; --segId) {
            const int angleId = (segId + mTestCounter) % segmentTotal;
            const float angleRatio = 1.0f - static_cast<float>(angleId) / static_cast<float>(segmentTotal);
            const float rad = degToRad(angleRatio * angleDelta + angleStart);
            const Vec2f dir(-1.0f * sin(rad), cos(rad));
            const Vec2f p = center + dir * radius;
            mOverlay->boxOutline(BBox2i(Vec2i(p - Vec2f(halfBoxSize)), Vec2i(p + Vec2f(halfBoxSize))),
                                 (segId == segmentTotal - 1) ? C3(255, 0, 0) : C3(255, 255, 255),
                                 static_cast<unsigned char>(segIdDistRatio(segId, segmentTotal - 1) * 255.0f),
                                 true);
        }
    };

    const Vec2f wOff = Vec2f(5.0f, 5.0f);
    const Vec2f wMin = wOff;
    const Vec2f wMax = Vec2f(mOverlay->getWidth(), mOverlay->getHeight()) - wOff;
    const Vec2f hWinMin = wMin;
    const Vec2f hWinMax = hWinMin + (wMax - wMin) * Vec2f(lineRatio, 1.0f - lineRatio);
    const Vec2f vWinMin = hWinMax;
    const Vec2f vWinMax = wMax;
    const Vec2f bWinMin(hWinMax[0], hWinMin[1]);
    const Vec2f bWinMax(vWinMax[0], vWinMin[1]);
    drawVLine(Vec2ui(vWinMin + wOff), Vec2ui(vWinMax - wOff));
    drawHLine(Vec2ui(hWinMin + wOff), Vec2ui(hWinMax - wOff));
    drawBox(Vec2ui(bWinMin + wOff), Vec2ui(bWinMax - wOff), 0.2f, 0.0f, 360.0f);

    mTestCounter++;
}

void
LayoutPathVis::drawVectorProfile(const std::function<void()>& func)
{
    // run function once
    func();

    if (mProfileLoopMax == 0) return; // skip profile

    //
    // start profile run
    //
    const int oldTestCounter = mTestCounter;

#ifdef SC_TIMELOG
    mOverlay->getScTimeLog().reset();
#endif // end of SC_TIMELOG

    scene_rdl2::rec_time::RecTime recTime;
    recTime.start();
    for (unsigned i = 0; i < mProfileLoopMax; ++i) {
        func();
    }
    const float sec = recTime.end();
    const float avgSec = sec / static_cast<float>(mProfileLoopMax);

    std::ostringstream ostr;
    ostr << "drawVectorProfile {\n"
         << "  mProfileLoopMax:" << mProfileLoopMax << '\n'
         << "  test pattern draw ave:" << scene_rdl2::str_util::secStr(avgSec) << '\n';
#ifdef SC_TIMELOG
    if (!mOverlay->getScTimeLog().isEmpty()) {
        ostr << scene_rdl2::str_util::addIndent(mOverlay->getScTimeLog().show()) << '\n';
    }
#endif // end of SC_TIMELOG
    ostr << "}";
    std::cerr << ostr.str();

    mTestCounter = oldTestCounter;
}

void
LayoutPathVis::drawPathVisCtrlInfo(const DisplayInfo& info)
{
    const vectorPacket::Manager* vecPktMgrObsrPtr = info.mVectorPacketManagerObsrPtr;

    std::ostringstream ostr;
    ostr << "==== PathVisCtrl ====\n";
    if (vecPktMgrObsrPtr) {
        ostr << vecPktMgrObsrPtr->
            genPathVisCtrlInfoPanelMsg([&](const scene_rdl2::math::Color& c) -> std::string {
                C3 c3(c);
                std::ostringstream ostr;
                ostr << colFg(c3.bestContrastCol()) << colBg(c3)
                     << "rgb"
                     << colReset()
                     << '('
                     << std::setw(3) << static_cast<int>(c3.mR) << ','
                     << std::setw(3) << static_cast<int>(c3.mG) << ','
                     << std::setw(3) << static_cast<int>(c3.mB)
                     << ')'
                     << colReset();
                return ostr.str();
            });
    }

    subPanelMessage(10, // x
                    mBBoxGlobalInfo.lower.y - 10 - mStepPixY, // y
                    ostr.str(),
                    mBBoxPathVisCtrlInfo);
}

void
LayoutPathVis::drawPathVisClientInfo()
{
    const std::function<std::string()>& callBack = mDisplay.getTelemetryPanelPathVisClientInfoCallBack();
    if (!callBack) return;

    const std::string clientInfoMsg = callBack();

    subPanelMessage(10, // x
                    mBBoxPathVisCtrlInfo.lower.y - 10 - mStepPixY, // y
                    clientInfoMsg,
                    mBBoxPathVisClientInfo);
}

void
LayoutPathVis::drawPathVisCurrInfo(const DisplayInfo& info)
{
    const vectorPacket::Manager* vecPktMgrObsrPtr = info.mVectorPacketManagerObsrPtr;

    std::ostringstream ostr;
    ostr << "=== PathVis Current Info ===\n";
    if (vecPktMgrObsrPtr) {
        ostr << vecPktMgrObsrPtr->genPathVisCurrInfoPanelMsg([&]() -> std::string {
            return colReset();
        });
    }

    subPanelMessageUpperRight(mBBoxGlobalProgressBar.upper.x, // upper right X
                              mBBoxGlobalProgressBar.lower.y - 10 - 2, // upper right Y
                              ostr.str(),
                              mBBoxPathVisCurrentInfo);
}

void
LayoutPathVis::drawPathVisHotKeyHelp()
{
    if (!mHotKeyHelp) return;

    std::ostringstream ostr;
    ostr << " ========================= PathVis Panel Specific Key ==========================\n"
         << " 1 : PathVis on/off                            6 : UseSceneSamples on/off toggle\n"
         << " 2 : ++pixelSamples  2+Shift : --pixelSamples  7 : OcclusionRays on/off toggle\n"
         << " 3 : ++lightSamples  3+Shift : --lightSamples  8 : SpecularRays on/off toggle\n"
         << " 4 : ++BsdfSamples   4+Shift : --BsdfSamples   9 : DiffuseRays on/off toggle\n"
         << " 5 : ++MaxDepth      5+Shift : --MaxDepth      0 : BsdfSamples on/off toggle\n"
         << "                                              - : LgithSamples on/off toggle\n"
         << " = : current line on/off\n"
         << " \\ : current position on/off\n"
         << " P : ++rankId                P+Shift : --rankId\n"
         << " ] : ++currLineId            ]+Shift : --currLineId\n"
         << " K : only-draw-currRank toggle\n"
         << " J       : orbitCamRecenter-to-currPos\n"
         << " J+Shift : orbitCamRecenter-to-currPos-anim\n"
         << "\n"
         << " M : camCheckpointPush       M+Shift : set-currentCam-as-pathVisCam\n"
         << " V : camCheckpointNext       V+Shift : camCheckpointPrev\n"
         << " B : PathVisCam toggle\n"
         << "\n"
         << " X : pix Xpos increase       X+Shift : pix Xpos decrease\n"
         << " Y : pix Ypos increase       Y+Shift : pix Ypos decrease\n"
         << " Z : pos-move-step increase  Z+Shift : pos-move-step decrease\n"
         << "\n"
         << " ESC : draw-line-only toggle\n"
         << " ? : hot key help toggle\n"
         << "\n"
         << " ========================= PathVis Panel Specific Mouse Click ======================== \n"
         << " MouseClick + Shift        : Set PathVis pix position if cam position is pathVisCamera\n"
         << " MouseClick + Shift + Ctrl : pick current vertex if possible\n"
         << "\n"
         << " ============= General HotKey =============     ========= Free/Orbit Cam =========\n"
         << " O : freecam/orbitcam toggle                    W : Forward\n"
         << " N : denoise on/off toggle                      A : Left        D : Right\n"
         << " H : telemetry overlay on/off toggle            S : Backward\n"
         << " [ : telemetry panel to parent                  Space : UP      C : Down\n"
         << " G, '(Apostrophy) : telemetry panel to next     Q : SlowDown    E : SpeedUp\n"
         << " G+Shift, ; : telemetry panel to prev           T : PrintCamMatrix\n"
         << " / : telemetry panel to child                   U : ResetRoll\n"
         << "                                                R : RestCamToInitialPosition\n"
         << "\n"
         << "                                                F : RecenterCamera (OrbitCam only)\n"
         << "                                                L : FocusLock      (OrbitCam only)\n";
    
    subPanelMessageCenter(mOverlay->getWidth() / 2,
                          mOverlay->getHeight() / 2,
                          ostr.str(),
                          mBBoxPathVisHotKeyHelp);
}

} // namespace telemetry
} // namespace mcrt_dataio
