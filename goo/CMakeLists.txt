# ========================================================================
#
# goo/CMakeLists.txt
#
# CMake script for the goo library.
#
# Copyright 2015 Glyph & Cog, LLC
#
# ========================================================================

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")

find_package(fmt CONFIG REQUIRED)
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h")
include_directories("${UWEBSOCKETS_INCLUDE_DIRS}")

add_library(goo_objs OBJECT
	FixedPoint.cc
	GHash.cc
	GList.cc
	GString.cc
	gfile.cc
	gmem.cc
	gmempp.cc
	parseargs.c
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
