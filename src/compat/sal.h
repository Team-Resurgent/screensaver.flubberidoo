/*
 *  Minimal <sal.h> for Microsoft Visual C++ 7.1 (Visual Studio .NET 2003) + Xbox SDK.
 *
 *  SAL (Source-code Annotation Language) macros are compile-time analysis hints with
 *  no runtime effect. VC7.1 / the Xbox SDK predate sal.h; the Kodi dev-kit's
 *  tools/StringUtils.h includes it (for _Printf_format_string_). Defining the common
 *  annotations as empty satisfies the include and the annotations with no behaviour change.
 *
 *  Placed on the compiler include path via the project's AdditionalIncludeDirectories.
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _MSC_SAL_H_
#define _MSC_SAL_H_

#if _MSC_VER > 1000
#pragma once
#endif

/* Printf/scanf format-string annotation (the one StringUtils.h actually uses). */
#ifndef _Printf_format_string_
#define _Printf_format_string_
#endif
#ifndef _Scanf_format_string_
#define _Scanf_format_string_
#endif

/* Common parameter annotations - all no-ops. */
#define _In_
#define _In_opt_
#define _In_z_
#define _In_opt_z_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_
#define _Ret_z_
#define _Printf_format_string_params_(x)
#define _Scanf_format_string_params_(x)
#define _In_reads_(x)
#define _In_reads_z_(x)
#define _Out_writes_(x)
#define _Out_writes_z_(x)
#define __format_string

#endif /* _MSC_SAL_H_ */
