//========================================================================
//
// GHash.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "gmem.h"
#include "gmempp.h"
#include "GHash.h"

//------------------------------------------------------------------------

struct GHashBucket
{
	std::string key;

	union
	{
		void* p;
		int   i;
	} val;

	GHashBucket* next;
};

struct GHashIter
{
	int          h;
	GHashBucket* p;
};

//------------------------------------------------------------------------

GHash::GHash(bool deleteKeysA)
{
	size = 7;
	tab  = (GHashBucket**)gmallocn(size, sizeof(GHashBucket*));
	for (int h = 0; h < size; ++h)
		tab[h] = nullptr;
	len = 0;
}

GHash::~GHash()
{
	GHashBucket* p;
	int          h;

	for (h = 0; h < size; ++h)
	{
		while (tab[h])
		{
			p      = tab[h];
			tab[h] = p->next;
			delete p;
		}
	}
	gfree(tab);
}

void GHash::add(const std::string& key, void* val)
{
	GHashBucket* p;
	int          h;

	// expand the table if necessary
	if (len >= size)
		expand();

	// add the new symbol
	p        = new GHashBucket;
	p->key   = key;
	p->val.p = val;
	h        = hash(key);
	p->next  = tab[h];
	tab[h]   = p;
	++len;
}

void GHash::add(const std::string& key, int val)
{
	GHashBucket* p;
	int          h;

	// expand the table if necessary
	if (len >= size)
		expand();

	// add the new symbol
	p        = new GHashBucket;
	p->key   = key;
	p->val.i = val;
	h        = hash(key);
	p->next  = tab[h];
	tab[h]   = p;
	++len;
}

void GHash::replace(const std::string& key, void* val)
{
	GHashBucket* p;
	int          h;

	if ((p = find(key, &h)))
		p->val.p = val;
	else
		add(key, val);
}

void GHash::replace(const std::string& key, int val)
{
	GHashBucket* p;
	int          h;

	if ((p = find(key, &h)))
		p->val.i = val;
	else
		add(key, val);
}

void* GHash::lookup(const std::string& key)
{
	GHashBucket* p;
	int          h;

	if (!(p = find(key, &h))) return nullptr;
	return p->val.p;
}

int GHash::lookupInt(const std::string& key)
{
	GHashBucket* p;
	int          h;

	if (!(p = find(key, &h)))
		return 0;
	return p->val.i;
}

void* GHash::lookup(const char* key)
{
	GHashBucket* p;
	int          h;

	if (!(p = find(key, &h))) return nullptr;
	return p->val.p;
}

int GHash::lookupInt(const char* key)
{
	GHashBucket* p;
	int          h;

	if (!(p = find(key, &h)))
		return 0;
	return p->val.i;
}

void* GHash::remove(const std::string& key)
{
	GHashBucket*  p;
	GHashBucket** q;
	void*         val;
	int           h;

	if (!(p = find(key, &h))) return nullptr;
	q = &tab[h];
	while (*q != p)
		q = &((*q)->next);
	*q  = p->next;
	val = p->val.p;
	delete p;
	--len;
	return val;
}

int GHash::removeInt(const std::string& key)
{
	GHashBucket*  p;
	GHashBucket** q;
	int           val;
	int           h;

	if (!(p = find(key, &h)))
		return 0;
	q = &tab[h];
	while (*q != p)
		q = &((*q)->next);
	*q  = p->next;
	val = p->val.i;
	delete p;
	--len;
	return val;
}

void* GHash::remove(const char* key)
{
	GHashBucket*  p;
	GHashBucket** q;
	void*         val;
	int           h;

	if (!(p = find(key, &h))) return nullptr;
	q = &tab[h];
	while (*q != p)
		q = &((*q)->next);
	*q  = p->next;
	val = p->val.p;
	delete p;
	--len;
	return val;
}

int GHash::removeInt(const char* key)
{
	GHashBucket*  p;
	GHashBucket** q;
	int           val;
	int           h;

	if (!(p = find(key, &h)))
		return 0;
	q = &tab[h];
	while (*q != p)
		q = &((*q)->next);
	*q  = p->next;
	val = p->val.i;
	delete p;
	--len;
	return val;
}

void GHash::startIter(GHashIter** iter)
{
	*iter      = new GHashIter;
	(*iter)->h = -1;
	(*iter)->p = nullptr;
}

bool GHash::getNext(GHashIter** iter, std::string& key, void** val)
{
	if (!*iter)
		return false;
	if ((*iter)->p)
		(*iter)->p = (*iter)->p->next;
	while (!(*iter)->p)
	{
		if (++(*iter)->h == size)
		{
			delete *iter;
			*iter = nullptr;
			return false;
		}
		(*iter)->p = tab[(*iter)->h];
	}
	key  = (*iter)->p->key;
	*val = (*iter)->p->val.p;
	return true;
}

bool GHash::getNext(GHashIter** iter, std::string& key, int* val)
{
	if (!*iter)
		return false;
	if ((*iter)->p)
		(*iter)->p = (*iter)->p->next;
	while (!(*iter)->p)
	{
		if (++(*iter)->h == size)
		{
			delete *iter;
			*iter = nullptr;
			return false;
		}
		(*iter)->p = tab[(*iter)->h];
	}
	key  = (*iter)->p->key;
	*val = (*iter)->p->val.i;
	return true;
}

void GHash::killIter(GHashIter** iter)
{
	delete *iter;
	*iter = nullptr;
}

void GHash::expand()
{
	GHashBucket** oldTab;
	GHashBucket*  p;
	int           oldSize, h, i;

	oldSize = size;
	oldTab  = tab;
	size    = 2 * size + 1;
	tab     = (GHashBucket**)gmallocn(size, sizeof(GHashBucket*));
	for (h = 0; h < size; ++h)
		tab[h] = nullptr;
	for (i = 0; i < oldSize; ++i)
	{
		while (oldTab[i])
		{
			p         = oldTab[i];
			oldTab[i] = oldTab[i]->next;
			h         = hash(p->key);
			p->next   = tab[h];
			tab[h]    = p;
		}
	}
	gfree(oldTab);
}

GHashBucket* GHash::find(const std::string& key, int* h)
{
	GHashBucket* p;

	*h = hash(key);
	for (p = tab[*h]; p; p = p->next)
		if (p->key == key) return p;

	return nullptr;
}

GHashBucket* GHash::find(const char* key, int* h)
{
	GHashBucket* p;

	*h = hash(key);
	for (p = tab[*h]; p; p = p->next)
		if (p->key == key) return p;

	return nullptr;
}

int GHash::hash(const std::string& key)
{
	const char*  p;
	unsigned int h;
	int          i;

	h = 0;
	for (p = key.c_str(), i = 0; i < key.size(); ++p, ++i)
		h = 17 * h + (int)(*p & 0xff);
	return (int)(h % size);
}

int GHash::hash(const char* key)
{
	const char*  p;
	unsigned int h;

	h = 0;
	for (p = key; *p; ++p)
		h = 17 * h + (int)(*p & 0xff);
	return (int)(h % size);
}
