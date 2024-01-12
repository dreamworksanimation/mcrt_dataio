# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(target mcrt_dataio_share_util_tests)

add_executable(${target})

target_sources(${target}
    PRIVATE
        main.cc
	TestValueTimeTracker.cc
)

target_link_libraries(${target}
    PRIVATE
        SceneRdl2::pdevunit
        McrtDataio::client_receiver
)

# Set standard compile/link options
McrtDataio_cxx_compile_definitions(${target})
McrtDataio_cxx_compile_features(${target})
McrtDataio_cxx_compile_options(${target})
McrtDataio_link_options(${target})

add_test(NAME ${target} COMMAND ${target})
set_tests_properties(${target} PROPERTIES LABELS "unit")
