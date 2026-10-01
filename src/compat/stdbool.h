/*
 *  Minimal ISO C99 <stdbool.h> for Microsoft Visual C++ 7.1 (Visual Studio .NET 2003).
 *
 *  The Kodi addon dev-kit C-API headers include <stdbool.h>, which VC7.1 does not
 *  ship. In C++ (how this addon is compiled) bool/true/false are language keywords,
 *  so this header only needs to define the __bool_true_false_are_defined marker.
 *  A C fallback is provided for completeness.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _MSC_STDBOOL_H_
#define _MSC_STDBOOL_H_

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef __cplusplus
#define bool   _Bool
#define true   1
#define false  0
typedef unsigned char _Bool;
#endif

#define __bool_true_false_are_defined 1

#endif /* _MSC_STDBOOL_H_ */
