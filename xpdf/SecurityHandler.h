//========================================================================
//
// SecurityHandler.h
//
// Copyright 2004 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"

class PDFDoc;
struct XpdfSecurityHandler;

//------------------------------------------------------------------------
// SecurityHandler
//------------------------------------------------------------------------

class SecurityHandler
{
public:
	static SecurityHandler* make(PDFDoc* docA, Object* encryptDictA);

	SecurityHandler(PDFDoc* docA);
	virtual ~SecurityHandler();

	// Returns true if the file is actually unencrypted.
	virtual bool isUnencrypted() { return false; }

	// Check the document's encryption.  If the document is encrypted,
	// this will first try <ownerPassword> and <userPassword> (in
	// "batch" mode), and if those fail, it will attempt to request a
	// password from the user.  This is the high-level function that
	// calls the lower level functions for the specific security handler
	// (requesting a password three times, etc.).  Returns true if the
	// document can be opened (if it's unencrypted, or if a correct
	// password is obtained); false otherwise (encrypted and no correct
	// password).
	bool checkEncryption(const std::string& ownerPassword, const std::string& userPassword);

	// Create authorization data for the specified owner and user
	// passwords.  If the security handler doesn't support "batch" mode,
	// this function should return nullptr.
	virtual void* makeAuthData(const std::string& ownerPassword, const std::string& userPassword) = 0;

	// Construct authorization data, typically by prompting the user for
	// a password.  Returns an authorization data object, or nullptr to
	// cancel.
	virtual void* getAuthData() = 0;

	// Free the authorization data returned by makeAuthData or
	// getAuthData.
	virtual void freeAuthData(void* authData) = 0;

	// Attempt to authorize the document, using the supplied
	// authorization data (which may be nullptr).  Returns true if
	// successful (i.e., if at least the right to open the document was
	// granted).
	virtual bool authorize(void* authData) = 0;

	// Return the various authorization parameters.  These are only
	// valid after authorize has returned true.
	virtual int            getPermissionFlags() = 0;
	virtual bool           getOwnerPasswordOk() = 0;
	virtual uint8_t*       getFileKey()         = 0;
	virtual int            getFileKeyLength()   = 0;
	virtual int            getEncVersion()      = 0;
	virtual CryptAlgorithm getEncAlgorithm()    = 0;

protected:
	PDFDoc* doc = nullptr; //
};

//------------------------------------------------------------------------
// StandardSecurityHandler
//------------------------------------------------------------------------

class StandardSecurityHandler : public SecurityHandler
{
public:
	StandardSecurityHandler(PDFDoc* docA, Object* encryptDictA);
	virtual ~StandardSecurityHandler();

	virtual bool  isUnencrypted();
	virtual void* makeAuthData(const std::string& ownerPassword, const std::string& userPassword);
	virtual void* getAuthData();
	virtual void  freeAuthData(void* authData);
	virtual bool  authorize(void* authData);

	virtual int getPermissionFlags() { return permFlags; }

	virtual bool getOwnerPasswordOk() { return ownerPasswordOk; }

	virtual uint8_t* getFileKey() { return fileKey; }

	virtual int getFileKeyLength() { return fileKeyLength; }

	virtual int getEncVersion() { return encVersion; }

	virtual CryptAlgorithm getEncAlgorithm() { return encAlgorithm; }

private:
	int            permFlags       = 0;        //
	bool           ownerPasswordOk = false;    //
	uint8_t        fileKey[32]     = {};       //
	int            fileKeyLength   = 0;        //
	int            encVersion      = 0;        //
	int            encRevision     = 0;        //
	CryptAlgorithm encAlgorithm    = cryptAES; //
	bool           encryptMetadata = false;    //
	std::string    ownerKey        = "";       //
	std::string    userKey         = "";       //
	std::string    ownerEnc        = "";       //
	std::string    userEnc         = "";       //
	std::string    fileID          = "";       //
	bool           ok              = false;    //
};
