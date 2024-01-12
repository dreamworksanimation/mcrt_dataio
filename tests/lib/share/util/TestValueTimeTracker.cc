// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TestValueTimeTracker.h"

#include <scene_rdl2/render/util/StrUtil.h>

#include <random>
#include <unistd.h> // usleep

namespace mcrt_dataio {
namespace unittest {

void
TestValueTimeTracker::testFull()
{
    for (int i = 0; i < 3; ++i) {
        int dataCount = 0;
        switch (i) {
        case 0 : dataCount = 2; break;
        case 1 : dataCount = 15; break;
        case 2 : dataCount = 27; break;
        }

        ValueTimeTracker vt(1.0f);
        dataSetup(dataCount, -1, 0.0f, 1.0f, vt);
        // std::cerr << vt.show() << '\n'; // useful debug message

        CPPUNIT_ASSERT("testFull resample=1" && main(1, vt));
        CPPUNIT_ASSERT("testFull resample=7" && main(7, vt));
        CPPUNIT_ASSERT("testFull resample=15" && main(15, vt));
        CPPUNIT_ASSERT("testFull resample=33" && main(33, vt));
    }
}

void
TestValueTimeTracker::testShort()
{
    for (int i = 0; i < 3; ++i) {
        int stopId = -1;
        switch (i) {
        case 0 : stopId = 2; break;
        case 1 : stopId = 9; break;
        case 2 : stopId = 13; break;
        }

        ValueTimeTracker vt(1.0f);
        dataSetup(15, stopId, 0.0f, 1.0f, vt);
        // std::cerr << vt.show() << '\n'; // useful debug message

        CPPUNIT_ASSERT("testShort resample=1" && main(1, vt));
        CPPUNIT_ASSERT("testShort resample=7" && main(7, vt));
        CPPUNIT_ASSERT("testShort resample=15" && main(15, vt));
        CPPUNIT_ASSERT("testShort resample=33" && main(33, vt));
    }
}

void
TestValueTimeTracker::testSingle()
//
// test single entry data
//
{
    ValueTimeTracker vt(1.0f);
    dataSetup(15, 1, 0.0f, 1.0f, vt);
    // std::cerr << vt.show() << '\n'; // useful debug message

    CPPUNIT_ASSERT("testSingle resample=1" && main(1, vt));
    CPPUNIT_ASSERT("testSingle resample=7" && main(7, vt));
    CPPUNIT_ASSERT("testSingle resample=15" && main(15, vt));
    CPPUNIT_ASSERT("testSingle resample=33" && main(33, vt));
}

void
TestValueTimeTracker::testEmpty()
//
// test empty data
//
{
    ValueTimeTracker vt(1.0f);
    // std::cerr << vt.show() << '\n'; // useful debug message

    CPPUNIT_ASSERT("testEmpty resample=1" && main(1, vt));
    CPPUNIT_ASSERT("testEmpty resample=7" && main(7, vt));
    CPPUNIT_ASSERT("testEmpty resample=15" && main(15, vt));
    CPPUNIT_ASSERT("testEmpty resample=33" && main(33, vt));
}

bool
TestValueTimeTracker::main(int resampleCount, const ValueTimeTracker& vt) const
{
    std::vector<float> tbl0;
    vt.getResampleValue(resampleCount, tbl0);
    std::vector<float> tbl1;
    vt.getResampleValueExhaust(resampleCount, tbl1);

    /* useful debug message
    float timeStepSec = vt.getValueKeepDurationSec() / static_cast<float>(resampleCount);
    std::cerr << showVec(tbl0, timeStepSec) << '\n';
    std::cerr << showVec(tbl1, timeStepSec) << '\n';
    */

    return compareTbl(tbl0, tbl1);
}

void
TestValueTimeTracker::dataSetup(int dataCount,
                                int stopId, // -1 if you need all data
                                float minValue,
                                float maxValue,
                                ValueTimeTracker& vt) const
{
    auto randomUSleep = [](float maxSec) { usleep(static_cast<int>(maxSec * 1000000.0f)); };

    std::random_device seedGen;
    std::mt19937 engine(seedGen());
    std::uniform_real_distribution<float> dist(minValue, maxValue);

    float maxSleep = vt.getValueKeepDurationSec() / static_cast<float>(dataCount - 1);
    for (int i = 0; i < dataCount; ++i) {
        if (stopId == i) break;
        randomUSleep(maxSleep);
        vt.push(dist(engine));
    }
}

bool
TestValueTimeTracker::compareTbl(const std::vector<float>& tblA, const std::vector<float>& tblB) const
{
    size_t max = tblA.size();
    if (max != tblB.size()) return false;
    
    constexpr float threshold = 0.0001f;

    for (size_t i = 0; i < max; ++i) {
        if (abs(tblB[i] - tblA[i]) > threshold) return false;
    }
    return true;
}

std::string
TestValueTimeTracker::showVec(std::vector<float>& tbl, float timeStepSec) const
{
    if (tbl.empty()) return "empty";

    int w = scene_rdl2::str_util::getNumberOfDigits(tbl.size() - 1);

    std::ostringstream ostr;
    ostr << "size:" << tbl.size() << " {\n";
    for (size_t i = 0; i < tbl.size(); ++i) {
        float startTime = timeStepSec * static_cast<float>(i);
        float endTime = startTime + timeStepSec;
        ostr << "  i:" << std::setw(w) << i
             << " " << scene_rdl2::str_util::secStr(startTime) << "-"
             << scene_rdl2::str_util::secStr(endTime)
             << ' ' << tbl[i] << '\n';
    }
    ostr << "}";
    return ostr.str();
}

} // namespace unittest
} // namespace mcrt_dataio
