/*
 *  fog.cpp — see fog.h. Ported from BootAnimRXDK green_fog.cpp + tex_gen.cpp.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "main.h"      // d3dSetRenderState / d3dSetTextureStageState wrappers
#include "fog.h"
#include "scene.h"
#include "blob.h"      // QuickRand
#include "flubshaders_bin.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <xgraphics.h>         // XGSwizzleRect (the plasma texture must be swizzled)

// xgraphics for XGSwizzleRect. flubber.cpp already pulls xgraphics.lib for the
// production .xbs; the standalone runner links xgraphics itself.
#ifndef FLUBBERIDOO_NO_DX8_LIB_PRAGMA
#pragma comment(lib, "xgraphics.lib")
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

// --- tunables (match green_fog.cpp) -----------------------------------------
static const int   PLASMA_SIZE   = 256;
static const int   NUM_PLASMAS   = 3;
static const f32   MUL_SCALE     = 0.005f;
static const f32   MAIN_FOG_RAD  = 40.0f;
// Steady ambient green add (cinematic: replaces the boot-anim finish flash).
static const f32   FOG_STEADY_GLOW = 0.15f;

/* ---------------------------------------------------- shader loaders ------- */
static DWORD FogLoadPS(LPDIRECT3DDEVICE8 dev, const BYTE* blob)
{
  const D3DPIXELSHADERDEF_FILE* f = (const D3DPIXELSHADERDEF_FILE*)blob;
  DWORD h = 0;
  dev->CreatePixelShader((D3DPIXELSHADERDEF*)&f->Psd, &h);
  return h;
}
static DWORD FogLoadVS(LPDIRECT3DDEVICE8 dev, const DWORD* decl, const BYTE* blob)
{
  DWORD h = 0;
  dev->CreateVertexShader(decl, (const DWORD*)blob, &h, 0);
  return h;
}

/* ------------------------------------------------ plasma texture ----------- */
// tex_gen::CreateIntensityTexture_8Bit for a single wrap-around diamond-square
// plasma (the original generates NUM_PLASMAS but swizzles only #0 into all of
// them, so one texture reproduces the rendered result). A8, intensity 0..85.
static u32 RandScale(QuickRand& r, int scale)
{
  u32 n = r.Rand();
  return (u32)(((unsigned long long)n * (unsigned int)scale) >> 32);
}

static LPDIRECT3DTEXTURE8 BuildPlasmaTexture(LPDIRECT3DDEVICE8 dev)
{
  const int size = PLASMA_SIZE;
  const int tffonp = 255 / NUM_PLASMAS;             // 85
  const int noise  = 5 * tffonp;                    // 425
  const int intensity_seed = ((tffonp * 3) / 4) << 8;   // (BYTE) -> 0
  const int intensity_max  = 255 / (NUM_PLASMAS > 1 ? NUM_PLASMAS : 1); // 85

  LPDIRECT3DTEXTURE8 tex = 0;
  if (FAILED(dev->CreateTexture(size, size, 1, 0, D3DFMT_A8, D3DPOOL_MANAGED, &tex)))
    return 0;

  BYTE* src = (BYTE*)malloc((size_t)size * size);
  if (!src) { tex->Release(); return 0; }
  memset(src, 0, (size_t)size * size);

  QuickRand rng(0x13572468u);
  src[0] = (BYTE)intensity_seed;

  int curSize = size >> 1, curX = curSize, curY = curSize;
  int curNoise = noise >> 1, curStep = size;
  bool bSquare = true, bSecondPass = false;

  while (curSize > 0)
  {
    int lx = curX - curSize, rx = curX + curSize;
    int ly = curY - curSize, uy = curY + curSize;
    if (lx < 0)     lx += size;
    if (rx >= size) rx -= size;
    if (ly < 0)     ly += size;
    if (uy >= size) uy -= size;

    if (bSquare)
    {
      int sw = src[size*ly+lx], se = src[size*ly+rx], nw = src[size*uy+lx], ne = src[size*uy+rx];
      int dwI = (sw + se + nw + ne) >> 2;
      dwI += (int)RandScale(rng, curNoise * 2) - curNoise;
      src[size*curY+curX] = (BYTE)max(0, min(intensity_max, dwI));

      curX += curStep;
      if (curX >= size)
      {
        curY += curStep;
        if (curY >= size) { curX = curSize; curY = 0; bSquare = false; continue; }
        curX = curSize;
      }
    }
    else
    {
      int cN = src[size*uy+curX] & 0xff, cS = src[size*ly+curX] & 0xff;
      int cW = src[size*curY+lx] & 0xff, cE = src[size*curY+rx] & 0xff;
      int dwI = (cN + cS + cE + cW) >> 2;
      dwI += (int)RandScale(rng, curNoise * 2) - curNoise;
      src[size*curY+curX] = (BYTE)max(0, min(intensity_max, dwI));

      curX += curStep;
      if (curX >= size)
      {
        curY += curStep;
        if (curY >= size)
        {
          if (bSecondPass)
          {
            curStep = curSize; curSize >>= 1; curNoise >>= 1;
            curX = curSize; curY = curSize; bSquare = true;
          }
          else { curX = 0; curY = curSize; }
          bSecondPass = !bSecondPass;
          continue;
        }
        curX = bSecondPass ? 0 : curSize;
      }
    }
  }

  D3DLOCKED_RECT rc;
  if (SUCCEEDED(tex->LockRect(0, &rc, NULL, 0)))
  {
    XGSwizzleRect(src, 0, NULL, rc.pBits, size, size, NULL, sizeof(BYTE));
    tex->UnlockRect(0);
  }
  free(src);
  return tex;
}

