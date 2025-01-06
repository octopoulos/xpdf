// stdafx.h
// octopoulos @ 2025-01-01

#pragma once

#ifndef NOMINMAX
#	define NOMINMAX
#endif

#include <atomic>        //
#include <cassert>       // assert
#include <cctype>        // tolower
#include <cstdint>       // uint64_t
#include <iostream>      //
#include <filesystem>    //
#include <fstream>       //
#include <map>           //
#include <mutex>         //
#include <string>        //
#include <string_view>   //
#include <unordered_map> //
#include <unordered_set> //
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

// templates for map, umap, uset, vec
// level 1
template <typename T, typename T2>
using MAP = std::map<T, T2>;

// !! MSVC preserves insertion order, GCC does not !!
template <typename T, typename T2>
using UMAP = std::unordered_map<T, T2>;

template <typename T>
using USET = std::unordered_set<T>;

template <typename T>
using VEC = std::vector<T>;

// level 2
template <typename T>
using MAP_STR = MAP<std::string, T>;

template <typename T>
using UMAP_STR = UMAP<std::string, T>;

// shortcuts for uset & vec
using USET_INT    = USET<int>;
using USET_INT64  = USET<int64_t>;
using USET_STR    = USET<std::string>;
using VEC_INT     = VEC<int>;
using VEC_INT64   = VEC<int64_t>;
using VEC_STR     = VEC<std::string>;

// shortcuts for map & umap
using UMAP_INT_INT   = UMAP<int, int>;
using UMAP_INT_STR   = UMAP<int, std::string>;
using UMAP_INT64_INT = UMAP<int64_t, int>;
using UMAP_INT64_STR = UMAP<int64_t, std::string>;
using UMAP_NAMES     = UMAP<int, USET_STR>;
using UMAP_STR_INT   = UMAP_STR<int>;
using UMAP_STR_INT64 = UMAP_STR<int64_t>;
using UMAP_STR_STR   = UMAP_STR<std::string>;
using UMAP_VIEW_INT  = UMAP<std::string_view, int>;

#include "common.h"
