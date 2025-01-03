//========================================================================
//
// XRef.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "gfile.h"
#include "Object.h"
#if MULTITHREADED
#	include "GMutex.h"
#endif

class Dict;
class Stream;
class Parser;
class ObjectStream;
class XRefPosSet;

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

enum XRefEntryType
{
	xrefEntryFree,
	xrefEntryUncompressed,
	xrefEntryCompressed
};

struct XRefEntry
{
	GFileOffset   offset; //
	int           gen;    //
	XRefEntryType type;   //
};

struct XRefCacheEntry
{
	int    num; //
	int    gen; //
	Object obj; //
};

#define xrefCacheSize 16

#define objStrCacheSize    128
#define objStrCacheTimeout 1000

class XRef
{
public:
	// Constructor.  Read xref table from stream.
	XRef(BaseStream* strA, bool repair);

	// Destructor.
	~XRef();

	// Is xref table valid?
	bool isOk() { return ok; }

	// Get the error code (if isOk() returns false).
	int getErrorCode() { return errCode; }

	// Was the xref constructed by the repair code?
	bool isRepaired() { return repaired; }

	// Set the encryption parameters.
	void setEncryption(int permFlagsA, bool ownerPasswordOkA, uint8_t* fileKeyA, int keyLengthA, int encVersionA, CryptAlgorithm encAlgorithmA);

	// Is the file encrypted?
	bool isEncrypted() { return encrypted; }

	bool getEncryption(int* permFlagsA, bool* ownerPasswordOkA, int* keyLengthA, int* encVersionA, CryptAlgorithm* encAlgorithmA);

	// Check various permissions.
	bool okToPrint(bool ignoreOwnerPW = false);
	bool okToChange(bool ignoreOwnerPW = false);
	bool okToCopy(bool ignoreOwnerPW = false);
	bool okToAddNotes(bool ignoreOwnerPW = false);

	int getPermFlags() { return permFlags; }

	// Get catalog object.
	Object* getCatalog(Object* obj) { return fetch(rootNum, rootGen, obj); }

	// Fetch an indirect reference.
	Object* fetch(int num, int gen, Object* obj, int recursion = 0);

	// Return the document's Info dictionary (if any).
	Object* getDocInfo(Object* obj);
	Object* getDocInfoNF(Object* obj);

	// Return the number of objects in the xref table.
	int getNumObjects() { return last + 1; }

	// Return the offset of the last xref table.
	GFileOffset getLastXRefPos() { return lastXRefPos; }

	// Return the offset of the 'startxref' at the end of the file.
	GFileOffset getLastStartxrefPos() { return lastStartxrefPos; }

	// Return the catalog object reference.
	int getRootNum() { return rootNum; }

	int getRootGen() { return rootGen; }

	// Get the xref table positions.
	int getNumXRefTables() { return xrefTablePosLen; }

	GFileOffset getXRefTablePos(int idx) { return xrefTablePos[idx]; }

	// Get end position for a stream in a damaged file.
	// Returns false if unknown or file is not damaged.
	bool getStreamEnd(GFileOffset streamStart, GFileOffset* streamEnd);

	// Direct access.
	int getSize() { return size; }

	XRefEntry* getEntry(int i) { return &entries[i]; }

	Object* getTrailerDict() { return &trailerDict; }

private:
	BaseStream*    str;                            // input stream
	GFileOffset    start;                          // offset in file (to allow for garbage at beginning of file)
	XRefEntry*     entries;                        // xref entries
	int            size;                           // size of <entries> array
	int            last;                           // last used index in <entries>
	int            rootNum;                        //
	int            rootGen;                        // catalog dict
	bool           ok;                             // true if xref table is valid
	int            errCode;                        // error code (if <ok> is false)
	bool           repaired;                       // set if the xref table was constructed by the repair code
	Object         trailerDict;                    // trailer dictionary
	GFileOffset    lastXRefPos;                    // offset of last xref table
	GFileOffset    lastStartxrefPos;               // offset of 'startxref' at end of file
	GFileOffset*   xrefTablePos;                   // positions of all xref tables
	int            xrefTablePosLen;                // number of xref table positions
	GFileOffset*   streamEnds;                     // 'endstream' positions - only used in damaged files
	int            streamEndsLen;                  // number of valid entries in streamEnds
	ObjectStream*  objStrs[objStrCacheSize];       // cached object streams
	int            objStrCacheLength;              // number of valid entries in objStrs[]
	uint32_t       objStrLastUse[objStrCacheSize]; // time of last use for each obj stream
	uint32_t       objStrTime;                     // current time for the obj stream cache
	bool           encrypted;                      // true if file is encrypted
	int            permFlags;                      // permission bits
	bool           ownerPasswordOk;                // true if owner password is correct
	uint8_t        fileKey[32];                    // file decryption key
	int            keyLength;                      // length of key, in bytes
	int            encVersion;                     // encryption version
	CryptAlgorithm encAlgorithm;                   // encryption algorithm
	XRefCacheEntry cache[xrefCacheSize];           // cache of recently accessed objects

#if MULTITHREADED
	GMutex objStrsMutex;
	GMutex cacheMutex;
#endif

	GFileOffset   getStartXref();
	bool          readXRef(GFileOffset* pos, XRefPosSet* posSet, bool hybrid);
	bool          readXRefTable(GFileOffset* pos, int offset, XRefPosSet* posSet);
	bool          readXRefStream(Stream* xrefStr, GFileOffset* pos, bool hybrid);
	bool          readXRefStreamSection(Stream* xrefStr, int* w, int first, int n);
	bool          constructXRef();
	void          constructTrailerDict(GFileOffset pos);
	void          saveTrailerDict(Dict* dict, bool isXRefStream);
	char*         constructObjectEntry(char* p, GFileOffset pos, int* objNum);
	void          constructObjectStreamEntries(Object* objStr, int objStrObjNum);
	bool          constructXRefEntry(int num, int gen, GFileOffset pos, XRefEntryType type);
	bool          getObjectStreamObject(int objStrNum, int objIdx, int objNum, Object* obj, int recursion);
	ObjectStream* getObjectStreamFromCache(int objStrNum);
	void          addObjectStreamToCache(ObjectStream* objStr);
	void          cleanObjectStreamCache();
	GFileOffset   strToFileOffset(char* s);
};
