//========================================================================
//
// gfile.h
//
// Miscellaneous file and directory name manipulation.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#if defined(_WIN32)
#	include <sys/stat.h>
#	ifdef FPTEX
#		include <win32lib.h>
#	else
#		include <windows.h>
#	endif
#elif defined(ACORN)
#elif defined(ANDROID)
#else
#	include <unistd.h>
#	include <sys/types.h>
#endif

// Windows 10 supports long paths - with a registry setting, and only with Unicode (...W) functions.
#ifdef _WIN32
#	define winMaxLongPath 32767
#endif

//------------------------------------------------------------------------

// Get home directory path.
extern std::string getHomeDir();

// Get current directory.
extern std::string getCurrentDir();

// Append a file name to a path string.
// <path> may be an empty string, denoting the current directory).
// Returns <path>.
extern std::string appendToPath(std::string path, const char* fileName);

// Grab the path from the front of the file name.
// If there is no directory component in <fileName>, returns an empty string.
extern std::string grabPath(const char* fileName);

// Is this an absolute path or file name?
extern bool isAbsolutePath(const char* path);

// Make this path absolute by prepending current directory (if path is  relative)
// or prepending user's directory (if path starts with '~').
extern std::string makePathAbsolute(std::string path);

// Returns true if [path] exists and is a regular file.
extern bool pathIsFile(const char* path);

// Returns true if [path] exists and is a directory.
extern bool pathIsDir(const char* path);

// Get the modification time for <fileName>.  Returns 0 if there is an
// error.
extern time_t getModTime(char* fileName);

// Create a temporary file and open it for writing.
// If <ext> is not nullptr, it will be used as the file name extension.
// Returns both the name and the file pointer.
// For security reasons, all writing should be done to the returned file pointer;
// the file may be reopened later for reading, but not for writing.
// The <mode> string should be "w" or "wb".
// Returns true on success.
extern bool openTempFile(std::string& name, FILE** f, const char* mode, const char* ext);

// Create a directory.  Returns true on success.
extern bool createDir(char* path, int mode);

// Execute <command>.  Returns true on success.
extern bool executeCommand(char* cmd);

// Open a file.
// On Windows, this converts the path from UTF-8 to UCS-2 and calls _wfopen().
// On other OSes, this simply calls fopen().
extern FILE* openFile(const char* path, const char* mode);

// Create a directory.
// On Windows, this converts the path from UTF-8 to UCS-2 and calls _wmkdir(), ignoring the mode argument.
// On other OSes, this simply calls mkdir().
extern bool makeDir(const std::filesystem::path& path, int mode);

// Just like fgets, but handles Unix, Mac, and/or DOS end-of-line conventions.
extern char* getLine(char* buf, int size, FILE* f);

// Type used by gfseek/gftell for file offsets.
// This will be 64 bits on systems that support it.
#if HAVE_FSEEKO
typedef off_t GFileOffset;
#	define GFILEOFFSET_MAX 0x7fffffffffffffffLL
#elif HAVE_FSEEK64
typedef long long GFileOffset;
#	define GFILEOFFSET_MAX 0x7fffffffffffffffLL
#elif HAVE_FSEEKI64
typedef __int64 GFileOffset;
#	define GFILEOFFSET_MAX 0x7fffffffffffffffLL
#else
typedef long GFileOffset;
#	define GFILEOFFSET_MAX LONG_MAX
#endif

// Like fseek, but uses a 64-bit file offset if available.
extern int gfseek(FILE* f, GFileOffset offset, int whence);

// Like ftell, but returns a 64-bit file offset if available.
extern GFileOffset gftell(FILE* f);

// On Windows, this gets the Unicode command line and converts it to UTF-8.
// On other systems, this is a nop.
extern void fixCommandLine(int* argc, char** argv[]);
