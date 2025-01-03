//========================================================================
//
// gfile.cc
//
// Miscellaneous file and directory name manipulation.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#ifdef _WIN32
#	undef WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <time.h>
#	include <direct.h>
#	include <shobjidl.h>
#	include <shlguid.h>
#else
#	if !defined(ACORN)
#		include <sys/types.h>
#		include <sys/stat.h>
#		include <fcntl.h>
#	endif
#	include <time.h>
#	include <limits.h>
#	include <string.h>
#	if !defined(VMS) && !defined(ACORN)
#		include <pwd.h>
#	endif
#	if defined(VMS) && (__DECCXX_VER < 50200000)
#		include <unixlib.h>
#	endif
#endif // _WIN32
#include "gmem.h"
#include "gmempp.h"
#include "gfile.h"

// Some systems don't define this, so just make it something reasonably
// large.
#ifndef PATH_MAX
#	define PATH_MAX 1024
#endif

//------------------------------------------------------------------------

std::string getHomeDir()
{
#ifdef VMS
	//---------- VMS ----------
	return "SYS$LOGIN:";

#elif defined(_WIN32)
	//---------- Win32 ----------
	if (char* s = getenv("USERPROFILE"); s && *s)
		return s;
	else
		return ".";

#elif defined(__EMX__)
	//---------- OS/2+EMX ----------
	if (char* s = getenv("HOME"); s && *s)
		return s;
	else
		return ".";

#elif defined(ACORN)
	//---------- RISCOS ----------
	return "@";

#else
	//---------- Unix ----------
	struct passwd* pw;

	if (char* s = getenv("HOME"); s && *s)
		return s;
	else
	{
		if (char* s = getenv("USER"); s && *s)
			pw = getpwnam(s);
		else
			pw = getpwuid(getuid());
		if (pw)
			return pw->pw_dir;
		else
			return ".";
	}
#endif
}

std::string getCurrentDir()
{
	char buf[PATH_MAX + 1];

#if defined(__EMX__)
	if (_getcwd2(buf, sizeof(buf)))
#elif defined(_WIN32)
	if (GetCurrentDirectoryA(sizeof(buf), buf))
#elif defined(ACORN)
	if (strcpy(buf, "@"))
#else
	if (getcwd(buf, sizeof(buf)))
#endif
		return buf;
	return "";
}

