//========================================================================
//
// Decrypt.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "Decrypt.h"

static void aes256KeyExpansion(DecryptAES256State* s, uint8_t* objKey, int objKeyLen);
static void aes256DecryptBlock(DecryptAES256State* s, uint8_t* in, bool last);
static void sha256(uint8_t* msg, int msgLen, uint8_t* hash);
static void sha384(uint8_t* msg, int msgLen, uint8_t* hash);
static void sha512(uint8_t* msg, int msgLen, uint8_t* hash);

static uint8_t passwordPad[32] = {
	0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41,
	0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
	0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80,
	0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

//------------------------------------------------------------------------
// Decrypt
//------------------------------------------------------------------------

bool Decrypt::makeFileKey(int encVersion, int encRevision, int keyLength, const std::string& ownerKey, const std::string& userKey, const std::string& ownerEnc, const std::string& userEnc, int permissions, const std::string& fileID, const std::string& ownerPassword, const std::string& userPassword, uint8_t* fileKey, bool encryptMetadata, bool* ownerPasswordOk)
{
	DecryptAES256State state;
	uint8_t            test[127 + 56], test2[32];
	const char*        userPW;
	uint8_t            fState[256];
	uint8_t            tmpKey[16];
	uint8_t            fx, fy;

	*ownerPasswordOk = false;

	if (encRevision == 5 || encRevision == 6)
	{
		// check the owner password
		if (int len = TO_INT(ownerPassword.size()))
		{
			//~ this is supposed to convert the password to UTF-8 using "SASLprep"
			if (len > 127)
				len = 127;
			memcpy(test, ownerPassword.c_str(), len);
			memcpy(test + len, ownerKey.c_str() + 32, 8);
			memcpy(test + len + 8, userKey.c_str(), 48);
			sha256(test, len + 56, test);
			if (encRevision == 6)
				r6Hash(test, 32, ownerPassword.c_str(), len, userKey.c_str());
			if (!memcmp(test, ownerKey.c_str(), 32))
			{
				// compute the file key from the owner password
				memcpy(test, ownerPassword.c_str(), len);
				memcpy(test + len, ownerKey.c_str() + 40, 8);
				memcpy(test + len + 8, userKey.c_str(), 48);
				sha256(test, len + 56, test);
				if (encRevision == 6)
					r6Hash(test, 32, ownerPassword.c_str(), len, userKey.c_str());
				aes256KeyExpansion(&state, test, 32);
				for (int i = 0; i < 16; ++i)
					state.cbc[i] = 0;
				aes256DecryptBlock(&state, (uint8_t*)ownerEnc.c_str(), false);
				memcpy(fileKey, state.buf, 16);
				aes256DecryptBlock(&state, (uint8_t*)ownerEnc.c_str() + 16, false);
				memcpy(fileKey + 16, state.buf, 16);

				*ownerPasswordOk = true;
				return true;
			}
		}

		// check the user password
		int len = TO_INT(userPassword.size());
		if (len)
		{
			//~ this is supposed to convert the password to UTF-8 using "SASLprep"
			userPW  = userPassword.c_str();
			int len = TO_INT(userPassword.size());
			if (len > 127) len = 127;
		}
		else
		{
			userPW = "";
		}
		memcpy(test, userPW, len);
		memcpy(test + len, userKey.c_str() + 32, 8);
		sha256(test, len + 8, test);
		if (encRevision == 6)
			r6Hash(test, 32, userPW, len, nullptr);
		if (!memcmp(test, userKey.c_str(), 32))
		{
			// compute the file key from the user password
			memcpy(test, userPW, len);
			memcpy(test + len, userKey.c_str() + 40, 8);
			sha256(test, len + 8, test);
			if (encRevision == 6)
				r6Hash(test, 32, userPW, len, nullptr);
			aes256KeyExpansion(&state, test, 32);
			for (int i = 0; i < 16; ++i)
				state.cbc[i] = 0;
			aes256DecryptBlock(&state, (uint8_t*)userEnc.c_str(), false);
			memcpy(fileKey, state.buf, 16);
			aes256DecryptBlock(&state, (uint8_t*)userEnc.c_str() + 16, false);
			memcpy(fileKey + 16, state.buf, 16);

			return true;
		}

		return false;
	}
	else
	{
		// try using the supplied owner password to generate the user password
		if (int len = TO_INT(ownerPassword.size()))
		{
			if (len < 32)
			{
				memcpy(test, ownerPassword.c_str(), len);
				memcpy(test + len, passwordPad, 32 - len);
			}
			else
			{
				memcpy(test, ownerPassword.c_str(), 32);
			}
			md5(test, 32, test);
			if (encRevision == 3)
				for (int i = 0; i < 50; ++i)
					md5(test, keyLength, test);
			if (encRevision == 2)
			{
				rc4InitKey(test, keyLength, fState);
				fx = fy = 0;
				for (int i = 0; i < 32; ++i)
					test2[i] = rc4DecryptByte(fState, &fx, &fy, ownerKey.at(i));
			}
			else
			{
				memcpy(test2, ownerKey.c_str(), 32);
				for (int i = 19; i >= 0; --i)
				{
					for (int j = 0; j < keyLength; ++j)
						tmpKey[j] = (uint8_t)(test[j] ^ i);
					rc4InitKey(tmpKey, keyLength, fState);
					fx = fy = 0;
					for (int j = 0; j < 32; ++j)
						test2[j] = rc4DecryptByte(fState, &fx, &fy, test2[j]);
				}
			}
			const auto userPassword2 = std::string((char*)test2, 32);
			if (makeFileKey2(encVersion, encRevision, keyLength, ownerKey, userKey, permissions, fileID, userPassword2, fileKey, encryptMetadata))
			{
				*ownerPasswordOk = true;
				return true;
			}
		}

		// try using the supplied user password
		return makeFileKey2(encVersion, encRevision, keyLength, ownerKey, userKey, permissions, fileID, userPassword, fileKey, encryptMetadata);
	}
}

void Decrypt::r6Hash(uint8_t* key, int keyLen, const char* pwd, int pwdLen, const char* userKey)
{
	uint8_t         key1[64 * (127 + 64 + 48)];
	DecryptAESState state128;

	int i = 0;
	while (1)
	{
		memcpy(key1, pwd, pwdLen);
		memcpy(key1 + pwdLen, key, keyLen);
		int n = pwdLen + keyLen;
		if (userKey)
		{
			memcpy(key1 + pwdLen + keyLen, userKey, 48);
			n += 48;
		}
		for (int j = 1; j < 64; ++j)
			memcpy(key1 + j * n, key1, n);
		n *= 64;
		aesKeyExpansion(&state128, key, 16, false);
		for (int j = 0; j < 16; ++j)
			state128.cbc[j] = key[16 + j];
		for (int j = 0; j < n; j += 16)
		{
			aesEncryptBlock(&state128, key1 + j);
			memcpy(key1 + j, state128.buf, 16);
		}
		int k = 0;
		for (int j = 0; j < 16; ++j)
			k += key1[j] % 3;
		k %= 3;
		switch (k)
		{
		case 0:
			sha256(key1, n, key);
			keyLen = 32;
			break;
		case 1:
			sha384(key1, n, key);
			keyLen = 48;
			break;
		case 2:
			sha512(key1, n, key);
			keyLen = 64;
			break;
		}
		// from the spec, it appears that i should be incremented after
		// the test, but that doesn't match what Adobe does
		++i;
		if (i >= 64 && key1[n - 1] <= i - 32)
			break;
	}
}

bool Decrypt::makeFileKey2(int encVersion, int encRevision, int keyLength, const std::string& ownerKey, const std::string& userKey, int permissions, const std::string& fileID, const std::string& userPassword, uint8_t* fileKey, bool encryptMetadata)
{
	uint8_t* buf;
	uint8_t  test[32];
	uint8_t  fState[256];
	uint8_t  tmpKey[16];
	uint8_t  fx, fy;
	bool     ok;

	// generate file key
	buf = (uint8_t*)gmalloc(72 + TO_INT(fileID.size()));
	if (int len = TO_INT(userPassword.size()))
	{
		if (len < 32)
		{
			memcpy(buf, userPassword.c_str(), len);
			memcpy(buf + len, passwordPad, 32 - len);
		}
		else
		{
			memcpy(buf, userPassword.c_str(), 32);
		}
	}
	else
	{
		memcpy(buf, passwordPad, 32);
	}
	memcpy(buf + 32, ownerKey.c_str(), 32);
	buf[64] = (uint8_t)(permissions & 0xff);
	buf[65] = (uint8_t)((permissions >> 8) & 0xff);
	buf[66] = (uint8_t)((permissions >> 16) & 0xff);
	buf[67] = (uint8_t)((permissions >> 24) & 0xff);
	memcpy(buf + 68, fileID.c_str(), fileID.size());
	int len = 68 + TO_INT(fileID.size());
	if (!encryptMetadata)
	{
		buf[len++] = 0xff;
		buf[len++] = 0xff;
		buf[len++] = 0xff;
		buf[len++] = 0xff;
	}
	md5(buf, len, fileKey);
	if (encRevision == 3)
		for (int i = 0; i < 50; ++i)
			md5(fileKey, keyLength, fileKey);

	// test user password
	if (encRevision == 2)
	{
		rc4InitKey(fileKey, keyLength, fState);
		fx = fy = 0;
		for (int i = 0; i < 32; ++i)
			test[i] = rc4DecryptByte(fState, &fx, &fy, userKey.at(i));
		ok = memcmp(test, passwordPad, 32) == 0;
	}
	else if (encRevision == 3)
	{
		memcpy(test, userKey.c_str(), 32);
		for (int i = 19; i >= 0; --i)
		{
			for (int j = 0; j < keyLength; ++j)
				tmpKey[j] = (uint8_t)(fileKey[j] ^ i);
			rc4InitKey(tmpKey, keyLength, fState);
			fx = fy = 0;
			for (int j = 0; j < 32; ++j)
				test[j] = rc4DecryptByte(fState, &fx, &fy, test[j]);
		}
		memcpy(buf, passwordPad, 32);
		memcpy(buf + 32, fileID.c_str(), fileID.size());
		md5(buf, 32 + TO_INT(fileID.size()), buf);
		ok = memcmp(test, buf, 16) == 0;
	}
	else
	{
		ok = false;
	}

	gfree(buf);
	return ok;
}

//------------------------------------------------------------------------
// DecryptStream
//------------------------------------------------------------------------

DecryptStream::DecryptStream(Stream* strA, uint8_t* fileKeyA, CryptAlgorithm algoA, int keyLengthA, int objNumA, int objGenA)
    : FilterStream(strA)
{
	memcpy(fileKey, fileKeyA, keyLengthA);
	algo      = algoA;
	keyLength = keyLengthA;
	objNum    = objNumA;
	objGen    = objGenA;

	// construct object key
	for (int i = 0; i < keyLength; ++i)
		objKey[i] = fileKey[i];
	switch (algo)
	{
	case cryptRC4:
		objKey[keyLength]     = (uint8_t)(objNum & 0xff);
		objKey[keyLength + 1] = (uint8_t)((objNum >> 8) & 0xff);
		objKey[keyLength + 2] = (uint8_t)((objNum >> 16) & 0xff);
		objKey[keyLength + 3] = (uint8_t)(objGen & 0xff);
		objKey[keyLength + 4] = (uint8_t)((objGen >> 8) & 0xff);
		md5(objKey, keyLength + 5, objKey);
		if ((objKeyLength = keyLength + 5) > 16)
			objKeyLength = 16;
		break;
	case cryptAES:
		objKey[keyLength]     = (uint8_t)(objNum & 0xff);
		objKey[keyLength + 1] = (uint8_t)((objNum >> 8) & 0xff);
		objKey[keyLength + 2] = (uint8_t)((objNum >> 16) & 0xff);
		objKey[keyLength + 3] = (uint8_t)(objGen & 0xff);
		objKey[keyLength + 4] = (uint8_t)((objGen >> 8) & 0xff);
		objKey[keyLength + 5] = 0x73; // 's'
		objKey[keyLength + 6] = 0x41; // 'A'
		objKey[keyLength + 7] = 0x6c; // 'l'
		objKey[keyLength + 8] = 0x54; // 'T'
		md5(objKey, keyLength + 9, objKey);
		if ((objKeyLength = keyLength + 5) > 16)
			objKeyLength = 16;
		break;
	case cryptAES256:
		objKeyLength = keyLength;
		break;
	}
}

DecryptStream::~DecryptStream()
{
	delete str;
}

Stream* DecryptStream::copy()
{
	return new DecryptStream(str->copy(), fileKey, algo, keyLength, objNum, objGen);
}

void DecryptStream::reset()
{
	str->reset();
	switch (algo)
	{
	case cryptRC4:
		state.rc4.x = state.rc4.y = 0;
		rc4InitKey(objKey, objKeyLength, state.rc4.state);
		state.rc4.buf = EOF;
		break;
	case cryptAES:
		aesKeyExpansion(&state.aes, objKey, objKeyLength, true);
		str->getBlock((char*)state.aes.cbc, 16);
		state.aes.bufIdx = 16;
		break;
	case cryptAES256:
		aes256KeyExpansion(&state.aes256, objKey, objKeyLength);
		str->getBlock((char*)state.aes256.cbc, 16);
		state.aes256.bufIdx = 16;
		break;
	}
}

int DecryptStream::getChar()
{
	uint8_t in[16];
	int     c;

	c = EOF; // make gcc happy
	switch (algo)
	{
	case cryptRC4:
		if (state.rc4.buf == EOF)
		{
			c = str->getChar();
			if (c != EOF)
				state.rc4.buf = rc4DecryptByte(state.rc4.state, &state.rc4.x, &state.rc4.y, (uint8_t)c);
		}
		c             = state.rc4.buf;
		state.rc4.buf = EOF;
		break;
	case cryptAES:
		if (state.aes.bufIdx == 16)
		{
			if (str->getBlock((char*)in, 16) != 16)
				return EOF;
			aesDecryptBlock(&state.aes, in, str->lookChar() == EOF);
		}
		if (state.aes.bufIdx == 16)
			c = EOF;
		else
			c = state.aes.buf[state.aes.bufIdx++];
		break;
	case cryptAES256:
		if (state.aes256.bufIdx == 16)
		{
			if (str->getBlock((char*)in, 16) != 16)
				return EOF;
			aes256DecryptBlock(&state.aes256, in, str->lookChar() == EOF);
		}
		if (state.aes256.bufIdx == 16)
			c = EOF;
		else
			c = state.aes256.buf[state.aes256.bufIdx++];
		break;
	}
	return c;
}

int DecryptStream::lookChar()
{
	uint8_t in[16];
	int     c;

	c = EOF; // make gcc happy
	switch (algo)
	{
	case cryptRC4:
		if (state.rc4.buf == EOF)
		{
			c = str->getChar();
			if (c != EOF)
				state.rc4.buf = rc4DecryptByte(state.rc4.state, &state.rc4.x, &state.rc4.y, (uint8_t)c);
		}
		c = state.rc4.buf;
		break;
	case cryptAES:
		if (state.aes.bufIdx == 16)
		{
			if (str->getBlock((char*)in, 16) != 16)
				return EOF;
			aesDecryptBlock(&state.aes, in, str->lookChar() == EOF);
		}
		if (state.aes.bufIdx == 16)
			c = EOF;
		else
			c = state.aes.buf[state.aes.bufIdx];
		break;
	case cryptAES256:
		if (state.aes256.bufIdx == 16)
		{
			if (str->getBlock((char*)in, 16) != 16)
				return EOF;
			aes256DecryptBlock(&state.aes256, in, str->lookChar() == EOF);
		}
		if (state.aes256.bufIdx == 16)
			c = EOF;
		else
			c = state.aes256.buf[state.aes256.bufIdx];
		break;
	}
	return c;
}

bool DecryptStream::isBinary(bool last)
{
	return str->isBinary(last);
}

//------------------------------------------------------------------------
// RC4-compatible decryption
//------------------------------------------------------------------------

void rc4InitKey(uint8_t* key, int keyLen, uint8_t* state)
{
	uint8_t index1, index2;
	uint8_t t;

	for (int i = 0; i < 256; ++i)
		state[i] = (uint8_t)i;
	index1 = index2 = 0;
	for (int i = 0; i < 256; ++i)
	{
		index2        = (uint8_t)(key[index1] + state[i] + index2);
		t             = state[i];
		state[i]      = state[index2];
		state[index2] = t;
		index1        = (uint8_t)((index1 + 1) % keyLen);
	}
}

uint8_t rc4DecryptByte(uint8_t* state, uint8_t* x, uint8_t* y, uint8_t c)
{
	uint8_t x1, y1, tx, ty;

	x1 = *x = (uint8_t)(*x + 1);
	y1 = *y   = (uint8_t)(state[*x] + *y);
	tx        = state[x1];
	ty        = state[y1];
	state[x1] = ty;
	state[y1] = tx;
	return c ^ state[(tx + ty) % 256];
}

//------------------------------------------------------------------------
// AES decryption
//------------------------------------------------------------------------

static uint8_t sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static uint8_t invSbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static uint32_t rcon[11] = {
	0x00000000, // unused
	0x01000000,
	0x02000000,
	0x04000000,
	0x08000000,
	0x10000000,
	0x20000000,
	0x40000000,
	0x80000000,
	0x1b000000,
	0x36000000
};

static inline uint32_t subWord(uint32_t x)
{
	return (sbox[x >> 24] << 24)
	    | (sbox[(x >> 16) & 0xff] << 16)
	    | (sbox[(x >> 8) & 0xff] << 8)
	    | sbox[x & 0xff];
}

static inline uint32_t rotWord(uint32_t x)
{
	return ((x << 8) & 0xffffffff) | (x >> 24);
}

static inline void subBytes(uint8_t* state)
{
	for (int i = 0; i < 16; ++i)
		state[i] = sbox[state[i]];
}

static inline void invSubBytes(uint8_t* state)
{
	for (int i = 0; i < 16; ++i)
		state[i] = invSbox[state[i]];
}

static inline void shiftRows(uint8_t* state)
{
	uint8_t t;

	t        = state[4];
	state[4] = state[5];
	state[5] = state[6];
	state[6] = state[7];
	state[7] = t;

	t         = state[8];
	state[8]  = state[10];
	state[10] = t;
	t         = state[9];
	state[9]  = state[11];
	state[11] = t;

	t         = state[15];
	state[15] = state[14];
	state[14] = state[13];
	state[13] = state[12];
	state[12] = t;
}

static inline void invShiftRows(uint8_t* state)
{
	uint8_t t;

	t        = state[7];
	state[7] = state[6];
	state[6] = state[5];
	state[5] = state[4];
	state[4] = t;

	t         = state[8];
	state[8]  = state[10];
	state[10] = t;
	t         = state[9];
	state[9]  = state[11];
	state[11] = t;

	t         = state[12];
	state[12] = state[13];
	state[13] = state[14];
	state[14] = state[15];
	state[15] = t;
}

// {02} \cdot s
static inline uint8_t mul02(uint8_t s)
{
	uint8_t s2;

	s2 = (uint8_t)((s & 0x80) ? ((s << 1) ^ 0x1b) : (s << 1));
	return s2;
}

// {03} \cdot s
static inline uint8_t mul03(uint8_t s)
{
	uint8_t s2;

	s2 = (uint8_t)((s & 0x80) ? ((s << 1) ^ 0x1b) : (s << 1));
	return s ^ s2;
}

// {09} \cdot s
static inline uint8_t mul09(uint8_t s)
{
	uint8_t s2, s4, s8;

	s2 = (uint8_t)((s & 0x80) ? ((s << 1) ^ 0x1b) : (s << 1));
	s4 = (uint8_t)((s2 & 0x80) ? ((s2 << 1) ^ 0x1b) : (s2 << 1));
	s8 = (uint8_t)((s4 & 0x80) ? ((s4 << 1) ^ 0x1b) : (s4 << 1));
	return s ^ s8;
}

// {0b} \cdot s
static inline uint8_t mul0b(uint8_t s)
{
	uint8_t s2, s4, s8;

	s2 = (uint8_t)((s & 0x80) ? ((s << 1) ^ 0x1b) : (s << 1));
	s4 = (uint8_t)((s2 & 0x80) ? ((s2 << 1) ^ 0x1b) : (s2 << 1));
	s8 = (uint8_t)((s4 & 0x80) ? ((s4 << 1) ^ 0x1b) : (s4 << 1));
	return s ^ s2 ^ s8;
}

// {0d} \cdot s
static inline uint8_t mul0d(uint8_t s)
{
	uint8_t s2, s4, s8;

	s2 = (uint8_t)((s & 0x80) ? ((s << 1) ^ 0x1b) : (s << 1));
	s4 = (uint8_t)((s2 & 0x80) ? ((s2 << 1) ^ 0x1b) : (s2 << 1));
	s8 = (uint8_t)((s4 & 0x80) ? ((s4 << 1) ^ 0x1b) : (s4 << 1));
	return s ^ s4 ^ s8;
}

// {0e} \cdot s
static inline uint8_t mul0e(uint8_t s)
{
	uint8_t s2, s4, s8;

	s2 = (uint8_t)((s & 0x80) ? ((s << 1) ^ 0x1b) : (s << 1));
	s4 = (uint8_t)((s2 & 0x80) ? ((s2 << 1) ^ 0x1b) : (s2 << 1));
	s8 = (uint8_t)((s4 & 0x80) ? ((s4 << 1) ^ 0x1b) : (s4 << 1));
	return s2 ^ s4 ^ s8;
}

static inline void mixColumns(uint8_t* state)
{
	int     c;
	uint8_t s0, s1, s2, s3;

	for (c = 0; c < 4; ++c)
	{
		s0            = state[c];
		s1            = state[4 + c];
		s2            = state[8 + c];
		s3            = state[12 + c];
		state[c]      = mul02(s0) ^ mul03(s1) ^ s2 ^ s3;
		state[4 + c]  = s0 ^ mul02(s1) ^ mul03(s2) ^ s3;
		state[8 + c]  = s0 ^ s1 ^ mul02(s2) ^ mul03(s3);
		state[12 + c] = mul03(s0) ^ s1 ^ s2 ^ mul02(s3);
	}
}

static inline void invMixColumns(uint8_t* state)
{
	int     c;
	uint8_t s0, s1, s2, s3;

	for (c = 0; c < 4; ++c)
	{
		s0            = state[c];
		s1            = state[4 + c];
		s2            = state[8 + c];
		s3            = state[12 + c];
		state[c]      = mul0e(s0) ^ mul0b(s1) ^ mul0d(s2) ^ mul09(s3);
		state[4 + c]  = mul09(s0) ^ mul0e(s1) ^ mul0b(s2) ^ mul0d(s3);
		state[8 + c]  = mul0d(s0) ^ mul09(s1) ^ mul0e(s2) ^ mul0b(s3);
		state[12 + c] = mul0b(s0) ^ mul0d(s1) ^ mul09(s2) ^ mul0e(s3);
	}
}

static inline void invMixColumnsW(uint32_t* w)
{
	int     c;
	uint8_t s0, s1, s2, s3;

	for (c = 0; c < 4; ++c)
	{
		s0   = (uint8_t)(w[c] >> 24);
		s1   = (uint8_t)(w[c] >> 16);
		s2   = (uint8_t)(w[c] >> 8);
		s3   = (uint8_t)w[c];
		w[c] = ((mul0e(s0) ^ mul0b(s1) ^ mul0d(s2) ^ mul09(s3)) << 24)
		    | ((mul09(s0) ^ mul0e(s1) ^ mul0b(s2) ^ mul0d(s3)) << 16)
		    | ((mul0d(s0) ^ mul09(s1) ^ mul0e(s2) ^ mul0b(s3)) << 8)
		    | (mul0b(s0) ^ mul0d(s1) ^ mul09(s2) ^ mul0e(s3));
	}
}

static inline void addRoundKey(uint8_t* state, uint32_t* w)
{
	int c;

	for (c = 0; c < 4; ++c)
	{
		state[c] ^= (uint8_t)(w[c] >> 24);
		state[4 + c] ^= (uint8_t)(w[c] >> 16);
		state[8 + c] ^= (uint8_t)(w[c] >> 8);
		state[12 + c] ^= (uint8_t)w[c];
	}
}

void aesKeyExpansion(DecryptAESState* s, uint8_t* objKey, int objKeyLen, bool decrypt)
{
	uint32_t temp;
	int      i, round;

	//~ this assumes objKeyLen == 16

	for (i = 0; i < 4; ++i)
		s->w[i] = (objKey[4 * i] << 24) + (objKey[4 * i + 1] << 16) + (objKey[4 * i + 2] << 8) + objKey[4 * i + 3];
	for (i = 4; i < 44; ++i)
	{
		temp = s->w[i - 1];
		if (!(i & 3))
			temp = subWord(rotWord(temp)) ^ rcon[i / 4];
		s->w[i] = s->w[i - 4] ^ temp;
	}
	if (decrypt)
		for (round = 1; round <= 9; ++round)
			invMixColumnsW(&s->w[round * 4]);
}

void aesEncryptBlock(DecryptAESState* s, uint8_t* in)
{
	int c, round;

	// initial state + CBC
	for (c = 0; c < 4; ++c)
	{
		s->state[c]      = in[4 * c] ^ s->cbc[4 * c];
		s->state[4 + c]  = in[4 * c + 1] ^ s->cbc[4 * c + 1];
		s->state[8 + c]  = in[4 * c + 2] ^ s->cbc[4 * c + 2];
		s->state[12 + c] = in[4 * c + 3] ^ s->cbc[4 * c + 3];
	}

	// round 0
	addRoundKey(s->state, &s->w[0]);

	// rounds 1 .. 9
	for (round = 1; round <= 9; ++round)
	{
		subBytes(s->state);
		shiftRows(s->state);
		mixColumns(s->state);
		addRoundKey(s->state, &s->w[round * 4]);
	}

	// round 10
	subBytes(s->state);
	shiftRows(s->state);
	addRoundKey(s->state, &s->w[10 * 4]);

	// output + save for next CBC
	for (c = 0; c < 4; ++c)
	{
		s->buf[4 * c] = s->cbc[4 * c] = s->state[c];
		s->buf[4 * c + 1] = s->cbc[4 * c + 1] = s->state[4 + c];
		s->buf[4 * c + 2] = s->cbc[4 * c + 2] = s->state[8 + c];
		s->buf[4 * c + 3] = s->cbc[4 * c + 3] = s->state[12 + c];
	}
}

void aesDecryptBlock(DecryptAESState* s, uint8_t* in, bool last)
{
	int c, round, n, i;

	// initial state
	for (c = 0; c < 4; ++c)
	{
		s->state[c]      = in[4 * c];
		s->state[4 + c]  = in[4 * c + 1];
		s->state[8 + c]  = in[4 * c + 2];
		s->state[12 + c] = in[4 * c + 3];
	}

	// round 0
	addRoundKey(s->state, &s->w[10 * 4]);

	// rounds 1-9
	for (round = 9; round >= 1; --round)
	{
		invSubBytes(s->state);
		invShiftRows(s->state);
		invMixColumns(s->state);
		addRoundKey(s->state, &s->w[round * 4]);
	}

	// round 10
	invSubBytes(s->state);
	invShiftRows(s->state);
	addRoundKey(s->state, &s->w[0]);

	// CBC
	for (c = 0; c < 4; ++c)
	{
		s->buf[4 * c]     = s->state[c] ^ s->cbc[4 * c];
		s->buf[4 * c + 1] = s->state[4 + c] ^ s->cbc[4 * c + 1];
		s->buf[4 * c + 2] = s->state[8 + c] ^ s->cbc[4 * c + 2];
		s->buf[4 * c + 3] = s->state[12 + c] ^ s->cbc[4 * c + 3];
	}

	// save the input block for the next CBC
	for (i = 0; i < 16; ++i)
		s->cbc[i] = in[i];

	// remove padding
	s->bufIdx = 0;
	if (last)
	{
		n = s->buf[15];
		if (n < 1 || n > 16)
		{ // this should never happen
			n = 16;
		}
		for (i = 15; i >= n; --i)
			s->buf[i] = s->buf[i - n];
		s->bufIdx = n;
	}
}

//------------------------------------------------------------------------
// AES-256 decryption
//------------------------------------------------------------------------

static void aes256KeyExpansion(DecryptAES256State* s, uint8_t* objKey, int objKeyLen)
{
	uint32_t temp;
	int      i, round;

	//~ this assumes objKeyLen == 32

	for (i = 0; i < 8; ++i)
		s->w[i] = (objKey[4 * i] << 24) + (objKey[4 * i + 1] << 16) + (objKey[4 * i + 2] << 8) + objKey[4 * i + 3];
	for (i = 8; i < 60; ++i)
	{
		temp = s->w[i - 1];
		if ((i & 7) == 0)
			temp = subWord(rotWord(temp)) ^ rcon[i / 8];
		else if ((i & 7) == 4)
			temp = subWord(temp);
		s->w[i] = s->w[i - 8] ^ temp;
	}
	for (round = 1; round <= 13; ++round)
		invMixColumnsW(&s->w[round * 4]);
}

static void aes256DecryptBlock(DecryptAES256State* s, uint8_t* in, bool last)
{
	int c, round, n, i;

	// initial state
	for (c = 0; c < 4; ++c)
	{
		s->state[c]      = in[4 * c];
		s->state[4 + c]  = in[4 * c + 1];
		s->state[8 + c]  = in[4 * c + 2];
		s->state[12 + c] = in[4 * c + 3];
	}

	// round 0
	addRoundKey(s->state, &s->w[14 * 4]);

	// rounds 13-1
	for (round = 13; round >= 1; --round)
	{
		invSubBytes(s->state);
		invShiftRows(s->state);
		invMixColumns(s->state);
		addRoundKey(s->state, &s->w[round * 4]);
	}

	// round 14
	invSubBytes(s->state);
	invShiftRows(s->state);
	addRoundKey(s->state, &s->w[0]);

	// CBC
	for (c = 0; c < 4; ++c)
	{
		s->buf[4 * c]     = s->state[c] ^ s->cbc[4 * c];
		s->buf[4 * c + 1] = s->state[4 + c] ^ s->cbc[4 * c + 1];
		s->buf[4 * c + 2] = s->state[8 + c] ^ s->cbc[4 * c + 2];
		s->buf[4 * c + 3] = s->state[12 + c] ^ s->cbc[4 * c + 3];
	}

	// save the input block for the next CBC
	for (i = 0; i < 16; ++i)
		s->cbc[i] = in[i];

	// remove padding
	s->bufIdx = 0;
	if (last)
	{
		n = s->buf[15];
		if (n < 1 || n > 16)
		{ // this should never happen
			n = 16;
		}
		for (i = 15; i >= n; --i)
			s->buf[i] = s->buf[i - n];
		s->bufIdx = n;
	}
}

//------------------------------------------------------------------------
// MD5 message digest
//------------------------------------------------------------------------

// this works around a bug in older Sun compilers
static inline uint32_t rotateLeft(uint32_t x, int r)
{
	x &= 0xffffffff;
	return ((x << r) | (x >> (32 - r))) & 0xffffffff;
}

static inline uint32_t md5Round1(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t Xk, int s, uint32_t Ti)
{
	return b + rotateLeft((a + ((b & c) | (~b & d)) + Xk + Ti), s);
}

static inline uint32_t md5Round2(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t Xk, int s, uint32_t Ti)
{
	return b + rotateLeft((a + ((b & d) | (c & ~d)) + Xk + Ti), s);
}

static inline uint32_t md5Round3(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t Xk, int s, uint32_t Ti)
{
	return b + rotateLeft((a + (b ^ c ^ d) + Xk + Ti), s);
}

static inline uint32_t md5Round4(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t Xk, int s, uint32_t Ti)
{
	return b + rotateLeft((a + (c ^ (b | ~d)) + Xk + Ti), s);
}

void md5Start(MD5State* state)
{
	state->a      = 0x67452301;
	state->b      = 0xefcdab89;
	state->c      = 0x98badcfe;
	state->d      = 0x10325476;
	state->bufLen = 0;
	state->msgLen = 0;
}

static void md5ProcessBlock(MD5State* state)
{
	uint32_t x[16];
	uint32_t a, b, c, d;

	for (int i = 0; i < 16; ++i)
		x[i] = state->buf[4 * i] | (state->buf[4 * i + 1] << 8) | (state->buf[4 * i + 2] << 16) | (state->buf[4 * i + 3] << 24);

	a = state->a;
	b = state->b;
	c = state->c;
	d = state->d;

	// round 1
	a = md5Round1(a, b, c, d, x[0], 7, 0xd76aa478);
	d = md5Round1(d, a, b, c, x[1], 12, 0xe8c7b756);
	c = md5Round1(c, d, a, b, x[2], 17, 0x242070db);
	b = md5Round1(b, c, d, a, x[3], 22, 0xc1bdceee);
	a = md5Round1(a, b, c, d, x[4], 7, 0xf57c0faf);
	d = md5Round1(d, a, b, c, x[5], 12, 0x4787c62a);
	c = md5Round1(c, d, a, b, x[6], 17, 0xa8304613);
	b = md5Round1(b, c, d, a, x[7], 22, 0xfd469501);
	a = md5Round1(a, b, c, d, x[8], 7, 0x698098d8);
	d = md5Round1(d, a, b, c, x[9], 12, 0x8b44f7af);
	c = md5Round1(c, d, a, b, x[10], 17, 0xffff5bb1);
	b = md5Round1(b, c, d, a, x[11], 22, 0x895cd7be);
	a = md5Round1(a, b, c, d, x[12], 7, 0x6b901122);
	d = md5Round1(d, a, b, c, x[13], 12, 0xfd987193);
	c = md5Round1(c, d, a, b, x[14], 17, 0xa679438e);
	b = md5Round1(b, c, d, a, x[15], 22, 0x49b40821);

	// round 2
	a = md5Round2(a, b, c, d, x[1], 5, 0xf61e2562);
	d = md5Round2(d, a, b, c, x[6], 9, 0xc040b340);
	c = md5Round2(c, d, a, b, x[11], 14, 0x265e5a51);
	b = md5Round2(b, c, d, a, x[0], 20, 0xe9b6c7aa);
	a = md5Round2(a, b, c, d, x[5], 5, 0xd62f105d);
	d = md5Round2(d, a, b, c, x[10], 9, 0x02441453);
	c = md5Round2(c, d, a, b, x[15], 14, 0xd8a1e681);
	b = md5Round2(b, c, d, a, x[4], 20, 0xe7d3fbc8);
	a = md5Round2(a, b, c, d, x[9], 5, 0x21e1cde6);
	d = md5Round2(d, a, b, c, x[14], 9, 0xc33707d6);
	c = md5Round2(c, d, a, b, x[3], 14, 0xf4d50d87);
	b = md5Round2(b, c, d, a, x[8], 20, 0x455a14ed);
	a = md5Round2(a, b, c, d, x[13], 5, 0xa9e3e905);
	d = md5Round2(d, a, b, c, x[2], 9, 0xfcefa3f8);
	c = md5Round2(c, d, a, b, x[7], 14, 0x676f02d9);
	b = md5Round2(b, c, d, a, x[12], 20, 0x8d2a4c8a);

	// round 3
	a = md5Round3(a, b, c, d, x[5], 4, 0xfffa3942);
	d = md5Round3(d, a, b, c, x[8], 11, 0x8771f681);
	c = md5Round3(c, d, a, b, x[11], 16, 0x6d9d6122);
	b = md5Round3(b, c, d, a, x[14], 23, 0xfde5380c);
	a = md5Round3(a, b, c, d, x[1], 4, 0xa4beea44);
	d = md5Round3(d, a, b, c, x[4], 11, 0x4bdecfa9);
	c = md5Round3(c, d, a, b, x[7], 16, 0xf6bb4b60);
	b = md5Round3(b, c, d, a, x[10], 23, 0xbebfbc70);
	a = md5Round3(a, b, c, d, x[13], 4, 0x289b7ec6);
	d = md5Round3(d, a, b, c, x[0], 11, 0xeaa127fa);
	c = md5Round3(c, d, a, b, x[3], 16, 0xd4ef3085);
	b = md5Round3(b, c, d, a, x[6], 23, 0x04881d05);
	a = md5Round3(a, b, c, d, x[9], 4, 0xd9d4d039);
	d = md5Round3(d, a, b, c, x[12], 11, 0xe6db99e5);
	c = md5Round3(c, d, a, b, x[15], 16, 0x1fa27cf8);
	b = md5Round3(b, c, d, a, x[2], 23, 0xc4ac5665);

	// round 4
	a = md5Round4(a, b, c, d, x[0], 6, 0xf4292244);
	d = md5Round4(d, a, b, c, x[7], 10, 0x432aff97);
	c = md5Round4(c, d, a, b, x[14], 15, 0xab9423a7);
	b = md5Round4(b, c, d, a, x[5], 21, 0xfc93a039);
	a = md5Round4(a, b, c, d, x[12], 6, 0x655b59c3);
	d = md5Round4(d, a, b, c, x[3], 10, 0x8f0ccc92);
	c = md5Round4(c, d, a, b, x[10], 15, 0xffeff47d);
	b = md5Round4(b, c, d, a, x[1], 21, 0x85845dd1);
	a = md5Round4(a, b, c, d, x[8], 6, 0x6fa87e4f);
	d = md5Round4(d, a, b, c, x[15], 10, 0xfe2ce6e0);
	c = md5Round4(c, d, a, b, x[6], 15, 0xa3014314);
	b = md5Round4(b, c, d, a, x[13], 21, 0x4e0811a1);
	a = md5Round4(a, b, c, d, x[4], 6, 0xf7537e82);
	d = md5Round4(d, a, b, c, x[11], 10, 0xbd3af235);
	c = md5Round4(c, d, a, b, x[2], 15, 0x2ad7d2bb);
	b = md5Round4(b, c, d, a, x[9], 21, 0xeb86d391);

	// increment a, b, c, d
	state->a += a;
	state->b += b;
	state->c += c;
	state->d += d;

	state->bufLen = 0;
}

void md5Append(MD5State* state, uint8_t* data, int dataLen)
{
	uint8_t* p;
	int      remain, k;

	p      = data;
	remain = dataLen;
	while (state->bufLen + remain >= 64)
	{
		k = 64 - state->bufLen;
		memcpy(state->buf + state->bufLen, p, k);
		state->bufLen = 64;
		md5ProcessBlock(state);
		p += k;
		remain -= k;
	}
	if (remain > 0)
	{
		memcpy(state->buf + state->bufLen, p, remain);
		state->bufLen += remain;
	}
	state->msgLen += dataLen;
}

void md5Finish(MD5State* state)
{
	// padding and length
	state->buf[state->bufLen++] = 0x80;
	if (state->bufLen > 56)
	{
		while (state->bufLen < 64)
			state->buf[state->bufLen++] = 0x00;
		md5ProcessBlock(state);
	}
	while (state->bufLen < 56)
		state->buf[state->bufLen++] = 0x00;
	state->buf[56] = (uint8_t)(state->msgLen << 3);
	state->buf[57] = (uint8_t)(state->msgLen >> 5);
	state->buf[58] = (uint8_t)(state->msgLen >> 13);
	state->buf[59] = (uint8_t)(state->msgLen >> 21);
	state->buf[60] = (uint8_t)(state->msgLen >> 29);
	state->buf[61] = (uint8_t)0;
	state->buf[62] = (uint8_t)0;
	state->buf[63] = (uint8_t)0;
	state->bufLen  = 64;
	md5ProcessBlock(state);

	// break digest into bytes
	state->digest[0]  = (uint8_t)state->a;
	state->digest[1]  = (uint8_t)(state->a >> 8);
	state->digest[2]  = (uint8_t)(state->a >> 16);
	state->digest[3]  = (uint8_t)(state->a >> 24);
	state->digest[4]  = (uint8_t)state->b;
	state->digest[5]  = (uint8_t)(state->b >> 8);
	state->digest[6]  = (uint8_t)(state->b >> 16);
	state->digest[7]  = (uint8_t)(state->b >> 24);
	state->digest[8]  = (uint8_t)state->c;
	state->digest[9]  = (uint8_t)(state->c >> 8);
	state->digest[10] = (uint8_t)(state->c >> 16);
	state->digest[11] = (uint8_t)(state->c >> 24);
	state->digest[12] = (uint8_t)state->d;
	state->digest[13] = (uint8_t)(state->d >> 8);
	state->digest[14] = (uint8_t)(state->d >> 16);
	state->digest[15] = (uint8_t)(state->d >> 24);
}

void md5(uint8_t* msg, int msgLen, uint8_t* digest)
{
	MD5State state;
	int      i;

	if (msgLen < 0)
		return;
	md5Start(&state);
	md5Append(&state, msg, msgLen);
	md5Finish(&state);
	for (i = 0; i < 16; ++i)
		digest[i] = state.digest[i];
}

//------------------------------------------------------------------------
// SHA-256 hash
//------------------------------------------------------------------------

static uint32_t sha256K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t rotr(uint32_t x, uint32_t n)
{
	return (x >> n) | (x << (32 - n));
}

static inline uint32_t sha256Ch(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (~x & z);
}

static inline uint32_t sha256Maj(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

static inline uint32_t sha256Sigma0(uint32_t x)
{
	return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static inline uint32_t sha256Sigma1(uint32_t x)
{
	return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static inline uint32_t sha256sigma0(uint32_t x)
{
	return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static inline uint32_t sha256sigma1(uint32_t x)
{
	return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

static void sha256HashBlock(uint8_t* blk, uint32_t* H)
{
	uint32_t W[64];
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t T1, T2;
	uint32_t t;

	// 1. prepare the message schedule
	for (t = 0; t < 16; ++t)
		W[t] = (blk[t * 4] << 24) | (blk[t * 4 + 1] << 16) | (blk[t * 4 + 2] << 8) | blk[t * 4 + 3];
	for (t = 16; t < 64; ++t)
		W[t] = sha256sigma1(W[t - 2]) + W[t - 7] + sha256sigma0(W[t - 15]) + W[t - 16];

	// 2. initialize the eight working variables
	a = H[0];
	b = H[1];
	c = H[2];
	d = H[3];
	e = H[4];
	f = H[5];
	g = H[6];
	h = H[7];

	// 3.
	for (t = 0; t < 64; ++t)
	{
		T1 = h + sha256Sigma1(e) + sha256Ch(e, f, g) + sha256K[t] + W[t];
		T2 = sha256Sigma0(a) + sha256Maj(a, b, c);
		h  = g;
		g  = f;
		f  = e;
		e  = d + T1;
		d  = c;
		c  = b;
		b  = a;
		a  = T1 + T2;
	}

	// 4. compute the intermediate hash value
	H[0] += a;
	H[1] += b;
	H[2] += c;
	H[3] += d;
	H[4] += e;
	H[5] += f;
	H[6] += g;
	H[7] += h;
}

static void sha256(uint8_t* msg, int msgLen, uint8_t* hash)
{
	uint8_t  blk[64];
	uint32_t H[8];
	int      blkLen, i;

	H[0] = 0x6a09e667;
	H[1] = 0xbb67ae85;
	H[2] = 0x3c6ef372;
	H[3] = 0xa54ff53a;
	H[4] = 0x510e527f;
	H[5] = 0x9b05688c;
	H[6] = 0x1f83d9ab;
	H[7] = 0x5be0cd19;

	blkLen = 0;
	for (i = 0; i + 64 <= msgLen; i += 64)
		sha256HashBlock(msg + i, H);
	blkLen = msgLen - i;
	if (blkLen > 0)
		memcpy(blk, msg + i, blkLen);

	// pad the message
	blk[blkLen++] = 0x80;
	if (blkLen > 56)
	{
		while (blkLen < 64)
			blk[blkLen++] = 0;
		sha256HashBlock(blk, H);
		blkLen = 0;
	}
	while (blkLen < 56)
		blk[blkLen++] = 0;
	blk[56] = 0;
	blk[57] = 0;
	blk[58] = 0;
	blk[59] = 0;
	blk[60] = (uint8_t)(msgLen >> 21);
	blk[61] = (uint8_t)(msgLen >> 13);
	blk[62] = (uint8_t)(msgLen >> 5);
	blk[63] = (uint8_t)(msgLen << 3);
	sha256HashBlock(blk, H);

	// copy the output into the buffer (convert words to bytes)
	for (i = 0; i < 8; ++i)
	{
		hash[i * 4]     = (uint8_t)(H[i] >> 24);
		hash[i * 4 + 1] = (uint8_t)(H[i] >> 16);
		hash[i * 4 + 2] = (uint8_t)(H[i] >> 8);
		hash[i * 4 + 3] = (uint8_t)H[i];
	}
}

//------------------------------------------------------------------------
// SHA-384 and SHA-512 hashes
//------------------------------------------------------------------------

typedef unsigned long long SHA512Uint64;

static SHA512Uint64 sha512K[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
	0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
	0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
	0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
	0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
	0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
	0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
	0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
	0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
	0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
	0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static inline SHA512Uint64 rotr64(SHA512Uint64 x, uint32_t n)
{
	return (x >> n) | (x << (64 - n));
}

static inline SHA512Uint64 sha512Ch(SHA512Uint64 x, SHA512Uint64 y, SHA512Uint64 z)
{
	return (x & y) ^ (~x & z);
}

static inline SHA512Uint64 sha512Maj(SHA512Uint64 x, SHA512Uint64 y, SHA512Uint64 z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

static inline SHA512Uint64 sha512Sigma0(SHA512Uint64 x)
{
	return rotr64(x, 28) ^ rotr64(x, 34) ^ rotr64(x, 39);
}

static inline SHA512Uint64 sha512Sigma1(SHA512Uint64 x)
{
	return rotr64(x, 14) ^ rotr64(x, 18) ^ rotr64(x, 41);
}

static inline SHA512Uint64 sha512sigma0(SHA512Uint64 x)
{
	return rotr64(x, 1) ^ rotr64(x, 8) ^ (x >> 7);
}

static inline SHA512Uint64 sha512sigma1(SHA512Uint64 x)
{
	return rotr64(x, 19) ^ rotr64(x, 61) ^ (x >> 6);
}

static void sha512HashBlock(uint8_t* blk, SHA512Uint64* H)
{
	SHA512Uint64 W[80];
	SHA512Uint64 a, b, c, d, e, f, g, h;
	SHA512Uint64 T1, T2;

	// 1. prepare the message schedule
	for (uint32_t t = 0; t < 16; ++t)
		W[t] = ((SHA512Uint64)blk[t * 8] << 56) | ((SHA512Uint64)blk[t * 8 + 1] << 48) | ((SHA512Uint64)blk[t * 8 + 2] << 40) | ((SHA512Uint64)blk[t * 8 + 3] << 32) | ((SHA512Uint64)blk[t * 8 + 4] << 24) | ((SHA512Uint64)blk[t * 8 + 5] << 16) | ((SHA512Uint64)blk[t * 8 + 6] << 8) | (SHA512Uint64)blk[t * 8 + 7];
	for (uint32_t t = 16; t < 80; ++t)
		W[t] = sha512sigma1(W[t - 2]) + W[t - 7] + sha512sigma0(W[t - 15]) + W[t - 16];

	// 2. initialize the eight working variables
	a = H[0];
	b = H[1];
	c = H[2];
	d = H[3];
	e = H[4];
	f = H[5];
	g = H[6];
	h = H[7];

	// 3.
	for (uint32_t t = 0; t < 80; ++t)
	{
		T1 = h + sha512Sigma1(e) + sha512Ch(e, f, g) + sha512K[t] + W[t];
		T2 = sha512Sigma0(a) + sha512Maj(a, b, c);
		h  = g;
		g  = f;
		f  = e;
		e  = d + T1;
		d  = c;
		c  = b;
		b  = a;
		a  = T1 + T2;
	}

	// 4. compute the intermediate hash value
	H[0] += a;
	H[1] += b;
	H[2] += c;
	H[3] += d;
	H[4] += e;
	H[5] += f;
	H[6] += g;
	H[7] += h;
}

static void sha512(uint8_t* msg, int msgLen, uint8_t* hash)
{
	uint8_t      blk[128];
	SHA512Uint64 H[8];

	H[0] = 0x6a09e667f3bcc908ULL;
	H[1] = 0xbb67ae8584caa73bULL;
	H[2] = 0x3c6ef372fe94f82bULL;
	H[3] = 0xa54ff53a5f1d36f1ULL;
	H[4] = 0x510e527fade682d1ULL;
	H[5] = 0x9b05688c2b3e6c1fULL;
	H[6] = 0x1f83d9abfb41bd6bULL;
	H[7] = 0x5be0cd19137e2179ULL;

	int blkLen = 0;
	int i;
	for (i = 0; i + 128 <= msgLen; i += 128)
		sha512HashBlock(msg + i, H);
	blkLen = msgLen - i;
	if (blkLen > 0)
		memcpy(blk, msg + i, blkLen);

	// pad the message
	blk[blkLen++] = 0x80;
	if (blkLen > 112)
	{
		while (blkLen < 128) blk[blkLen++] = 0;
		sha512HashBlock(blk, H);
		blkLen = 0;
	}
	while (blkLen < 112)
		blk[blkLen++] = 0;
	blk[112] = 0;
	blk[113] = 0;
	blk[114] = 0;
	blk[115] = 0;
	blk[116] = 0;
	blk[117] = 0;
	blk[118] = 0;
	blk[119] = 0;
	blk[120] = 0;
	blk[121] = 0;
	blk[122] = 0;
	blk[123] = 0;
	blk[124] = (uint8_t)(msgLen >> 21);
	blk[125] = (uint8_t)(msgLen >> 13);
	blk[126] = (uint8_t)(msgLen >> 5);
	blk[127] = (uint8_t)(msgLen << 3);
	sha512HashBlock(blk, H);

	// copy the output into the buffer (convert words to bytes)
	for (int i = 0; i < 8; ++i)
	{
		hash[i * 8]     = (uint8_t)(H[i] >> 56);
		hash[i * 8 + 1] = (uint8_t)(H[i] >> 48);
		hash[i * 8 + 2] = (uint8_t)(H[i] >> 40);
		hash[i * 8 + 3] = (uint8_t)(H[i] >> 32);
		hash[i * 8 + 4] = (uint8_t)(H[i] >> 24);
		hash[i * 8 + 5] = (uint8_t)(H[i] >> 16);
		hash[i * 8 + 6] = (uint8_t)(H[i] >> 8);
		hash[i * 8 + 7] = (uint8_t)H[i];
	}
}

static void sha384(uint8_t* msg, int msgLen, uint8_t* hash)
{
	uint8_t      blk[128];
	SHA512Uint64 H[8];

	H[0] = 0xcbbb9d5dc1059ed8ULL;
	H[1] = 0x629a292a367cd507ULL;
	H[2] = 0x9159015a3070dd17ULL;
	H[3] = 0x152fecd8f70e5939ULL;
	H[4] = 0x67332667ffc00b31ULL;
	H[5] = 0x8eb44a8768581511ULL;
	H[6] = 0xdb0c2e0d64f98fa7ULL;
	H[7] = 0x47b5481dbefa4fa4ULL;

	int blkLen = 0;
	int i;
	for (i = 0; i + 128 <= msgLen; i += 128)
		sha512HashBlock(msg + i, H);
	blkLen = msgLen - i;
	if (blkLen > 0)
		memcpy(blk, msg + i, blkLen);

	// pad the message
	blk[blkLen++] = 0x80;
	if (blkLen > 112)
	{
		while (blkLen < 128) blk[blkLen++] = 0;
		sha512HashBlock(blk, H);
		blkLen = 0;
	}
	while (blkLen < 112)
		blk[blkLen++] = 0;
	blk[112] = 0;
	blk[113] = 0;
	blk[114] = 0;
	blk[115] = 0;
	blk[116] = 0;
	blk[117] = 0;
	blk[118] = 0;
	blk[119] = 0;
	blk[120] = 0;
	blk[121] = 0;
	blk[122] = 0;
	blk[123] = 0;
	blk[124] = (uint8_t)(msgLen >> 21);
	blk[125] = (uint8_t)(msgLen >> 13);
	blk[126] = (uint8_t)(msgLen >> 5);
	blk[127] = (uint8_t)(msgLen << 3);
	sha512HashBlock(blk, H);

	// copy the output into the buffer (convert words to bytes)
	for (int i = 0; i < 6; ++i)
	{
		hash[i * 8]     = (uint8_t)(H[i] >> 56);
		hash[i * 8 + 1] = (uint8_t)(H[i] >> 48);
		hash[i * 8 + 2] = (uint8_t)(H[i] >> 40);
		hash[i * 8 + 3] = (uint8_t)(H[i] >> 32);
		hash[i * 8 + 4] = (uint8_t)(H[i] >> 24);
		hash[i * 8 + 5] = (uint8_t)(H[i] >> 16);
		hash[i * 8 + 6] = (uint8_t)(H[i] >> 8);
		hash[i * 8 + 7] = (uint8_t)H[i];
	}
}
