// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "VerifyFeedback.h"

int
main(int ac, char **av)
{
    verifyFeedback::VerifyFeedback verifyFeedback;
    if (!verifyFeedback.main(ac, av)) {
        std::cerr << "error\n";
    }

    return 1;
}
