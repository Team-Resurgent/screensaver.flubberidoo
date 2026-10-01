/*
 *  Minimal ISO C99 <stdint.h> for Microsoft Visual C++ 7.1 (Visual Studio .NET 2003).
 *
 *  VC7.1 predates C99 and ships no <stdint.h>. The Kodi addon dev-kit
 *  (kodi/c-api/addon_base.h and friends) includes <stdint.h> unconditionally,
 *  so this shim provides the fixed-width integer types and limit macros that
 *  the dev-kit actually uses. Targets the 32-bit x86 Xbox ABI.
 *
 *  Placed on the compiler include path via the project's AdditionalIncludeDirectories.
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _MSC_STDINT_H_
#define _MSC_STDINT_H_

#if _MSC_VER > 1000
#pragma once
#endif

/* Exact-width integer types */
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef __int64            int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned __int64   uint64_t;

/* Minimum-width integer types */
typedef int8_t             int_least8_t;
typedef int16_t            int_least16_t;
typedef int32_t            int_least32_t;
typedef int64_t            int_least64_t;
typedef uint8_t            uint_least8_t;
typedef uint16_t           uint_least16_t;
typedef uint32_t           uint_least32_t;
typedef uint64_t           uint_least64_t;

/* Fastest minimum-width integer types */
typedef int8_t             int_fast8_t;
typedef int16_t            int_fast16_t;
typedef int32_t            int_fast32_t;
typedef int64_t            int_fast64_t;
typedef uint8_t            uint_fast8_t;
typedef uint16_t           uint_fast16_t;
typedef uint32_t           uint_fast32_t;
typedef uint64_t           uint_fast64_t;

/* Integer types capable of holding a pointer (32-bit Xbox target) */
typedef int                intptr_t;
typedef unsigned int       uintptr_t;

/* Greatest-width integer types */
typedef int64_t            intmax_t;
typedef uint64_t           uintmax_t;

/* Limits of exact-width integer types */
#define INT8_MIN           (-127i8 - 1)
#define INT16_MIN          (-32767i16 - 1)
#define INT32_MIN          (-2147483647i32 - 1)
#define INT64_MIN          (-9223372036854775807i64 - 1)
#define INT8_MAX           127i8
#define INT16_MAX          32767i16
#define INT32_MAX          2147483647i32
#define INT64_MAX          9223372036854775807i64
#define UINT8_MAX          0xffui8
#define UINT16_MAX         0xffffui16
#define UINT32_MAX         0xffffffffui32
#define UINT64_MAX         0xffffffffffffffffui64

/* Limits of minimum-width integer types */
#define INT_LEAST8_MIN     INT8_MIN
#define INT_LEAST16_MIN    INT16_MIN
#define INT_LEAST32_MIN    INT32_MIN
#define INT_LEAST64_MIN    INT64_MIN
#define INT_LEAST8_MAX     INT8_MAX
#define INT_LEAST16_MAX    INT16_MAX
#define INT_LEAST32_MAX    INT32_MAX
#define INT_LEAST64_MAX    INT64_MAX
#define UINT_LEAST8_MAX    UINT8_MAX
#define UINT_LEAST16_MAX   UINT16_MAX
#define UINT_LEAST32_MAX   UINT32_MAX
#define UINT_LEAST64_MAX   UINT64_MAX

/* Limits of fastest minimum-width integer types */
#define INT_FAST8_MIN      INT8_MIN
#define INT_FAST16_MIN     INT16_MIN
#define INT_FAST32_MIN     INT32_MIN
#define INT_FAST64_MIN     INT64_MIN
#define INT_FAST8_MAX      INT8_MAX
#define INT_FAST16_MAX     INT16_MAX
#define INT_FAST32_MAX     INT32_MAX
#define INT_FAST64_MAX     INT64_MAX
#define UINT_FAST8_MAX     UINT8_MAX
#define UINT_FAST16_MAX    UINT16_MAX
#define UINT_FAST32_MAX    UINT32_MAX
#define UINT_FAST64_MAX    UINT64_MAX

/* Limits of integer types capable of holding a pointer */
#define INTPTR_MIN         INT32_MIN
#define INTPTR_MAX         INT32_MAX
#define UINTPTR_MAX        UINT32_MAX

/* Limits of greatest-width integer types */
#define INTMAX_MIN         INT64_MIN
#define INTMAX_MAX         INT64_MAX
#define UINTMAX_MAX        UINT64_MAX

/* Limits of other integer types */
#define PTRDIFF_MIN        INT32_MIN
#define PTRDIFF_MAX        INT32_MAX
#define SIG_ATOMIC_MIN     INT32_MIN
#define SIG_ATOMIC_MAX     INT32_MAX
#define SIZE_MAX           UINT32_MAX

#ifndef WCHAR_MIN
#define WCHAR_MIN          0
#define WCHAR_MAX          0xffff
#endif

#define WINT_MIN           0
#define WINT_MAX           0xffff

/* Macros for integer constants */
#define INT8_C(val)        val##i8
#define INT16_C(val)       val##i16
#define INT32_C(val)       val##i32
#define INT64_C(val)       val##i64
#define UINT8_C(val)       val##ui8
#define UINT16_C(val)      val##ui16
#define UINT32_C(val)      val##ui32
#define UINT64_C(val)      val##ui64
#define INTMAX_C(val)      INT64_C(val)
#define UINTMAX_C(val)     UINT64_C(val)

#endif /* _MSC_STDINT_H_ */