std::string appendToPath(std::string path, const char* fileName)
{
#if defined(VMS)
	//---------- VMS ----------
	//~ this should handle everything necessary for file
	//~ requesters, but it's certainly not complete
	char *p0, *p1, *p2;
	char* q1;

	p0 = (char*)path.c_str();
	p1 = p0 + path.size() - 1;
	if (!strcmp(fileName, "-"))
	{
		if (*p1 == ']')
		{
			for (p2 = p1; p2 > p0 && *p2 != '.' && *p2 != '['; --p2)
				;
			if (*p2 == '[')
				++p2;
			path.erase(p2 - p0, p1 - p2);
		}
		else if (*p1 == ':')
		{
			path.append("[-]");
		}
		else
		{
			path.clear();
			path.append("[-]");
		}
	}
	else if ((q1 = strrchr(fileName, '.')) && !strncmp(q1, ".DIR;", 5))
	{
		if (*p1 == ']')
		{
			path.insert(p1 - p0, '.');
			path.insert(p1 - p0 + 1, fileName, q1 - fileName);
		}
		else if (*p1 == ':')
		{
			path += '[';
			path += ']';
			path.append(fileName, q1 - fileName);
		}
		else
		{
			path->clear();
			path.append(fileName, q1 - fileName);
		}
	}
	else
	{
		if (*p1 != ']' && *p1 != ':')
			path->clear();
		path->append(fileName);
	}
	return path;

#elif defined(_WIN32)
	//---------- Win32 ----------
	char  buf[256];
	char* fp;

	std::string tmp = path;
	tmp += '/';
	tmp += fileName;
	GetFullPathNameA(tmp.c_str(), sizeof(buf), buf, &fp);
	path = buf;
	return path;

#elif defined(ACORN)
	//---------- RISCOS ----------
	char* p;
	int   i;

	path += ".";
	i = path.size();
	path += fileName;
	for (p = path.c_str() + i; *p; ++p)
		if (*p == '/')
			*p = '.';
		else if (*p == '.')
			*p = '/';
	return path;

#elif defined(__EMX__)
	//---------- OS/2+EMX ----------
	int i;

	// appending "." does nothing
	if (!strcmp(fileName, "."))
		return path;

	// appending ".." goes up one directory
	if (!strcmp(fileName, ".."))
	{
		for (i = path.size() - 2; i >= 0; --i)
			if (path.at(i) == '/' || path.at(i) == '\\' || path.at(i) == ':')
				break;
		if (i <= 0)
		{
			if (path.at(0) == '/' || path.at(0) == '\\')
				path->erase(1, path.size() - 1);
			else if (path.size() >= 2 && path.at(1) == ':')
				path.erase(2, path.size() - 2);
			else
				path = "..";
		}
		else
		{
			if (path.at(i - 1) == ':') ++i;
			path.erase(i, path.size() - i);
		}
		return path;
	}

	// otherwise, append "/" and new path component
	if (path.size() > 0 && path.back() != '/' && path.back() != '\\')
		path += '/';
	path += fileName;
	return path;

#else
	//---------- Unix ----------
	int i;

	// appending "." does nothing
	if (!strcmp(fileName, "."))
		return path;

	// appending ".." goes up one directory
	if (!strcmp(fileName, ".."))
	{
		for (i = path.size() - 2; i >= 0; --i)
			if (path.at(i) == '/')
				break;
		if (i <= 0)
			if (path.at(0) == '/')
				path.erase(1, path.size() - 1);
			else
				path = "..";
		else
			path.erase(i, path.size() - i);
		return path;
	}

	// otherwise, append "/" and new path component
	if (path.size() > 0 && path.back() != '/')
		path += '/';
	path += fileName;
	return path;
#endif
}

std::string grabPath(const char* fileName)
{
	const char* p;

#ifdef VMS
	//---------- VMS ----------
	if ((p = strrchr(fileName, ']')))
		return std::string(fileName, p + 1 - fileName);
	if ((p = strrchr(fileName, ':')))
		return std::string(fileName, p + 1 - fileName);
	return "";

#elif defined(__EMX__) || defined(_WIN32)
	//---------- OS/2+EMX and Win32 ----------
	if ((p = strrchr(fileName, '/')))
		return std::string(fileName, (int)(p - fileName));
	if ((p = strrchr(fileName, '\\')))
		return std::string(fileName, (int)(p - fileName));
	if ((p = strrchr(fileName, ':')))
		return std::string(fileName, (int)(p + 1 - fileName));
	return "";

#elif defined(ACORN)
	//---------- RISCOS ----------
	if ((p = strrchr(fileName, '.')))
		return std::string(fileName, p - fileName);
	return "";

#else
	//---------- Unix ----------
	if ((p = strrchr(fileName, '/')))
		return std::string(fileName, (int)(p - fileName));
	return "";
#endif
}

bool isAbsolutePath(const char* path)
{
#ifdef VMS
	//---------- VMS ----------
	return strchr(path, ':') || (path[0] == '[' && path[1] != '.' && path[1] != '-');

#elif defined(__EMX__) || defined(_WIN32)
	//---------- OS/2+EMX and Win32 ----------
	return path[0] == '/' || path[0] == '\\' || path[1] == ':';

#elif defined(ACORN)
	//---------- RISCOS ----------
	return path[0] == '$';

#else
	//---------- Unix ----------
	return path[0] == '/';
#endif
}

