# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(target verifyMcrtFeedback)

add_executable(${target})

target_sources(${target}
    PRIVATE
        Fb.cc
        main.cc
        Mcrt.cc
        Merge.cc
        VerifyFeedback.cc
)

target_link_libraries(${target}
    PRIVATE
        SceneRdl2::common_fb_util
        SceneRdl2::common_grid_util
        SceneRdl2::common_math
)

# Set standard compile/link options
McrtDataio_cxx_compile_definitions(${target})
McrtDataio_cxx_compile_features(${target})
McrtDataio_cxx_compile_options(${target})
McrtDataio_link_options(${target})

install(TARGETS ${target}
    RUNTIME DESTINATION bin)
