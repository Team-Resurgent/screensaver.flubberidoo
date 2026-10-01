/*
 * Copyright (C) 2005-2021 Team Kodi
 * Copyright (C) 2005 Joakim Eriksson <je@plane9.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include "main.h"
#include "column.h"
#include <string>
#include <vector>

struct TRenderVertex
{
  CVector pos;
  f32 w;
  DWORD col;
  f32 u, v;
  enum { FVF_Flags = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 };
};

class CMatrixTrails
{
public:
  explicit CMatrixTrails(CConfig* config);
  ~CMatrixTrails();
  bool RestoreDevice(LPDIRECT3DDEVICE8 device, int x, int y, int width, int height,
                     const std::string& path);
  void InvalidateDevice();
  void Update(f32 dt);
  bool Draw();
private:
  CMatrixTrails(const CMatrixTrails&);
  CMatrixTrails& operator=(const CMatrixTrails&);
  CConfig* m_config;
  LPDIRECT3DDEVICE8 m_Device; // Borrowed from Kodi's screensaver properties.
  int m_X, m_Y;
  int m_NumColumns, m_NumRows;
  std::vector<CColumn> m_Columns;
  CVector m_CharSize;
  LPDIRECT3DVERTEXBUFFER8 m_VertexBuffer;
  LPDIRECT3DTEXTURE8 m_Texture;
};
