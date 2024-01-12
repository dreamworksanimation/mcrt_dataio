# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(target sockTest)

add_executable(${target})

target_sources(${target}
    PRIVATE
	main.cc
)

target_link_libraries(${target}
    PRIVATE
	McrtDataio::share_sock
)

# Set standard compile/link options
McrtDataio_cxx_compile_definitions(${target})
McrtDataio_cxx_compile_features(${target})
McrtDataio_cxx_compile_options(${target})
McrtDataio_link_options(${target})

install(TARGETS ${target}
    RUNTIME DESTINATION bin)