/* ------------------------------------------------------------- CFog -------- */
CFog::CFog()
  : m_dev(null), m_w(0), m_h(0), m_quadVB(null), m_backVB(null), m_plasmaTex(null),
    m_intensityU(null), m_intensityR(null),
    m_vsFog(0), m_psFog(0), m_vsZ(0), m_psZ(0) {}

CFog::~CFog() { Release(); }

void CFog::Release()
{
  SAFE_RELEASE(m_quadVB);
  SAFE_RELEASE(m_backVB);
  SAFE_RELEASE(m_plasmaTex);
  SAFE_RELEASE(m_intensityU);
  SAFE_RELEASE(m_intensityR);
  if (m_dev)
  {
    if (m_vsFog) m_dev->DeleteVertexShader(m_vsFog);
    if (m_psFog) m_dev->DeletePixelShader(m_psFog);
    if (m_vsZ)   m_dev->DeleteVertexShader(m_vsZ);
    if (m_psZ)   m_dev->DeletePixelShader(m_psZ);
  }
  m_vsFog = m_psFog = m_vsZ = m_psZ = 0;
  m_dev = null;
}

bool CFog::Create(LPDIRECT3DDEVICE8 dev, int screenW, int screenH)
{
  Release();
  m_dev = dev;

  // Intensity RT: a SQUARE power-of-two, and <= the backbuffer so the main depth
  // surface can be reused (exactly as BakeEnvCube's 128^3 cube RT does). A
  // non-square RT (e.g. 512x256) samples back with an NV2A swizzle mismatch -- a
  // checkerboard across the frame; a square RT swizzles cleanly like the cube.
  // Clip-space normalisation keeps the (4:3) scene aligned when sampled full-screen.
  int side = 256;
  if (side > screenW) side = screenW;
  if (side > screenH) side = screenH;
  m_w = m_h = side;
  if (m_w <= 0 || m_h <= 0) return false;

  if (FAILED(dev->CreateTexture(m_w, m_h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT, &m_intensityU))) return false;
  if (FAILED(dev->CreateTexture(m_w, m_h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT, &m_intensityR))) return false;

  // Composite quad (clip-space fullscreen). tu0/tv0 sample the intensity map;
  // tu1/tv1 seed the (camera-scrolled) plasma lookup. green_fog.cpp lines 49-69.
  if (FAILED(dev->CreateVertexBuffer(4 * sizeof(FogVtx), 0, 0, D3DPOOL_DEFAULT, &m_quadVB)))
    return false;
  {
    const f32 pd = (f32)PLASMA_SIZE;
    FogVtx qv[4];
    const f32 px[4] = { -1.0f, -1.0f, +1.0f, +1.0f };
    const f32 py[4] = { -1.0f, +1.0f, +1.0f, -1.0f };
    const f32 u0[4] = {  0.0f,  0.0f,  1.0f,  1.0f };
    const f32 v0[4] = {  1.0f,  0.0f,  0.0f,  1.0f };
    for (int i = 0; i < 4; i++)
    {
      qv[i].x = px[i]; qv[i].y = py[i]; qv[i].z = 1.0f;
      qv[i].tu0 = u0[i]; qv[i].tv0 = v0[i];
      qv[i].tv1 = -(2.0f * u0[i] - 1.0f) * 640.0f / pd;
      qv[i].tu1 = -(2.0f * v0[i] - 1.0f) * 480.0f / pd;
    }
    FogVtx* d = 0;
    if (SUCCEEDED(m_quadVB->Lock(0, 0, (BYTE**)&d, 0))) { memcpy(d, qv, sizeof(qv)); m_quadVB->Unlock(); }
  }

  // Backdrop quad (position only) for the intensity pass.
  if (FAILED(dev->CreateVertexBuffer(4 * sizeof(FogBackVtx), 0, 0, D3DPOOL_DEFAULT, &m_backVB)))
    return false;
  {
    FogBackVtx bv[4] = { {-1,-1,1}, {-1,1,1}, {1,1,1}, {1,-1,1} };
    FogBackVtx* d = 0;
    if (SUCCEEDED(m_backVB->Lock(0, 0, (BYTE**)&d, 0))) { memcpy(d, bv, sizeof(bv)); m_backVB->Unlock(); }
  }

  static const DWORD fogDecl[] =
  {
    D3DVSD_STREAM(0),
    D3DVSD_REG(0, D3DVSDT_FLOAT3),   // position
    D3DVSD_REG(1, D3DVSDT_FLOAT2),   // intensity texcoord
    D3DVSD_REG(2, D3DVSDT_FLOAT2),   // plasma texcoord
    D3DVSD_END()
  };
  static const DWORD zDecl[] =
  {
    D3DVSD_STREAM(0),
    D3DVSD_REG(0, D3DVSDT_FLOAT3),   // position (scene VBs carry a normal after; unread)
    D3DVSD_END()
  };

  m_vsFog = FogLoadVS(dev, fogDecl, g_green_fog_xvu);
  m_psFog = FogLoadPS(dev, g_green_fog_xpu);
  m_vsZ   = FogLoadVS(dev, zDecl,   g_scene_zr_xvu);
  m_psZ   = FogLoadPS(dev, g_scene_zr_xpu);

  m_plasmaTex = BuildPlasmaTexture(dev);

  return (m_vsFog && m_psFog && m_vsZ && m_psZ && m_plasmaTex);
}

