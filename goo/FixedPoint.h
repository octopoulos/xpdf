//========================================================================
//
// FixedPoint.h
//
// Fixed point type, with C++ operators.
//
// Copyright 2004 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

#if USE_FIXEDPOINT
#	include <stdio.h>
#	include <stdlib.h>

#	define fixptShift 16
#	define fixptMaskL ((1 << fixptShift) - 1)
#	define fixptMaskH (~fixptMaskL)

// typedef long long FixPtInt64;

class FixedPoint
{
public:
	FixedPoint() { val = 0; }

	FixedPoint(const FixedPoint& x) { val = x.val; }

	FixedPoint(double x) { val = (int)(x * (1 << fixptShift) + 0.5); }

	FixedPoint(int x) { val = x << fixptShift; }

	FixedPoint(uint32_t x) { val = x << fixptShift; }

	FixedPoint(long x) { val = (int)x << fixptShift; }

	operator float()
	{
		return (float)val * ((float)1 / (float)(1 << fixptShift));
	}

	operator double()
	{
		return (double)val * (1.0 / (double)(1 << fixptShift));
	}

	operator int()
	{
		return val >> fixptShift;
	}

	int get16Dot16() { return val; }

	FixedPoint operator=(FixedPoint x)
	{
		val = x.val;
		return *this;
	}

	bool operator==(FixedPoint x) const { return val == x.val; }

	bool operator==(double x) const { return *this == (FixedPoint)x; }

	bool operator==(int x) const { return *this == (FixedPoint)x; }

	bool operator==(uint32_t x) const { return *this == (FixedPoint)x; }

	bool operator==(long x) const { return *this == (FixedPoint)x; }

	bool operator!=(FixedPoint x) const { return val != x.val; }

	bool operator!=(double x) const { return *this != (FixedPoint)x; }

	bool operator!=(int x) const { return *this != (FixedPoint)x; }

	bool operator!=(uint32_t x) const { return *this != (FixedPoint)x; }

	bool operator!=(long x) const { return *this != (FixedPoint)x; }

	bool operator<(FixedPoint x) const { return val < x.val; }

	bool operator<(double x) const { return *this < (FixedPoint)x; }

	bool operator<(int x) const { return *this < (FixedPoint)x; }

	bool operator<(uint32_t x) const { return *this < (FixedPoint)x; }

	bool operator<(long x) const { return *this < (FixedPoint)x; }

	bool operator<=(FixedPoint x) const { return val <= x.val; }

	bool operator<=(double x) const { return *this <= (FixedPoint)x; }

	bool operator<=(int x) const { return *this <= (FixedPoint)x; }

	bool operator<=(uint32_t x) const { return *this <= (FixedPoint)x; }

	bool operator<=(long x) const { return *this <= (FixedPoint)x; }

	bool operator>(FixedPoint x) const { return val > x.val; }

	bool operator>(double x) const { return *this > (FixedPoint)x; }

	bool operator>(int x) const { return *this > (FixedPoint)x; }

	bool operator>(uint32_t x) const { return *this > (FixedPoint)x; }

	bool operator>(long x) const { return *this > (FixedPoint)x; }

	bool operator>=(FixedPoint x) const { return val >= x.val; }

	bool operator>=(double x) const { return *this >= (FixedPoint)x; }

	bool operator>=(int x) const { return *this >= (FixedPoint)x; }

	bool operator>=(uint32_t x) const { return *this >= (FixedPoint)x; }

	bool operator>=(long x) const { return *this >= (FixedPoint)x; }

	FixedPoint operator-() { return make(-val); }

	FixedPoint operator+(FixedPoint x) { return make(val + x.val); }

	FixedPoint operator+(double x) { return *this + (FixedPoint)x; }

	FixedPoint operator+(int x) { return *this + (FixedPoint)x; }

	FixedPoint operator+(uint32_t x) { return *this + (FixedPoint)x; }

	FixedPoint operator+(long x) { return *this + (FixedPoint)x; }

	FixedPoint operator+=(FixedPoint x)
	{
		val = val + x.val;
		return *this;
	}

	FixedPoint operator+=(double x) { return *this += (FixedPoint)x; }

	FixedPoint operator+=(int x) { return *this += (FixedPoint)x; }

	FixedPoint operator+=(uint32_t x) { return *this += (FixedPoint)x; }

