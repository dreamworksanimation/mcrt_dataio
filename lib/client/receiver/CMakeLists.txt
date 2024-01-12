# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component client_receiver)

set(InstallIncludeDir ${PACKAGE_NAME}/client/receiver)
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

target_sources(${component}
    PRIVATE
        ClientReceiverConsoleDriver.cc
        ClientReceiverDenoiser.cc
        ClientReceiverFb.cc
        ClientReceiverStats.cc
	TelemetryDisplay.cc
	TelemetryLayoutBase.cc
	TelemetryLayoutCorePerf.cc
	TelemetryLayoutDevel.cc
	TelemetryLayoutFeedback.cc
	TelemetryLayoutNetIO.cc
	TelemetryLayoutPanel.cc
	TelemetryOverlay.cc
	TelemetryPanel.cc
        TimingAnalysis.cc
        TimingRecorderHydra.cc
)

set_property(TARGET ${component}
    PROPERTY PUBLIC_HEADER
        ClientReceiverConsoleDriver.h
        ClientReceiverDenoiser.h
        ClientReceiverFb.h
        ClientReceiverStats.h
        TimingAnalysis.h
        TimingRecorderHydra.h
)

target_include_directories(${component}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${component}
    PRIVATE
	FreeType::FreeType
    PUBLIC
        McrtDenoise::denoiser
	${PROJECT_NAME}::engine_mcrt
        ${PROJECT_NAME}::engine_merger
        ${PROJECT_NAME}::share_codec
        ${PROJECT_NAME}::share_util
        McrtMessages::mcrt_messages
        SceneRdl2::common_grid_util
        SceneRdl2::common_math
        SceneRdl2::common_platform
        SceneRdl2::common_rec_time
        SceneRdl2::render_util
        TBB::tbb
)

# If at Dreamworks add a SConscript stub file so others can use this library.
SConscript_Stub(${component})

# Set standard compile/link options
McrtDataio_cxx_compile_definitions(${component})
McrtDataio_cxx_compile_features(${component})
McrtDataio_cxx_compile_options(${component})
McrtDataio_link_options(${component})

if (MOONRAY_USE_CUDA)
    target_compile_definitions(${component}
        PRIVATE MOONRAY_USE_CUDA)
endif()

# -------------------------------------
# Install the target and the export set
# -------------------------------------
include(GNUInstallDirs)

# install the target
install(TARGETS ${component}
    COMPONENT ${component}
    EXPORT ${exportGroup}
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        NAMELINK_SKIP
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${InstallIncludeDir}
)

# # install the export set
# install(
#     EXPORT ${exportGroup}
#     NAMESPACE ${PROJECT_NAME}::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
# )