// Render the fog-density map into m_intensityU.
void CFog::RenderIntensity(LPDIRECT3DDEVICE8 dev, CScene& scene, f32 fpos, const FogCam& cam)
{
  LPDIRECT3DSURFACE8 oldRT = 0, oldZ = 0;
  dev->GetRenderTarget(&oldRT);
  dev->GetDepthStencilSurface(&oldZ);

  LPDIRECT3DSURFACE8 surf = 0;
  if (FAILED(m_intensityU->GetSurfaceLevel(0, &surf)))
  { if (oldRT) oldRT->Release(); if (oldZ) oldZ->Release(); return; }

  dev->SetRenderTarget(surf, oldZ);           // reuse the main depth (>= m_w/m_h)
  if (SUCCEEDED(dev->BeginScene()))
  {
    dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
               D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

    // origin (blob centre) projected to NDC -- centres the radial falloff.
    const Mat4& vp = cam.viewProj;
    f32 ow = vp.m[15]; if (ow == 0.0f) ow = 1.0f;
    f32 osx = vp.m[12] / ow, osy = vp.m[13] / ow;

    f32 x_mul = cam.radBlob * MUL_SCALE;
    f32 y_mul = cam.radBlob * MUL_SCALE * cam.aspect;

    // --- backdrop: fill the whole map at the far plane (scene_zr) -----------
    d3dSetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    d3dSetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
    d3dSetRenderState(D3DRS_ALPHAREF, 0x00000001);
    d3dSetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    d3dSetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
    d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    d3dSetRenderState(D3DRS_ZWRITEENABLE, FALSE);

    dev->SetVertexShader(m_vsZ);
    dev->SetPixelShader(m_psZ);

    f32 c16[16];
    // c16=Z_MUL, c17=Z_ADD, c18=POS_MUL, c19=POS_SHIFT
    f32 z_mul = 1.0f, z_add = 0.0f;
    c16[0]=z_mul; c16[1]=z_mul; c16[2]=z_mul; c16[3]=1.0f;
    c16[4]=z_add; c16[5]=z_add; c16[6]=z_add; c16[7]=1.0f;
    c16[8]=x_mul; c16[9]=y_mul; c16[10]=0.0f; c16[11]=1.0f;
    c16[12]=0.5f - x_mul*osx; c16[13]=0.5f - y_mul*osy; c16[14]=0.5f; c16[15]=0.0f;
    dev->SetVertexShaderConstant(16, c16, 4);

    // Backdrop FINAL_MAT: identity with w taken from z (far plane), untransposed.
    static const f32 backMat[16] =
    { 1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,1,0 };
    dev->SetVertexShaderConstant(0, backMat, 4);

    dev->SetStreamSource(0, m_backVB, sizeof(FogBackVtx));
    dev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);

    // --- scene geometry carves the density (nearer surfaces => less fog) ----
    d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    d3dSetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

    z_add = MAIN_FOG_RAD - cam.radBlob;
    z_mul = 1.0f / (2.0f * MAIN_FOG_RAD);
    c16[0]=z_mul; c16[1]=z_mul; c16[2]=z_mul; c16[3]=1.0f;
    c16[4]=z_add; c16[5]=z_add; c16[6]=z_add; c16[7]=1.0f;
    dev->SetVertexShaderConstant(16, c16, 4);

    scene.DrawZ(dev, cam.viewProj, fpos);

    dev->SetPixelShader(NULL);
    dev->SetVertexShader(NULL);

    // Return the (shared) depth buffer to a cleared state for whatever follows.
    dev->Clear(0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_ARGB(0,0,0,0), 1.0f, 0);
    dev->EndScene();
  }

  dev->SetRenderTarget(oldRT, oldZ);
  if (surf)  surf->Release();
  if (oldRT) oldRT->Release();
  if (oldZ)  oldZ->Release();
}