std::string makePathAbsolute(std::string path)
{
#ifdef VMS
	//---------- VMS ----------
	char buf[PATH_MAX + 1];

	if (!isAbsolutePath(path.c_str()))
	{
		if (getcwd(buf, sizeof(buf)))
			path->insert(0, buf);
	}
	return path;

#elif defined(_WIN32)
	//---------- Win32 ----------
	char  buf[MAX_PATH];
	char* fp;

	buf[0] = '\0';
	if (!GetFullPathNameA(path.c_str(), MAX_PATH, buf, &fp))
	{
		path.clear();
		return path;
	}
	path = buf;
	return path;

#elif defined(ACORN)
	//---------- RISCOS ----------
	path.insert(0, '@');
	return path;

#else
	//---------- Unix and OS/2+EMX ----------
	struct passwd* pw;
	char           buf[PATH_MAX + 1];
	const char *   p1, *p2;
	int            n;

	if (path.at(0) == '~')
	{
		if (path.at(1) == '/' ||
#	ifdef __EMX__
		    path.at(1) == '\\' ||
#	endif
		    path.size() == 1)
		{
			path.erase(0, 1);
			path.insert(0, getHomeDir());
		}
		else
		{
			p1 = path.c_str() + 1;
#	ifdef __EMX__
			for (p2 = p1; *p2 && *p2 != '/' && *p2 != '\\'; ++p2)
				;
#	else
			for (p2 = p1; *p2 && *p2 != '/'; ++p2)
				;
#	endif
			if ((n = (int)(p2 - p1)) > PATH_MAX)
				n = PATH_MAX;
			strncpy(buf, p1, n);
			buf[n] = '\0';
			if ((pw = getpwnam(buf)))
			{
				path.erase(0, (int)(p2 - p1 + 1));
				path.insert(0, pw->pw_dir);
			}
		}
	}
	else if (!isAbsolutePath(path.c_str()))
	{
		if (getcwd(buf, sizeof(buf)))
		{
#	ifndef __EMX__
			path.insert(0, "/");
#	endif
			path.insert(0, buf);
		}
	}
	return path;
#endif
}

bool pathIsFile(const char* path)
{
#ifdef _WIN32
	wchar_t wPath[winMaxLongPath + 1];
	fileNameToUCS2(path, wPath, winMaxLongPath + 1);
	DWORD attr = GetFileAttributesW(wPath);
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE));
#else
	struct stat statBuf;
	return stat(path, &statBuf) == 0 && S_ISREG(statBuf.st_mode);
#endif
}

bool pathIsDir(const char* path)
{
#ifdef _WIN32
	wchar_t wPath[winMaxLongPath + 1];
	fileNameToUCS2(path, wPath, winMaxLongPath + 1);
	DWORD attr = GetFileAttributesW(wPath);
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat statBuf;
	return stat(path, &statBuf) == 0 && S_ISDIR(statBuf.st_mode);
#endif
}

time_t getModTime(char* fileName)
{
#ifdef _WIN32
	//~ should implement this, but it's (currently) only used in xpdf
	return 0;
#else
	struct stat statBuf;

	if (stat(fileName, &statBuf))
		return 0;
	return statBuf.st_mtime;
#endif
}

