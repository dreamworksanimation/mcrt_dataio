# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

find_path(FreeType_INCLUDE_DIR
    NAMES ft2build.h
    PATH_SUFFIXES freetype2
    HINTS $ENV{FREETYPE_ROOT}/include /usr/local/include)

# need to find <freetype2/ft2build.h.h>
set(FreeType_INCLUDE_DIRS ${FreeType_INCLUDE_DIR} ${FreeType_INCLUDE_DIR}/..)

find_library(FreeType_LIBRARIES
    NAMES freetype
    HINTS $ENV{FREETYPE_ROOT}/lib /usr/local/lib)

mark_as_advanced(FreeType_INCLUDE_DIR FreeType_INCLUDE_DIRS FreeType_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FreeType
    REQUIRED_VARS FreeType_LIBRARIES FreeType_INCLUDE_DIRS
)

if (FreeType_FOUND AND NOT TARGET FreeType::FreeType)
    add_library(FreeType::FreeType UNKNOWN IMPORTED)
    set_target_properties(FreeType::FreeType PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${FreeType_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${FreeType_INCLUDE_DIRS}")
endif()

