# ========================================================================
#
# goo/CMakeLists.txt
#
# CMake script for the goo library.
#
# Copyright 2015 Glyph & Cog, LLC
#
# ========================================================================

cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_STANDARD 20)

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")

find_package(fmt CONFIG REQUIRED)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
include_directories(
	.
	"${UWEBSOCKETS_INCLUDE_DIRS}"
)

add_library(goo_objs OBJECT
	FixedPoint.cc
	gfile.cc
	GList.cc
	gmem.cc
	gmempp.cc
	GString.cc
	stdafx.h
	Trace.cc
)
set_property(TARGET goo_objs
	PROPERTY POSITION_INDEPENDENT_CODE True)

add_library(goo
	$<TARGET_OBJECTS:goo_objs>
)

target_link_libraries(goo PRIVATE
	fmt::fmt-header-only
)
