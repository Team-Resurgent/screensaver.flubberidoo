////////////////////////////////////////////////////////////////////////////
//
// Matrix Trails Screensaver for XBox Media Center
// Copyright (c) 2005 Joakim Eriksson <je@plane9.com>
//
////////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "matrixtrails.h"

////////////////////////////////////////////////////////////////////////////
//
CMatrixTrails::CMatrixTrails(CConfig* config)
  : m_config(config), m_Device(null), m_X(0), m_Y(0)
{
    m_NumColumns    = m_config->m_NumColumns;
    m_NumRows        = m_config->m_NumRows;

    m_Columns.resize(m_NumColumns);
    for (int cNr=0; cNr<m_NumColumns; cNr++)
    {
        m_Columns[cNr].Init(m_config, m_NumRows);
    }

    m_Texture = null;
    m_VertexBuffer = null;
}

////////////////////////////////////////////////////////////////////////////
//
CMatrixTrails::~CMatrixTrails()
{
    InvalidateDevice();
}

////////////////////////////////////////////////////////////////////////////
//
bool CMatrixTrails::RestoreDevice(LPDIRECT3DDEVICE8 device, int x, int y, int width, int height, const std::string& path)
{
    LPDIRECT3DDEVICE8 d3dDevice = device;
    InvalidateDevice();
    m_Device = device;
    m_X = x;
    m_Y = y;

    if (!d3dDevice || width <= 0 || height <= 0)
        return false;
    // Reuse one column: at most 33,600 bytes even at 200 rows.
    DVERIFY(d3dDevice->CreateVertexBuffer(6*m_NumRows*sizeof(TRenderVertex), D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC, TRenderVertex::FVF_Flags, D3DPOOL_DEFAULT, &m_VertexBuffer));

DVERIFY(D3DXCreateTextureFromFileA(d3dDevice, path.c_str(), &m_Texture));

    m_CharSize.x    = (f32)width  / (f32)m_NumColumns;
    m_CharSize.y    = (f32)height / (f32)m_NumRows;
    m_CharSize.z    = 0.0f;

    return true;
}

////////////////////////////////////////////////////////////////////////////
//
void    CMatrixTrails::InvalidateDevice()
{
    SAFE_RELEASE( m_Texture );
    SAFE_RELEASE( m_VertexBuffer );
}

////////////////////////////////////////////////////////////////////////////
//
void        CMatrixTrails::Update(f32 dt)
{
    for (int cNr=0; cNr<m_NumColumns; cNr++)
    {
        m_Columns[cNr].Update(dt);
    }
}

////////////////////////////////////////////////////////////////////////////
//
bool        CMatrixTrails::Draw()
{
    LPDIRECT3DDEVICE8 d3dDevice = m_Device;

    if (!d3dDevice || !m_VertexBuffer || !m_Texture)
        return false;

    // Setup our texture
    d3dSetTextureStageState(0, D3DTSS_COLOROP,     D3DTOP_MODULATE);
    d3dSetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    d3dSetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    d3dSetTextureStageState(0, D3DTSS_ALPHAOP,     D3DTOP_DISABLE);
    d3dSetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    d3dSetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    d3dSetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    d3dSetTextureStageState(0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP);
    d3dSetTextureStageState(0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP);
    d3dSetTextureStageState(1, D3DTSS_COLOROP,     D3DTOP_DISABLE);
    d3dSetTextureStageState(1, D3DTSS_ALPHAOP,     D3DTOP_DISABLE);

    d3dSetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    d3dSetRenderState(D3DRS_FOGENABLE, FALSE);
    d3dSetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    d3dSetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    d3dSetRenderState(D3DRS_ZENABLE,    FALSE);
    d3dSetRenderState(D3DRS_LIGHTING,    FALSE);
    d3dSetRenderState(D3DRS_COLORVERTEX,TRUE);
    d3dSetRenderState(D3DRS_FILLMODE,    D3DFILL_SOLID );
    d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    d3dDevice->SetTexture( 0, m_Texture );
    d3dDevice->SetStreamSource(    0, m_VertexBuffer, sizeof(TRenderVertex) );
    d3dDevice->SetVertexShader( TRenderVertex::FVF_Flags    );

    // Independent triangle lists avoid strip connections between glyphs.
    // One draw per column uses only the existing Xbox import library.
    f32 posX = (f32)m_X - 0.5f;
    for (int cNr = 0; cNr < m_NumColumns; ++cNr)
    {
        TRenderVertex* vert = null;
        DVERIFY(m_VertexBuffer->Lock(0, 0, (BYTE**)&vert, 0));
        m_Columns[cNr].UpdateVertexBuffer(vert, posX, (f32)m_Y - 0.5f, m_CharSize, m_config->m_CharSizeTex);
        m_VertexBuffer->Unlock();
        d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2*m_NumRows);
        posX += m_CharSize.x;
    }

    return true;
}





