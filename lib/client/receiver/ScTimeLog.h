// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <scene_rdl2/common/rec_time/RecTime.h>
#include <scene_rdl2/render/util/StrUtil.h>

#include <sstream>
#include <string>

namespace mcrt_dataio {
namespace telemetry {

class ScTimeLog
{
public:
    ScTimeLog()
    {
        reset();
    }

    void reset() {
        for (size_t i = 0; i < 5; ++i) mSecTbl[i] = 0.0f;
        mLastTime = 0.0f;
        mRecTime.reset();
    }

    bool isEmpty() const { return (mTotalScanConvert == 0) ? true : false; }

    void start() {
        mTotalScanConvert++;
        mRecTime.start();
        mLastTime = 0.0f;
    }
    void endCalcMinMaxY() { recLap(0); }
    void endXScan() { recLap(1); }
    void endSetupCoverageTbl() { recLap(2); }
    void endCalcCoverage() { recLap(3); }
    void endScanlineFill() { recLap(4); }

    void finalize()
    {
        for (unsigned int i = 0; i < 5; ++i) {
            mSecTbl[i] /= static_cast<float>(mTotalScanConvert);
        }
    }

    std::string show() const
    {
        auto pctStr = [](const float v) {
            std::ostringstream ostr;
            ostr << v * 100.0f << "%";
            return ostr.str();
        };

        const float total = all();
        std::ostringstream ostr;
        ostr << "ScTimeLog {\n"
             << "  mTotalScanConvert:" << mTotalScanConvert << '\n'
             << "       calcMinMaxY():" << scene_rdl2::str_util::secStr(mSecTbl[0]) << ' ' << pctStr(mSecTbl[0] / total) << '\n'
             << "             xScan():" << scene_rdl2::str_util::secStr(mSecTbl[1]) << ' ' << pctStr(mSecTbl[1] / total) << '\n'
             << "  setupCoverageTbl():" << scene_rdl2::str_util::secStr(mSecTbl[2]) << ' ' << pctStr(mSecTbl[2] / total) << '\n'
             << "      calcCoverage():" << scene_rdl2::str_util::secStr(mSecTbl[3]) << ' ' << pctStr(mSecTbl[3] / total) << '\n'
             << "      scanlineFill():" << scene_rdl2::str_util::secStr(mSecTbl[4]) << ' ' << pctStr(mSecTbl[4] / total) << '\n'
             << "}";
        return ostr.str();
    }

protected:

    float lapTime() {
        const float oldLastTime = mLastTime;
        mLastTime = mRecTime.end();
        return mLastTime - oldLastTime;
    }

    void recLap(const int id) { mSecTbl[id] += lapTime(); }

    float all() const
    {
        float total = 0.0f;
        for (int i = 0; i < 5; ++i) {
            total += mSecTbl[i];
        }
        return total;
    }

    //------------------------------

    scene_rdl2::rec_time::RecTime mRecTime;

    unsigned mTotalScanConvert {0};
    float mLastTime {0.0f};

    // 0: calcMinMaxY()
    // 1: xScan()
    // 2: setupCoverageTbl()
    // 3: calcCoverage()
    // 4: scanlineFill()
    float mSecTbl[5];
};

} // namespace telemetry
} // namespace mcrt_dataio