	FixedPoint operator+=(long x) { return *this += (FixedPoint)x; }

	FixedPoint operator-(FixedPoint x) { return make(val - x.val); }

	FixedPoint operator-(double x) { return *this - (FixedPoint)x; }

	FixedPoint operator-(int x) { return *this - (FixedPoint)x; }

	FixedPoint operator-(uint32_t x) { return *this - (FixedPoint)x; }

	FixedPoint operator-(long x) { return *this - (FixedPoint)x; }

	FixedPoint operator-=(FixedPoint x)
	{
		val = val - x.val;
		return *this;
	}

	FixedPoint operator-=(double x) { return *this -= (FixedPoint)x; }

	FixedPoint operator-=(int x) { return *this -= (FixedPoint)x; }

	FixedPoint operator-=(uint32_t x) { return *this -= (FixedPoint)x; }

	FixedPoint operator-=(long x) { return *this -= (FixedPoint)x; }

	FixedPoint operator*(FixedPoint x) { return make(mul(val, x.val)); }

	FixedPoint operator*(double x) { return *this * (FixedPoint)x; }

	FixedPoint operator*(int x) { return *this * (FixedPoint)x; }

	FixedPoint operator*(uint32_t x) { return *this * (FixedPoint)x; }

	FixedPoint operator*(long x) { return *this * (FixedPoint)x; }

	FixedPoint operator*=(FixedPoint x)
	{
		val = mul(val, x.val);
		return *this;
	}

	FixedPoint operator*=(double x) { return *this *= (FixedPoint)x; }

	FixedPoint operator*=(int x) { return *this *= (FixedPoint)x; }

	FixedPoint operator*=(uint32_t x) { return *this *= (FixedPoint)x; }

	FixedPoint operator*=(long x) { return *this *= (FixedPoint)x; }

	FixedPoint operator/(FixedPoint x) { return make(div(val, x.val)); }

	FixedPoint operator/(double x) { return *this / (FixedPoint)x; }

	FixedPoint operator/(int x) { return *this / (FixedPoint)x; }

	FixedPoint operator/(uint32_t x) { return *this / (FixedPoint)x; }

	FixedPoint operator/(long x) { return *this / (FixedPoint)x; }

	FixedPoint operator/=(FixedPoint x)
	{
		val = div(val, x.val);
		return *this;
	}

	FixedPoint operator/=(double x) { return *this /= (FixedPoint)x; }

	FixedPoint operator/=(int x) { return *this /= (FixedPoint)x; }

	FixedPoint operator/=(uint32_t x) { return *this /= (FixedPoint)x; }

	FixedPoint operator/=(long x) { return *this /= (FixedPoint)x; }

	static FixedPoint abs(FixedPoint x) { return make(::abs(x.val)); }

	static int floor(FixedPoint x) { return x.val >> fixptShift; }

	static int ceil(FixedPoint x)
	{
		return (x.val & fixptMaskL) ? ((x.val >> fixptShift) + 1)
		                            : (x.val >> fixptShift);
	}

	static int round(FixedPoint x)
	{
		return (x.val + (1 << (fixptShift - 1))) >> fixptShift;
	}

	// Computes (x+y)/2 avoiding overflow and LSbit accuracy issues.
	static FixedPoint avg(FixedPoint x, FixedPoint y)
	{
		return make((x.val >> 1) + (y.val >> 1) + ((x.val | y.val) & 1));
	}

	static FixedPoint sqrt(FixedPoint x);

	static FixedPoint pow(FixedPoint x, FixedPoint y);

	// Compute *result = x/y; return false if there is an underflow or
	// overflow.
	static bool divCheck(FixedPoint x, FixedPoint y, FixedPoint* result);

	// Compute abs(m11*m22 - m12*m21) >= epsilon, handling the case
	// where the multiplications overflow.
	static bool checkDet(FixedPoint m11, FixedPoint m12, FixedPoint m21, FixedPoint m22, FixedPoint epsilon);

private:
	static FixedPoint make(int valA)
	{
		FixedPoint x;
		x.val = valA;
		return x;
	}

	static int mul(int x, int y);
	static int div(int x, int y);

	int val = 0; // fixed point: (n-fixptShift).(fixptShift)
};

#endif // USE_FIXEDPOINT
