/*
 *  Stub of <kodi/Filesystem.h> for the standalone MatrixTrails runner.
 *
 *  Only kodi::vfs::TranslateSpecialProtocol is used by src/main.cpp. With no Kodi
 *  VFS available, special:// paths can't be resolved, so the input is returned
 *  unchanged (GetAddonPath already yields a plain D:\ path on Xbox).
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <string>

#ifndef ATTR_DLL_LOCAL
#define ATTR_DLL_LOCAL
#endif

namespace kodi
{
namespace vfs
{
inline std::string ATTR_DLL_LOCAL TranslateSpecialProtocol(const std::string& source)
{ return source; }
} // namespace vfs
} // namespace kodi
