// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/common/grid_util/Arg.h>

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

namespace mcrt_dataio {
namespace unittest {

class TestMergeTracker : public CppUnit::TestFixture
{
public:
    void setUp() {}
    void tearDown() {}

    void testCodec();

    CPPUNIT_TEST_SUITE(TestMergeTracker);
    CPPUNIT_TEST(testCodec);
    CPPUNIT_TEST_SUITE_END();

private:

    bool main(const std::string& input, const std::string& output) const;

    bool encodeTestData(const std::string& input, std::string& data) const;
    std::vector<scene_rdl2::grid_util::Arg> convertToArgs(const std::string& input) const;
    std::string decodeTestData(const std::string& data) const;
};

} // namespace unittest
} // namespace mcrt_dataio
