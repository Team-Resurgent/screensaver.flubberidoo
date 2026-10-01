/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *  Copyright (C) 2005 Joakim Eriksson <je@plane9.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "types.h"

#include <xtl.h>

// The production addon builds from src/, where "lib/xbox_dx8.lib" resolves. The
// standalone test runner builds from test/ and links the lib explicitly instead,
// so it defines MATRIXTRAILS_NO_DX8_LIB_PRAGMA to opt out of this path-relative pragma.
#ifndef MATRIXTRAILS_NO_DX8_LIB_PRAGMA
#pragma comment (lib, "lib/xbox_dx8.lib" )
#endif

extern "C" void d3dGetRenderState(DWORD dwY, DWORD* dwZ);
extern "C" void d3dSetRenderState(DWORD dwY, DWORD dwZ);
extern "C" void d3dSetTextureStageState( int x, DWORD dwY, DWORD dwZ);

/***************************** D E F I N E S *******************************/
/****************************** M A C R O S ********************************/
/***************************** C L A S S E S *******************************/

////////////////////////////////////////////////////////////////////////////
//
class CConfig
{
public:
  int m_NumColumns; // Number of character columns
  int m_NumRows; // Number of character rows

  CRGBA m_CharCol;
  CRGBA m_CharEventCol;
  f32 m_FadeSpeedMin; // How quickly the characters fade out
  f32 m_FadeSpeedMax;
  f32 m_CharDelayMin; // How long we wait before we add a new char
  f32 m_CharDelayMax;

  CVector2 m_CharSizeTex; // Size of the characters in the texture
  int m_NumChars; // Number of characters in the texture

  void SetDefaults();
};
