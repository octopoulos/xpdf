//========================================================================
//
// xpdf.cc
//
// Copyright 1996-2105 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "gmem.h"
#include "Object.h"
#include "XpdfApp.h"
#include "gmempp.h"

int main(int argc, char* argv[])
{
	int exitCode;

	{
		// this is inside a block so that the XpdfApp object gets freed
		XpdfApp app(argc, argv);
		if (app.getNumViewers())
			exitCode = app.exec();
		else
			exitCode = 1;
	}

	Object::memCheck(stderr);
	gMemReport(stderr);

	return exitCode;
}

#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hIstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	wchar_t** args;
	int       argc, ret;

	if (!(args = CommandLineToArgvW(GetCommandLineW(), &argc)) || argc < 0)
		return -1;
	char** argv = (char**)gmallocn(argc + 1, sizeof(char*));
	for (int i = 0; i < argc; ++i)
	{
		const int n = WideCharToMultiByte(CP_UTF8, 0, args[i], -1, nullptr, 0, nullptr, nullptr);
		argv[i]     = (char*)gmalloc(n);
		WideCharToMultiByte(CP_UTF8, 0, args[i], -1, argv[i], n, nullptr, nullptr);
	}
	argv[argc] = nullptr;
	LocalFree(args);
	ret = main(argc, argv);
	for (int i = 0; i < argc; ++i)
		gfree(argv[i]);
	gfree(argv);
	return ret;
}
#endif
