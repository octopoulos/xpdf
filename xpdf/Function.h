//========================================================================
//
// Function.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"

class GList;
class Dict;
class Stream;
struct PSCode;

//------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------

#define funcMaxInputs        32
#define funcMaxOutputs       32
#define sampledFuncMaxInputs 16

class Function
{
public:
	Function();

	virtual ~Function();

	// Construct a function, with [expectedInputs] inputs and
	// [expectedOutputs] outputs.  [expectedOutputs] can be -1 to
	// indicate unknown.  Returns nullptr if unsuccessful.
	static Function* parse(Object* funcObj, int expectedInputs, int expectedOutputs, int recursion = 0);

	// Initialize the entries common to all function types.
	bool init(Dict* dict);

	virtual Function* copy() = 0;

	// Return the function type:
	//   -1 : identity
	//    0 : sampled
	//    2 : exponential
	//    3 : stitching
	//    4 : PostScript
	virtual int getType() = 0;

	// Return size of input and output tuples.
	int getInputSize() { return m; }

	int getOutputSize() { return n; }

	double getDomainMin(int i) { return domain[i][0]; }

	double getDomainMax(int i) { return domain[i][1]; }

	double getRangeMin(int i) { return range[i][0]; }

	double getRangeMax(int i) { return range[i][1]; }

	bool getHasRange() { return hasRange; }

	// Transform an input tuple into an output tuple.
	virtual void transform(double* in, double* out) = 0;

	virtual bool isOk() = 0;

protected:
	int    m, n;                     // size of input and output tuples
	double domain[funcMaxInputs][2]; // min and max values for function domain
	double range[funcMaxOutputs][2]; // min and max values for function range
	bool   hasRange;                 // set if range is defined
};

//------------------------------------------------------------------------
// IdentityFunction
//------------------------------------------------------------------------

class IdentityFunction : public Function
{
public:
	IdentityFunction(int nInputs);
	virtual ~IdentityFunction();

	virtual Function* copy() { return new IdentityFunction(m); }

	virtual int getType() { return -1; }

	virtual void transform(double* in, double* out);

	virtual bool isOk() { return true; }

private:
};

//------------------------------------------------------------------------
// SampledFunction
//------------------------------------------------------------------------

class SampledFunction : public Function
{
public:
	SampledFunction(Object* funcObj, Dict* dict);
	virtual ~SampledFunction();

	virtual Function* copy() { return new SampledFunction(this); }

	virtual int getType() { return 0; }

	virtual void transform(double* in, double* out);

	virtual bool isOk() { return ok; }

	int getSampleSize(int i) { return sampleSize[i]; }

	double getEncodeMin(int i) { return encode[i][0]; }

	double getEncodeMax(int i) { return encode[i][1]; }

	double getDecodeMin(int i) { return decode[i][0]; }

	double getDecodeMax(int i) { return decode[i][1]; }

	double* getSamples() { return samples; }

private:
	SampledFunction(SampledFunction* func);

	int     sampleSize[funcMaxInputs]; // number of samples for each domain element
	double  encode[funcMaxInputs][2];  // min and max values for domain encoder
	double  decode[funcMaxOutputs][2]; // min and max values for range decoder
	double  inputMul[funcMaxInputs];   // input multipliers
	int*    idxOffset;                 //
	double* samples;                   // the samples
	int     nSamples;                  // size of the samples array
	double* sBuf;                      // buffer for the transform function
	double  cacheIn[funcMaxInputs];    //
	double  cacheOut[funcMaxOutputs];  //
	bool    ok;                        //
};

//------------------------------------------------------------------------
// ExponentialFunction
//------------------------------------------------------------------------

class ExponentialFunction : public Function
{
public:
	ExponentialFunction(Object* funcObj, Dict* dict);
	virtual ~ExponentialFunction();

	virtual Function* copy() { return new ExponentialFunction(this); }

	virtual int getType() { return 2; }

	virtual void transform(double* in, double* out);

	virtual bool isOk() { return ok; }

	double* getC0() { return c0; }

	double* getC1() { return c1; }

	double getE() { return e; }

private:
	ExponentialFunction(ExponentialFunction* func);

	double c0[funcMaxOutputs]; //
	double c1[funcMaxOutputs]; //
	double e;                  //
	bool   ok;                 //
};

//------------------------------------------------------------------------
// StitchingFunction
//------------------------------------------------------------------------

class StitchingFunction : public Function
{
public:
	StitchingFunction(Object* funcObj, Dict* dict, int expectedInputs, int expectedOutputs, int recursion);
	virtual ~StitchingFunction();

	virtual Function* copy() { return new StitchingFunction(this); }

	virtual int getType() { return 3; }

	virtual void transform(double* in, double* out);

	virtual bool isOk() { return ok; }

	int getNumFuncs() { return k; }

	Function* getFunc(int i) { return funcs[i]; }

	double* getBounds() { return bounds; }

	double* getEncode() { return encode; }

	double* getScale() { return scale; }

private:
	StitchingFunction(StitchingFunction* func);

	int        k;      //
	Function** funcs;  //
	double*    bounds; //
	double*    encode; //
	double*    scale;  //
	bool       ok;     //
};

//------------------------------------------------------------------------
// PostScriptFunction
//------------------------------------------------------------------------

class PostScriptFunction : public Function
{
public:
	PostScriptFunction(Object* funcObj, Dict* dict);
	virtual ~PostScriptFunction();

	virtual Function* copy() { return new PostScriptFunction(this); }

	virtual int getType() { return 4; }

	virtual void transform(double* in, double* out);

	virtual bool isOk() { return ok; }

	std::string getCodeString() { return codeString; }

private:
	PostScriptFunction(PostScriptFunction* func);
	bool        parseCode(const VEC_STR& tokens, int* tokPtr, int* codePtr);
	void        addCode(int* codePtr, int op);
	void        addCodeI(int* codePtr, int op, int x);
	void        addCodeD(int* codePtr, int op, double x);
	std::string getToken(Stream* str);
	int         exec(double* stack, int sp0);

	std::string codeString;               //
	PSCode*     code;                     //
	int         codeLen;                  //
	int         codeSize;                 //
	double      cacheIn[funcMaxInputs];   //
	double      cacheOut[funcMaxOutputs]; //
	bool        ok;                       //
};
