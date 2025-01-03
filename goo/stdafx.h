// stdafx.h
// octopoulos @ 2024-12-29

#pragma once

#ifndef NOMINMAX
#	define NOMINMAX
#endif

#include <atomic>        //
#include <cctype>        // tolower
#include <cstdint>       // uint64_t
#include <iostream>      //
#include <filesystem>    //
#include <fstream>       //
#include <mutex>         //
#include <string>        //
#include <string_view>   //
#include <unordered_map> //
#include <vector>        //

using namespace std::string_literals;

#ifdef _MSC_VER
#	define _CRTDBG_MAP_ALLOC
#	include <stdlib.h>
#	include <crtdbg.h>
#endif

#ifndef FMT_HEADER_ONLY
#	define FMT_HEADER_ONLY
#endif
#include <fmt/core.h>

#define TO_INT(x)    static_cast<int>(x)
#define TO_UINT32(x) static_cast<uint32_t>(x)

// !! MSVC preserves insertion order, GCC does not !!
#define UMAP std::unordered_map

// shortcuts for map & set
using UMAP_STR_INT = std::unordered_map<std::string, int>;
using UMAP_STR_STR = std::unordered_map<std::string, std::string>;
using VEC_STR      = std::vector<std::string>;

#include "common.h"
