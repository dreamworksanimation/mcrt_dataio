# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(target mcrt_dataio_engine_merger_tests)

add_executable(${target})

target_sources(${target}
    PRIVATE
        main.cc
        TestMergeSequenceCodec.cc
        TestMergeTracker.cc	
)

target_link_libraries(${target}
    PRIVATE
        SceneRdl2::common_grid_util
        SceneRdl2::render_cache
        SceneRdl2::pdevunit
        McrtDataio::engine_merger
)

# Set standard compile/link options
McrtDataio_cxx_compile_definitions(${target})
McrtDataio_cxx_compile_features(${target})
McrtDataio_cxx_compile_options(${target})
McrtDataio_link_options(${target})

add_test(NAME ${target} COMMAND ${target})
set_tests_properties(${target} PROPERTIES LABELS "unit")
