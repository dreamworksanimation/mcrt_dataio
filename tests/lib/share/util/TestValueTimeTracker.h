// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mcrt_dataio/share/util/ValueTimeTracker.h>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

namespace mcrt_dataio {
namespace unittest {

class TestValueTimeTracker : public CppUnit::TestFixture
{
public:
    void setUp() {}
    void tearDown() {}

    void testFull();
    void testShort();
    void testSingle();
    void testEmpty();
    
    CPPUNIT_TEST_SUITE(TestValueTimeTracker);
    CPPUNIT_TEST(testFull);
    CPPUNIT_TEST(testShort);
    CPPUNIT_TEST(testSingle);
    CPPUNIT_TEST(testEmpty);
    CPPUNIT_TEST_SUITE_END();

private:
    bool main(int resampleCount, const ValueTimeTracker& vt) const;

    void dataSetup(int dataCount,
                   int stopId, // -1 if you need all data
                   float minValue,
                   float maxValue,
                   ValueTimeTracker& vt) const;
    bool compareTbl(const std::vector<float>& tblA,
                    const std::vector<float>& tblB) const;
    std::string showVec(std::vector<float>& tbl, float timeStepSec) const;
};

} // namespace unittest
} // namespace mcrt_dataio