bool openTempFile(std::string& name, FILE** f, const char* mode, const char* ext)
{
#if defined(_WIN32)
	//---------- Win32 ----------
	char        tempPath[MAX_PATH + 1];
	FILE*       f2;
	DWORD       n;
	std::string s;
	int         t, i;

	// this has the standard race condition problem, but I haven't found
	// a better way to generate temp file names with extensions on Windows
	n = GetTempPathA(sizeof(tempPath), tempPath);
	if (n > 0 && n <= sizeof(tempPath))
	{
		s = tempPath;
		if (tempPath[n - 1] != '\\')
			s += '\\';
	}
	else
	{
		s = ".\\";
	}
	s += fmt::format("xpdf_{}_{}_", (int)GetCurrentProcessId(), (int)GetCurrentThreadId());
	t = (int)time(nullptr);
	for (i = 0; i < 1000; ++i)
	{
		auto s2 = fmt::format("{}{}", s, t + i);
		if (ext)
			s2 += ext;
		if (!(f2 = fopen(s2.c_str(), "r")))
		{
			if (!(f2 = fopen(s2.c_str(), mode)))
				return false;
			name = s2;
			*f   = f2;
			return true;
		}
		fclose(f2);
	}
	return false;
#elif defined(VMS) || defined(__EMX__) || defined(ACORN)
	//---------- non-Unix ----------
	char* s;

	// There is a security hole here: an attacker can create a symlink
	// with this file name after the tmpnam call and before the fopen
	// call.  I will happily accept fixes to this function for non-Unix OSs.
	if (!(s = tmpnam(nullptr)))
		return false;
	name = s;
	if (ext)
		name += ext;
	if (!(*f = fopen(name.c_str(), mode)))
		return false;
	return true;
#else
	//---------- Unix ----------
	char* s;
	int   fd;

	if (ext)
	{
#	if HAVE_MKSTEMPS
		if ((s = getenv("TMPDIR")))
			name = s;
		else
			name = "/tmp";
		name += "/XXXXXX";
		name += ext;
		fd = mkstemps((char*)name.c_str(), (int)strlen(ext));
#	else
		if (!(s = tmpnam(nullptr)))
			return false;
		name = s;
		name += ext;
		fd = open(name.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#	endif
	}
	else
	{
#	if HAVE_MKSTEMP
		if ((s = getenv("TMPDIR")))
			name = s;
		else
			name = "/tmp";
		name += "/XXXXXX";
		fd = mkstemp((char*)name.c_str());
#	else  // HAVE_MKSTEMP
		if (!(s = tmpnam(nullptr)))
			return false;
		name = s;
		fd   = open(name.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#	endif // HAVE_MKSTEMP
	}
	if (fd < 0 || !(*f = fdopen(fd, mode)))
		return false;
	return true;
#endif
}

bool createDir(char* path, int mode)
{
#ifdef _WIN32
	return !_mkdir(path);
#else
	return !mkdir(path, mode);
#endif
}

bool executeCommand(char* cmd)
{
#ifdef VMS
	return system(cmd) ? true : false;
#else
	return system(cmd) ? false : true;
#endif
}

#ifdef _WIN32
std::string fileNameToUTF8(const char* path)
{
	std::string s;
	for (const char* p = path; *p; ++p)
	{
		if (*p & 0x80)
		{
			s += (char)(0xc0 | ((*p >> 6) & 0x03));
			s += (char)(0x80 | (*p & 0x3f));
		}
		else
		{
			s += *p;
		}
	}
	return s;
}

std::string fileNameToUTF8(wchar_t* path)
{
	std::string s;
	wchar_t*    p;

	for (p = path; *p; ++p)
	{
		if (*p < 0x80)
		{
			s += (char)*p;
		}
		else if (*p < 0x800)
		{
			s += (char)(0xc0 | ((*p >> 6) & 0x1f));
			s += (char)(0x80 | (*p & 0x3f));
		}
		else
		{
			s += (char)(0xe0 | ((*p >> 12) & 0x0f));
			s += (char)(0x80 | ((*p >> 6) & 0x3f));
			s += (char)(0x80 | (*p & 0x3f));
		}
	}
	return s;
}

std::string fileNameMultiByteToUTF8(char* path)
{
	wchar_t fileNameW[winMaxLongPath + 1];
	if (MultiByteToWideChar(CP_OEMCP, 0, path, -1, fileNameW, sizeof(fileNameW) / sizeof(wchar_t)))
	{
		return fileNameToUTF8(fileNameW);
	}
	else
	{
		// shouldn't happen, but just in case...
		return path;
	}
}

wchar_t* fileNameToUCS2(const char* path, wchar_t* out, size_t outSize)
{
	const char* p;
	size_t      i;

	for (p = path, i = 0; *p && i < outSize - 1; ++i)
	{
		if ((p[0] & 0xe0) == 0xc0 && p[1] && (p[1] & 0xc0) == 0x80)
		{
			out[i] = (wchar_t)(((p[0] & 0x1f) << 6) | (p[1] & 0x3f));
			p += 2;
		}
		else if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80)
		{
			out[i] = (wchar_t)(((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f));
			p += 3;
		}
		else
		{
			out[i] = (wchar_t)(p[0] & 0xff);
			p += 1;
		}
	}
	out[i] = (wchar_t)0;
	return out;
}
#endif

FILE* openFile(const char* path, const char* mode)
{
#if defined(_WIN32)
	wchar_t wPath[winMaxLongPath + 1];
	wchar_t wMode[8];
	int     i;

	fileNameToUCS2(path, wPath, winMaxLongPath + 1);
	for (i = 0; mode[i] && i < sizeof(wMode) / sizeof(wchar_t) - 1; ++i)
		wMode[i] = (wchar_t)(mode[i] & 0xff);
	wMode[i] = (wchar_t)0;
	readWindowsShortcut(wPath, winMaxLongPath + 1);
	return _wfopen(wPath, wMode);
#elif defined(VMS)
	return fopen(path, mode, "ctx=stm");
#else
	return fopen(path, mode);
#endif
}

#ifdef _WIN32
void readWindowsShortcut(wchar_t* wPath, size_t wPathSize)
{
	size_t n = wcslen(wPath);
	if (n < 4 || wcscmp(wPath + n - 4, L".lnk"))
		return;
	IShellLinkW* shellLink;
	HRESULT      hres;
	hres              = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&shellLink);
	bool needCoUninit = false;
	if (hres == CO_E_NOTINITIALIZED)
	{
		CoInitialize(nullptr);
		needCoUninit = true;
		hres         = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&shellLink);
	}
	if (FAILED(hres))
		return;
	IPersistFile* persistFile;
	hres = shellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&persistFile);
	if (FAILED(hres))
		return;
	hres = persistFile->Load(wPath, STGM_READ);
	if (FAILED(hres))
	{
		fprintf(stderr, "IPersistFile.Load failed: 0x%08lx\n", hres);
		exit(1);
	}
	wchar_t target[winMaxLongPath + 1];
	hres = shellLink->GetPath(target, winMaxLongPath + 1, nullptr, 0);
	if (FAILED(hres))
		return;
	shellLink->Release();
	if (needCoUninit)
		CoUninitialize();
	if (wcslen(target) > wPathSize - 1)
		return;
	wcscpy(wPath, target);
}
#endif

