//========================================================================
//
// PDF417Barcode.h
//
// Copyright 2018 Glyph & Cog, LLC
//
//========================================================================

#ifndef PDF417BARCODE_H
#define PDF417BARCODE_H

#include <aconf.h>

// Draw a PDF417 barcode:
// fieldWidth, fieldHeight = field size (in points)
// moduleWidth, moduleHeight = requested module size (in points)
// errorCorrectionLevel = 0 .. 8
// value = byte string
// output is appended to appearBuf
// Returns true on success, false on error.
extern bool drawPDF417Barcode(double fieldWidth, double fieldHeight, double moduleWidth, double moduleHeight, int errorCorrectionLevel, const std::string& value, std::string& appearBuf);

#endif
