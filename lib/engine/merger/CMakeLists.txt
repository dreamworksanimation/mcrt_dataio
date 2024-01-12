# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

set(component engine_merger)

set(InstallIncludeDir ${PACKAGE_NAME}/engine/merger)
set(exportGroup ${PROJECT_NAME}Targets)

add_library(${component} SHARED "")
add_library(${PROJECT_NAME}::${component} ALIAS ${component})

target_sources(${component}
    PRIVATE
        FbMsgMultiChans.cc
        FbMsgMultiFrames.cc
        FbMsgSingleChan.cc
        FbMsgSingleFrame.cc
        FbMsgUtil.cc
        GlobalNodeInfo.cc
	MergeActionTracker.cc
        MergeFbSender.cc
	MergeSequenceEnqueue.cc
        MergeStats.cc
)

set_property(TARGET ${component}
    PROPERTY PUBLIC_HEADER
        FbMsgMultiChans.h
        FbMsgMultiFrames.h
        FbMsgSingleChan.h
        FbMsgSingleFrame.h
        GlobalNodeInfo.h
	MergeActionTracker.h
        MergeFbSender.h
	MergeSequenceDequeue.h
	MergeSequenceEnqueue.h
	MergeSequenceKey.h
        MergeStats.h
        MsgSendHandler.h
)

target_include_directories(${component}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${component}
    PUBLIC
        SceneRdl2::common_fb_util
        SceneRdl2::common_grid_util
        SceneRdl2::common_platform
	SceneRdl2::render_cache
        SceneRdl2::render_util
        SceneRdl2::scene_rdl2
        ${PROJECT_NAME}::engine_mcrt
        McrtMessages::mcrt_messages
        TBB::tbb
)

# If at Dreamworks add a SConscript stub file so others can use this library.
SConscript_Stub(${component})

# Set standard compile/link options
McrtDataio_cxx_compile_definitions(${component})
McrtDataio_cxx_compile_features(${component})
McrtDataio_cxx_compile_options(${component})
McrtDataio_link_options(${component})

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
