//========================================================================
//
// GHash.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

struct GHashBucket;
struct GHashIter;

//------------------------------------------------------------------------

class GHash
{
public:
	GHash(bool deleteKeysA = false);
	~GHash();
	void  add(const std::string& key, void* val);
	void  add(const std::string& key, int val);
	void  replace(const std::string& key, void* val);
	void  replace(const std::string& key, int val);
	void* lookup(const std::string& key);
	int   lookupInt(const std::string& key);
	void* lookup(const char* key);
	int   lookupInt(const char* key);
	void* remove(const std::string& key);
	int   removeInt(const std::string& key);
	void* remove(const char* key);
	int   removeInt(const char* key);

	int getLength() { return len; }

	void startIter(GHashIter** iter);
	bool getNext(GHashIter** iter, std::string& key, void** val);
	bool getNext(GHashIter** iter, std::string& key, int* val);
	void killIter(GHashIter** iter);

private:
	void         expand();
	GHashBucket* find(const std::string& key, int* h);
	GHashBucket* find(const char* key, int* h);
	int          hash(const std::string& key);
	int          hash(const char* key);

	int           size; // number of buckets
	int           len;  // number of entries
	GHashBucket** tab;
};

#define deleteGHash(hash, T)                           \
	do {                                               \
		GHash* _hash = (hash);                         \
		{                                              \
			GHashIter*  _iter;                         \
			std::string _key;                          \
			void*       _p;                            \
			_hash->startIter(&_iter);                  \
			while (_hash->getNext(&_iter, &_key, &_p)) \
			{                                          \
				delete (T*)_p;                         \
			}                                          \
			delete _hash;                              \
		}                                              \
	}                                                  \
	while (0)
