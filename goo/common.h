// common.h
// octopoulos @ 2024-12-29

#pragma once

#if LOAD_FONTS_FROM_MEM
#	define LOAD_FONT_ARGS_CALLS(...) fontBuf##__VA_ARGS__
#	define LOAD_FONT_ARGS_DEFS(...)  const std::string& fontBuf##__VA_ARGS__
#else
#	define LOAD_FONT_ARGS_CALLS(...) fileName##__VA_ARGS__, deleteFile##__VA_ARGS__
#	define LOAD_FONT_ARGS_DEFS(...)  const char *fileName##__VA_ARGS__, bool deleteFile##__VA_ARGS__
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLI OPTIONS
//////////////

#define CLI_BEGIN(name)                 \
	CLI::App              cli { name }; \
	std::set<std::string> needApps;

#define CLI_NEEDAPP()                                                                        \
	int needApp = 0;                                                                         \
	for (int i = 0; i < argc; ++i)                                                           \
	{                                                                                        \
		if (const std::string name = argv ? argv[i] : ""; needApps.contains(name))           \
		{                                                                                    \
			if (i + 1 == argc)                                                               \
				++needApp;                                                                   \
			else                                                                             \
			{                                                                                \
				std::string_view value = (argv && i + 1 < argc) ? argv[i + 1] : "";          \
				if (value.size() && value.front() != '0' && value.front() != '=') ++needApp; \
			}                                                                                \
		}                                                                                    \
	}

/// Use with CLI11
/// ex: CLI_OPTION(tests, int, 0, 1, "Run tests");
#define CLI_OPTION(name, type, def, imp, app, desc) \
	type name = def;                                \
	if (app) needApps.insert("--" #name);           \
	cli.add_flag(fmt::format("--{}{{{}}}", #name, imp), name, desc)->default_val(def)->expected(0, 1)

#define CLI_REQUIRED(name, type, req, desc) \
	type name;                              \
	cli.add_option(#name, name, desc)->required(!!req)

#define CLI_STRING(text) text.size() && !text.starts_with('=')

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MISC
///////

/**
 * Find key in a container without exception
 */
template <typename C, typename K, typename T>
inline T FindDefault_(const C& container, const K& key, const T& def)
{
	if (const auto& it = container.find(key); it != container.end()) [[likely]]
		return it->second;
	else
		return def;
}

// clang-format off
inline int         FindDefault(const UMAP_STR_INT& container, const std::string& key, const int& def        ) { return FindDefault_<UMAP_STR_INT, std::string, int        >(container, key, def); }
inline std::string FindDefault(const UMAP_STR_STR& container, const std::string& key, const std::string& def) { return FindDefault_<UMAP_STR_STR, std::string, std::string>(container, key, def); }

// clang-format on