int makeDir(const char* path, int mode)
{
#ifdef _WIN32
	wchar_t wPath[winMaxLongPath + 1];
	return _wmkdir(fileNameToUCS2(path, wPath, winMaxLongPath + 1));
#else
	return mkdir(path, (mode_t)mode);
#endif
}

char* getLine(char* buf, int size, FILE* f)
{
	int c, i;

	i = 0;
	while (i < size - 1)
	{
		if ((c = fgetc(f)) == EOF)
			break;
		buf[i++] = (char)c;
		if (c == '\x0a')
			break;
		if (c == '\x0d')
		{
			c = fgetc(f);
			if (c == '\x0a' && i < size - 1)
				buf[i++] = (char)c;
			else if (c != EOF)
				ungetc(c, f);
			break;
		}
	}
	buf[i] = '\0';
	if (i == 0) return nullptr;
	return buf;
}

int gfseek(FILE* f, GFileOffset offset, int whence)
{
#if HAVE_FSEEKO
	return fseeko(f, offset, whence);
#elif HAVE_FSEEK64
	return fseek64(f, offset, whence);
#elif HAVE_FSEEKI64
	return _fseeki64(f, offset, whence);
#else
	return fseek(f, offset, whence);
#endif
}

GFileOffset gftell(FILE* f)
{
#if HAVE_FSEEKO
	return ftello(f);
#elif HAVE_FSEEK64
	return ftell64(f);
#elif HAVE_FSEEKI64
	return _ftelli64(f);
#else
	return ftell(f);
#endif
}

void fixCommandLine(int* argc, char** argv[])
{
#ifdef _WIN32
	int       argcw;
	wchar_t** argvw;
	int       i;

	argvw = CommandLineToArgvW(GetCommandLineW(), &argcw);
	if (!argvw || argcw < 0)
		return;

	*argc = argcw;

	*argv = (char**)gmallocn(argcw + 1, sizeof(char*));
	for (i = 0; i < argcw; ++i)
	{
		const auto arg = fileNameToUTF8(argvw[i]);
		(*argv)[i]     = copyString(arg.c_str());
	}
	(*argv)[argcw] = nullptr;

	LocalFree(argvw);
#endif
}
