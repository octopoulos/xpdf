//========================================================================
//
// Decrypt.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"
#include "Stream.h"

//------------------------------------------------------------------------
// Decrypt
//------------------------------------------------------------------------

class Decrypt
{
public:
	// Generate a file key.
	// The <fileKey> buffer must have space for at least 16 bytes.
	// Checks <ownerPassword> and then <userPassword> and returns true if either is correct.
	// Sets <ownerPasswordOk> if the owner password was correct.
	// Either or both of the passwords may be nullptr, which is treated as an empty string.
	static bool makeFileKey(int encVersion, int encRevision, int keyLength, const std::string& ownerKey, const std::string& userKey, const std::string& ownerEnc, const std::string& userEnc, int permissions, const std::string& fileID, const std::string& ownerPassword, const std::string& userPassword, uint8_t* fileKey, bool encryptMetadata, bool* ownerPasswordOk);

private:
	static void r6Hash(uint8_t* key, int keyLen, const char* pwd, int pwdLen, const char* userKey);
	static bool makeFileKey2(int encVersion, int encRevision, int keyLength, const std::string& ownerKey, const std::string& userKey, int permissions, const std::string& fileID, const std::string& userPassword, uint8_t* fileKey, bool encryptMetadata);
};

//------------------------------------------------------------------------
// DecryptStream
//------------------------------------------------------------------------

struct DecryptRC4State
{
	uint8_t state[256];
	uint8_t x, y;
	int     buf;
};

struct DecryptAESState
{
	uint32_t w[44];
	uint8_t  state[16];
	uint8_t  cbc[16];
	uint8_t  buf[16];
	int      bufIdx;
};

struct DecryptAES256State
{
	uint32_t w[60];
	uint8_t  state[16];
	uint8_t  cbc[16];
	uint8_t  buf[16];
	int      bufIdx;
};

class DecryptStream : public FilterStream
{
public:
	DecryptStream(Stream* strA, uint8_t* fileKeyA, CryptAlgorithm algoA, int keyLengthA, int objNumA, int objGenA);
	virtual ~DecryptStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual int  getChar();
	virtual int  lookChar();
	virtual bool isBinary(bool last);

	virtual Stream* getUndecodedStream() { return this; }

private:
	uint8_t        fileKey[32];  //
	CryptAlgorithm algo;         //
	int            keyLength;    //
	int            objNum;       //
	int            objGen;       //
	int            objKeyLength; //
	uint8_t        objKey[32];   //

	union
	{
		DecryptRC4State    rc4;
		DecryptAESState    aes;
		DecryptAES256State aes256;
	} state;
};

//------------------------------------------------------------------------

struct MD5State
{
	uint32_t a, b, c, d; //
	uint8_t  buf[64];    //
	int      bufLen;     //
	int      msgLen;     //
	uint8_t  digest[16]; //
};

extern void    rc4InitKey(uint8_t* key, int keyLen, uint8_t* state);
extern uint8_t rc4DecryptByte(uint8_t* state, uint8_t* x, uint8_t* y, uint8_t c);
void           md5Start(MD5State* state);
void           md5Append(MD5State* state, uint8_t* data, int dataLen);
void           md5Finish(MD5State* state);
extern void    md5(uint8_t* msg, int msgLen, uint8_t* digest);
extern void    aesKeyExpansion(DecryptAESState* s, uint8_t* objKey, int objKeyLen, bool decrypt);
extern void    aesEncryptBlock(DecryptAESState* s, uint8_t* in);
extern void    aesDecryptBlock(DecryptAESState* s, uint8_t* in, bool last);
