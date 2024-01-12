// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TestMergeSequenceCodec.h"
#include "TestMergeTracker.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <scene_rdl2/pdevunit/pdevunit.h>

int
main(int ac, char **av)
{
    using namespace mcrt_dataio::unittest;

    CPPUNIT_TEST_SUITE_REGISTRATION(TestMergeSequenceCodec);
    CPPUNIT_TEST_SUITE_REGISTRATION(TestMergeTracker);

    return pdevunit::run(ac, av);
}
