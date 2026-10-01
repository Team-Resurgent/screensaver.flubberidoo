/*
 *  Minimal ISO C99 <inttypes.h> for Microsoft Visual C++ 7.1 (Visual Studio .NET 2003).
 *
 *  Provides the PRI and SCN format-specifier macros used by the Kodi addon dev-kit
 *  (e.g. PRIu64). MSVC uses the "I64" length modifier for 64-bit integers.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _MSC_INTTYPES_H_
#define _MSC_INTTYPES_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include <stdint.h>

/* 8-bit */
#define PRId8   "d"
#define PRIi8   "i"
#define PRIo8   "o"
#define PRIu8   "u"
#define PRIx8   "x"
#define PRIX8   "X"

/* 16-bit */
#define PRId16  "d"
#define PRIi16  "i"
#define PRIo16  "o"
#define PRIu16  "u"
#define PRIx16  "x"
#define PRIX16  "X"

/* 32-bit */
#define PRId32  "d"
#define PRIi32  "i"
#define PRIo32  "o"
#define PRIu32  "u"
#define PRIx32  "x"
#define PRIX32  "X"

/* 64-bit (MSVC length modifier is I64) */
#define PRId64  "I64d"
#define PRIi64  "I64i"
#define PRIo64  "I64o"
#define PRIu64  "I64u"
#define PRIx64  "I64x"
#define PRIX64  "I64X"

/* pointer-width / max-width (32-bit target -> 32-bit ptr, 64-bit max) */
#define PRIdPTR   PRId32
#define PRIiPTR   PRIi32
#define PRIoPTR   PRIo32
#define PRIuPTR   PRIu32
#define PRIxPTR   PRIx32
#define PRIXPTR   PRIX32

#define PRIdMAX   PRId64
#define PRIiMAX   PRIi64
#define PRIoMAX   PRIo64
#define PRIuMAX   PRIu64
#define PRIxMAX   PRIx64
#define PRIXMAX   PRIX64

#endif /* _MSC_INTTYPES_H_ */
