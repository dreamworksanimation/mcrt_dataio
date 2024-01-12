// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

namespace mcrt_dataio {
namespace unittest {

class TestMergeSequenceCodec : public CppUnit::TestFixture
{
public:
    void setUp() {}
    void tearDown() {}

    void testSequence();

    CPPUNIT_TEST_SUITE(TestMergeSequenceCodec);
    CPPUNIT_TEST(testSequence);
    CPPUNIT_TEST_SUITE_END();

private:

    bool main(const std::string& input) const;
};

} // namespace unittest
} // namespace mcrt_dataio
