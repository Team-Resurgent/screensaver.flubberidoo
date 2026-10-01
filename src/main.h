/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "types.h"

#include <xtl.h>

// The production addon builds from src/, where "lib/xbox_dx8.lib" resolves and
// XBMC owns the D3D device, so render-state changes must route through its cached
// path (xbox_dx8.dll). The standalone test runner builds from test/, owns its own
// device, and implements these wrappers directly; it defines
// FLUBBERIDOO_NO_DX8_LIB_PRAGMA to opt out of this path-relative pragma.
#ifndef FLUBBERIDOO_NO_DX8_LIB_PRAGMA
#pragma comment (lib, "lib/xbox_dx8.lib" )
#endif

extern "C" void d3dGetRenderState(DWORD dwY, DWORD* dwZ);
extern "C" void d3dSetRenderState(DWORD dwY, DWORD dwZ);
extern "C" void d3dSetTextureStageState( int x, DWORD dwY, DWORD dwZ);