void CFog::Render(LPDIRECT3DDEVICE8 dev, CScene& scene, f32 fpos,
                  const FogCam& cam, f32 blobIntensity, const CTheme& theme)
{
  if (!m_vsFog || !m_psFog || !m_vsZ || !m_psZ || !m_plasmaTex) return;

  // (1) Build the density map for this frame.
  RenderIntensity(dev, scene, fpos, cam);

  // (2) Composite the plasma mist over the scene.
  dev->SetVertexShader(m_vsFog);
  dev->SetPixelShader(m_psFog);

  // Swap: sample what RenderIntensity just wrote.
  LPDIRECT3DTEXTURE8 swap = m_intensityR; m_intensityR = m_intensityU; m_intensityU = swap;

  dev->SetTexture(0, m_intensityR);
  d3dSetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
  d3dSetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
  d3dSetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
  d3dSetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
  d3dSetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

  for (int i = 0; i < NUM_PLASMAS; i++)
  {
    dev->SetTexture(i + 1, m_plasmaTex);
    d3dSetTextureStageState(i + 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    d3dSetTextureStageState(i + 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    d3dSetTextureStageState(i + 1, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    d3dSetTextureStageState(i + 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    d3dSetTextureStageState(i + 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
  }

  d3dSetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
  d3dSetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
  d3dSetRenderState(D3DRS_ALPHAREF, 0x00000001);
  d3dSetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);       // steady fog, no depth gate
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  d3dSetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);    // additive
  d3dSetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

  // Pixel constants: c0 = Plasma1 * blob-intensity, c1 = Plasma2 * steady glow.
  f32 fi = blobIntensity * 0.7f - 0.1f; if (fi < 0.0f) fi = 0.0f;
  const CRGBA& p1 = theme.plasma1;
  const CRGBA& p2 = theme.plasma2;
  f32 pc[8];
  pc[0] = p1.r * fi; pc[1] = p1.g * fi; pc[2] = p1.b * fi; pc[3] = 1.0f;
  pc[4] = p2.r * FOG_STEADY_GLOW; pc[5] = p2.g * FOG_STEADY_GLOW; pc[6] = p2.b * FOG_STEADY_GLOW; pc[7] = 1.0f;
  dev->SetPixelShaderConstant(0, pc, 2);

  // Vertex constants c0..c5: per-plasma scroll (offset + scale) from camera.
  const Mat4& vp = cam.viewProj;
  f32 ow = vp.m[15]; if (ow == 0.0f) ow = 1.0f;
  f32 osx = vp.m[12] / ow, osy = vp.m[13] / ow;
  const f32 pd = (f32)PLASMA_SIZE;

  f32 vc[6 * 4];
  for (int i = 0; i < NUM_PLASMAS; i++)
  {
    f32 rad = 0.6f * (((f32)(NUM_PLASMAS - i - 1)) / (f32)NUM_PLASMAS - 0.2f);
    f32 x_mul = 0.5f * cam.radBlob * MUL_SCALE;
    f32 y_mul = 1.0f * cam.aspect * cam.radBlob * MUL_SCALE;
    f32 x_add = rad * cam.theta - osx * x_mul * 640.0f / pd;
    f32 y_add = -rad * cam.phi  + osy * y_mul * 480.0f / pd;

    f32* off = &vc[(2*i + 0) * 4];
    f32* scl = &vc[(2*i + 1) * 4];
    off[0] = y_add;  off[1] = x_add;  off[2] = 0.0f; off[3] = 0.0f;
    scl[0] = -y_mul; scl[1] = -x_mul; scl[2] = 1.0f; scl[3] = 1.0f;
  }
  dev->SetVertexShaderConstant(0, vc, 2 * NUM_PLASMAS);

  dev->SetStreamSource(0, m_quadVB, sizeof(FogVtx));
  dev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);

  // Restore.
  dev->SetPixelShader(NULL);
  dev->SetVertexShader(NULL);
  for (int i = 0; i < 1 + NUM_PLASMAS; i++) dev->SetTexture(i, NULL);

  d3dSetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  d3dSetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
}
