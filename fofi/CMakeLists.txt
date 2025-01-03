#========================================================================
#
# fofi/CMakeLists.txt
#
# CMake script for the fofi library.
#
# Copyright 2015 Glyph & Cog, LLC
#
#========================================================================

cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_STANDARD 20)

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/goo")

find_package(fmt CONFIG REQUIRED)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
include_directories("${UWEBSOCKETS_INCLUDE_DIRS}")

add_library(fofi_objs OBJECT
	FoFiBase.cc
	FoFiEncodings.cc
	FoFiIdentifier.cc
	FoFiTrueType.cc
	FoFiType1.cc
	FoFiType1C.cc
)
set_property(TARGET fofi_objs
	PROPERTY POSITION_INDEPENDENT_CODE True)

add_library(fofi
	$<TARGET_OBJECTS:fofi_objs>
)

target_link_libraries(fofi_objs PRIVATE
	fmt::fmt-header-only
)
