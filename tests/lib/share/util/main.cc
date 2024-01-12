// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TestValueTimeTracker.h"

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>
#include <scene_rdl2/pdevunit/pdevunit.h>

int
main(int argc, char** argv)
{
    using namespace mcrt_dataio::unittest;

    CPPUNIT_TEST_SUITE_REGISTRATION(TestValueTimeTracker);

    return pdevunit::run(argc, argv);
}
