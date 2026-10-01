/*
 *  flubber.cpp — Flubberidoo engine (see flubber.h).
 *
 *  This slice renders the first two layers: the animating blob body and the
 *  moving camera (TCB-Hermite spline). Scene geometry, shields and plasma are
 *  wired in behind their Draw* hooks in later slices.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "main.h"      // d3dSetRenderState / d3dSetTextureStageState wrappers
#include "flubber.h"
#include "flubshaders_bin.h"   // pre-assembled NV2A shader blobs (g_vblob_*, ...)

#include <math.h>
#include <string.h>
#include <xgraphics.h>         // XGSwizzleRect (cube textures must be swizzled)

// The production .xbs links xbox_dx8.lib via main.h's pragma; it also needs
// xgraphics for XGSwizzleRect. The standalone runner links xgraphics itself.
#ifndef FLUBBERIDOO_NO_DX8_LIB_PRAGMA
#pragma comment(lib, "xgraphics.lib")
#endif

using namespace flubtime;

/* ------------------------------------------------- intensity (anim.js) ---- */
struct Energy { f32 base, pulse, blob; };

static f32 SumPulses(const FlubPulse* p, f32 et)
{
  f32 sum = 0.0f;
  for (int i = 0; i < 12; i++)
  {
    f32 fdt = (f32)fabs(et - p[i].x);
    if (fdt > p[i].y) continue;
    f32 c = (f32)cos((fdt * 0.5f * PI) / p[i].y);
    sum += p[i].z * c;
  }
  return sum;
}

static void MakePulses(QuickRand& rng, FlubPulse* pulses)
{
  for (int i = 0; i < 12; i++)
  {
    f32 x = ((f32)(i + 1)) / 13.0f + rng.Rand11() * 0.03f;
    x = 1.0f - (0.5f * x * x + 0.5f * x);
    f32 y = (1.2f - x) * (1.2f - x) * (rng.Rand01() + 2.0f) * 0.05f;
    if (y < 0.1f) y = 0.1f;
    f32 z = (x + 0.5f) * (rng.Rand01() + 1.0f) * 0.2f;
    x = x * BLOB_PULSE_ELAPSED + BLOB_PULSE_START;
    f32 lo = BLOB_PULSE_START + y;
    if (x < lo) x = lo;
    pulses[i].x = x; pulses[i].y = y; pulses[i].z = z;
  }
  pulses[11].x = BLOB_PULSE_START + pulses[11].y;
  pulses[11].z *= 3.0f;
}

// Cinematic screensaver intensity: no boot-anim fade-in/ramp. The base is held
// at a steady "mature" level so the flubber continuously illuminates the scene,
// and the 12 pulses give it its heartbeat. (Original ramped base 0 -> ~0.5 over
// the demo and faded in; that caused a brightness reset at each loop point.)
static const f32 STEADY_BASE = 0.5f;
static Energy IntensityAt(f32 t, const FlubPulse* pulses)
{
  Energy e;
  e.base = STEADY_BASE;
  e.pulse = SumPulses(pulses, t);
  e.blob = e.base + e.pulse;
  return e;
}

/* ---------------------------------------------- procedural glow texture --- */
// tex_gen.cpp CreateGlowTexture(256) via blob.js glowTexels: a radial falloff
// with all four channels equal. Built into a linear A8R8G8B8 texture (no Xbox
// swizzling to worry about).
static bool BuildGlowTexture(LPDIRECT3DDEVICE8 dev, LPDIRECT3DTEXTURE8* out)
{
  const int size = 256;
  LPDIRECT3DTEXTURE8 tex = 0;
  if (FAILED(dev->CreateTexture(size, size, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_MANAGED, &tex)))
    return false;
  D3DLOCKED_RECT lr;
  if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); return false; }

  int scale = 1, tmp = 4096 / size; while (tmp != 1) { scale++; tmp >>= 1; }
  int cntr = (size - 1) / 2;
  const double limit = 4096.0 * 4096.0;
  BYTE* basep = (BYTE*)lr.pBits;
  for (int y = 0; y < size; y++)
  {
    DWORD* rowp = (DWORD*)(basep + y * lr.Pitch);
    for (int x = 0; x < size; x++)
    {
      int sx = (x - cntr) << scale, sy = (y - cntr) << scale;
      double dist2 = (double)sx * sx + (double)sy * sy;
      double term = dist2 <= limit ? (limit - dist2) : 0.0;
      double shifted = (double)((unsigned int)term & 0x1ff0000u) * 256.0;
      unsigned int carry = shifted >= 4294967296.0 ? 1u : 0u;
      unsigned int e = (unsigned int)(shifted - 4294967296.0 * floor(shifted / 4294967296.0)) - carry;
      unsigned int h = (e >> 24) & 0xff;
      unsigned int v = h * h;
      v = (unsigned int)((double)v * v / 65536.0);
      v = ((unsigned int)((double)v * v / 65536.0)) & 0xff00;
      unsigned int b = v >> 8;
      rowp[x] = (b << 24) | (b << 16) | (b << 8) | b;
    }
  }
  tex->UnlockRect(0);
  *out = tex;
  return true;
}

// Radial exp() falloff for the plasma: value = exp(-rho*6), rho = 2*dist from
// centre (0 at centre, 1 at edge). Stored in all channels; linear texture.
static bool BuildPlasmaTexture(LPDIRECT3DDEVICE8 dev, LPDIRECT3DTEXTURE8* out)
{
  const int size = 128;
  LPDIRECT3DTEXTURE8 tex = 0;
  if (FAILED(dev->CreateTexture(size, size, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_MANAGED, &tex)))
    return false;
  D3DLOCKED_RECT lr;
  if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); return false; }
  BYTE* basep = (BYTE*)lr.pBits;
  for (int y = 0; y < size; y++)
  {
    DWORD* rowp = (DWORD*)(basep + y * lr.Pitch);
    for (int x = 0; x < size; x++)
    {
      f32 dx = (f32)x / (size - 1) - 0.5f, dy = (f32)y / (size - 1) - 0.5f;
      f32 rho = 2.0f * (f32)sqrt(dx * dx + dy * dy);
      f32 v = (f32)exp(-rho * 6.0);
      unsigned int b = (unsigned int)(v * 255.0f + 0.5f); if (b > 255) b = 255;
      rowp[x] = (b << 24) | (b << 16) | (b << 8) | b;
    }
  }
  tex->UnlockRect(0);
  *out = tex;
  return true;
}

/* ----------------------------------------------- real-shader blob infra --- */
struct UsVtx { f32 x, y, z; };          // stream0: unit-sphere position
struct ChVtx { f32 x, y, z, w; };       // stream1: accumulated normal + displacement

// Vertex declarations matching blob.cpp.
static const DWORD kBlobDecl[] = {
  D3DVSD_STREAM(0), D3DVSD_REG(0, D3DVSDT_FLOAT3),
  D3DVSD_STREAM(1), D3DVSD_REG(1, D3DVSDT_FLOAT4),
  D3DVSD_END()
};
static const DWORD kBlobletDecl[] = {
  D3DVSD_STREAM(0), D3DVSD_REG(0, D3DVSDT_FLOAT3),
  D3DVSD_END()
};

static DWORD LoadPS(LPDIRECT3DDEVICE8 dev, const BYTE* blob)
{
  const D3DPIXELSHADERDEF_FILE* f = (const D3DPIXELSHADERDEF_FILE*)blob;
  DWORD h = 0;
  dev->CreatePixelShader((D3DPIXELSHADERDEF*)&f->Psd, &h);
  return h;
}
static DWORD LoadVS(LPDIRECT3DDEVICE8 dev, const DWORD* decl, const BYTE* blob)
{
  DWORD h = 0;
  dev->CreateVertexShader(decl, (const DWORD*)blob, &h, 0);
  return h;
}

// VectorToRGBA: unit vector -> ARGB (decoded in the pixel shader via _bx2).
static DWORD VecToRGBA(f32 x, f32 y, f32 z)
{
  int r = (int)((x + 1.0f) * 127.5f);
  int g = (int)((y + 1.0f) * 127.5f);
  int b = (int)((z + 1.0f) * 127.5f);
  return ((DWORD)255 << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
}

// tex_gen::CreateNormalizationCubeMap — each texel encodes its normalized dir.
static LPDIRECT3DCUBETEXTURE8 BuildNormalizationCube(LPDIRECT3DDEVICE8 dev, int size)
{
  LPDIRECT3DCUBETEXTURE8 cube = 0;
  if (FAILED(dev->CreateCubeTexture(size, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &cube)))
    return 0;
  std::vector<DWORD> src(size * size);
  for (int f = 0; f < 6; f++)
  {
    LPDIRECT3DSURFACE8 face = 0;
    if (FAILED(cube->GetCubeMapSurface((D3DCUBEMAP_FACES)f, 0, &face))) { cube->Release(); return 0; }
    DWORD* p = &src[0];
    for (int y = 0; y < size; y++)
    {
      f32 h = ((f32)y / (size - 1)) * 2.0f - 1.0f;
      for (int x = 0; x < size; x++)
      {
        f32 w = ((f32)x / (size - 1)) * 2.0f - 1.0f;
        f32 nx, ny, nz;
        switch (f) {
          case 0: nx = 1;  ny = -h; nz = -w; break;   // +X
          case 1: nx = -1; ny = -h; nz = w;  break;   // -X
          case 2: nx = w;  ny = 1;  nz = h;  break;   // +Y
          case 3: nx = w;  ny = -1; nz = -h; break;   // -Y
          case 4: nx = w;  ny = -h; nz = 1;  break;   // +Z
          default: nx = -w; ny = -h; nz = -1; break;  // -Z
        }
        f32 l = (f32)sqrt(nx * nx + ny * ny + nz * nz); if (l < 1e-6f) l = 1.0f;
        *p++ = VecToRGBA(nx / l, ny / l, nz / l);
      }
    }
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(face->LockRect(&lr, 0, 0)))
    {
      XGSwizzleRect(&src[0], 0, NULL, lr.pBits, size, size, NULL, sizeof(DWORD));
      face->UnlockRect();
    }
    face->Release();
  }
  return cube;
}

// Fill a static stream-0 VB with unit-sphere positions in triangle-strip order.
static void FillUsStrip(LPDIRECT3DVERTEXBUFFER8 vb, const f32* pos, const u16* idx, int n)
{
  if (!vb) return;
  UsVtx* v = 0;
  if (FAILED(vb->Lock(0, 0, (BYTE**)&v, 0))) return;
  for (int k = 0; k < n; k++) { int p = idx[k]; v[k].x = pos[p*3]; v[k].y = pos[p*3+1]; v[k].z = pos[p*3+2]; }
  vb->Unlock();
}

/* --------------------------------------------------------------- CFlubber - */
CFlubber::CFlubber()
  : m_dev(null), m_x(0), m_y(0), m_w(0), m_h(0), m_time(0.0f),
    m_eBase(0.0f), m_ePulse(0.0f), m_eBlob(0.0f),
    m_blob(null), m_blobStripVerts(0), m_blobletStripVerts(0),
    m_vsBlob(0), m_psBlob(0), m_vsBloblet(0), m_psBloblet(0), m_normCube(null),
    m_blobUsVB(null), m_blobChVB(null), m_blobletUsVB(null),
    m_glowTex(null), m_haloVB(null),
    m_plasmaTex(null), m_plasmaVB(null)
{
  m_view = Mat4::Identity();
  m_proj = Mat4::Identity();
}

CFlubber::~CFlubber()
{
  Release();
}

bool CFlubber::RestoreDevice(LPDIRECT3DDEVICE8 device, int x, int y, int width, int height,
                             const std::string& iniPath)
{
  m_dev = device;
  m_x = x; m_y = y; m_w = width; m_h = height;
  if (!m_dev || m_w <= 0 || m_h <= 0)
    return false;

  m_theme.Load(iniPath);          // missing ini just keeps the stock look
  m_time = 0.0f;

  m_proj = BuildPerspectiveFovLH(PI * 0.25f, (f32)m_w / (f32)m_h, 0.4f, 800.0f);

  m_camera.Build(m_theme.cameraMode);

  // app RNG: pulses are drawn first, then the shield manager continues from the
  // SAME generator (shields.js shares state.pulseRand). The blob sim owns a
  // separate, identically-seeded RNG, so it doesn't clash.
  m_appRand.Init(0x76543210);
  MakePulses(m_appRand, m_pulses);

  // Blob body: strip-ordered dynamic vertex buffer (we expand the shared-vertex
  // strip into DrawPrimitive order to use the 3-arg Xbox DrawPrimitive).
  m_blob = new CBlobSim();
  m_blobStripVerts    = m_blob->StripIndexCount();
  m_blobletStripVerts = m_blob->BlobletIndexCount();

  // Real-shader blob pipeline: load vblob/vbloblet + the normalization cubemap.
  m_vsBlob    = LoadVS(m_dev, kBlobDecl, g_vblob_xvu);
  m_psBlob    = LoadPS(m_dev, g_vblob_xpu);
  m_vsBloblet = LoadVS(m_dev, kBlobletDecl, g_vbloblet_xvu);
  m_psBloblet = LoadPS(m_dev, g_vbloblet_xpu);
  m_normCube  = BuildNormalizationCube(m_dev, 64);

  // stream0 (static): blob unit-sphere positions in strip order.
  m_dev->CreateVertexBuffer(m_blobStripVerts * sizeof(UsVtx), 0, 0, D3DPOOL_DEFAULT, &m_blobUsVB);
  FillUsStrip(m_blobUsVB, m_blob->UnitPos(), m_blob->StripIndices(), m_blobStripVerts);
  // stream1 (dynamic): changing (nx,ny,nz,disp), rebuilt each frame.
  m_dev->CreateVertexBuffer(m_blobStripVerts * sizeof(ChVtx), 0, 0, D3DPOOL_DEFAULT, &m_blobChVB);
  // bloblet stream0 (static).
  m_dev->CreateVertexBuffer(m_blobletStripVerts * sizeof(UsVtx), 0, 0, D3DPOOL_DEFAULT, &m_blobletUsVB);
  FillUsStrip(m_blobletUsVB, m_blob->BlobletPos(), m_blob->BlobletIndices(), m_blobletStripVerts);

  // Static scene meshes (per-mesh expanded triangle-list buffers).
  m_scene.Create(m_dev);

  // Shields: geometry + initial state (drawing from m_appRand, after the pulses).
  m_shieldMgr.Build(m_appRand);
  m_shieldMgr.CreateBuffers(m_dev);

  // Blob halo: procedural glow texture + a 4-vertex camera-facing billboard.
  BuildGlowTexture(m_dev, &m_glowTex);
  m_dev->CreateVertexBuffer(4 * sizeof(HaloVtx), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                            D3DFVF_XYZ | D3DFVF_TEX1, D3DPOOL_DEFAULT, &m_haloVB);

  // Plasma background glow: an exp() falloff texture + a screen-space quad.
  BuildPlasmaTexture(m_dev, &m_plasmaTex);
  m_dev->CreateVertexBuffer(4 * sizeof(PlasmaVtx), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                            D3DFVF_XYZRHW | D3DFVF_TEX1, D3DPOOL_DEFAULT, &m_plasmaVB);
  return true;
}

void CFlubber::Release()
{
  m_shieldMgr.Release();
  m_scene.Release();
  if (m_dev)
  {
    if (m_psBlob)    m_dev->DeletePixelShader(m_psBlob);
    if (m_psBloblet) m_dev->DeletePixelShader(m_psBloblet);
    if (m_vsBlob)    m_dev->DeleteVertexShader(m_vsBlob);
    if (m_vsBloblet) m_dev->DeleteVertexShader(m_vsBloblet);
  }
  m_psBlob = m_psBloblet = m_vsBlob = m_vsBloblet = 0;
  SAFE_RELEASE(m_plasmaVB);
  SAFE_RELEASE(m_plasmaTex);
  SAFE_RELEASE(m_haloVB);
  SAFE_RELEASE(m_glowTex);
  SAFE_RELEASE(m_normCube);
  SAFE_RELEASE(m_blobletUsVB);
  SAFE_RELEASE(m_blobChVB);
  SAFE_RELEASE(m_blobUsVB);
  SAFE_DELETE(m_blob);
  m_dev = null;   // XBMC / the runner owns the device
}

void CFlubber::Update(f32 dtSeconds)
{
  if (dtSeconds < 0.0f)       dtSeconds = 0.0f;
  else if (dtSeconds > 0.25f) dtSeconds = 0.25f;

  m_time += dtSeconds;
  while (m_time >= FINISH_START_TIME)
    m_time -= FINISH_START_TIME;   // blob/shield sims restart on rewind
}

void CFlubber::SetupFrame()
{
  CamShot shot = m_camera.Sample(m_time);
  m_eye = shot.pos;
  m_look = shot.look;
  m_view = BuildLookAtLH(shot.pos, shot.look, Vec3(0.0f, 0.0f, 1.0f));

  D3DMATRIX world = Mat4::Identity().ToD3D();
  D3DMATRIX view  = m_view.ToD3D();
  D3DMATRIX proj  = m_proj.ToD3D();
  m_dev->SetTransform(D3DTS_WORLD, &world);
  m_dev->SetTransform(D3DTS_VIEW, &view);
  m_dev->SetTransform(D3DTS_PROJECTION, &proj);
}

void CFlubber::SetBaseState()
{
  d3dSetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  d3dSetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  d3dSetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  d3dSetRenderState(D3DRS_FOGENABLE, FALSE);
  d3dSetRenderState(D3DRS_LIGHTING, FALSE);
  d3dSetRenderState(D3DRS_COLORVERTEX, TRUE);
  d3dSetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}

bool CFlubber::Draw()
{
  if (!m_dev)
    return false;

  SetupFrame();
  m_dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
  SetBaseState();

  // Intensity once per frame; advance the blob sim before any pass reads it
  // (the scene light position depends on the bloblet positions).
  Energy e = IntensityAt(m_time, m_pulses);
  m_eBase = e.base; m_ePulse = e.pulse; m_eBlob = e.blob;
  if (m_blob) m_blob->Seek(m_time);

  if (m_theme.sceneRender)  DrawScene();
  if (m_theme.blobRender)   DrawBlob();
  if (m_theme.shieldRender) DrawShields();
  if (m_theme.plasmaRender) DrawPlasma();

  return true;
}

/* ------------------------------------------------------------- DrawBlob --- */
// blob::render body pass. The WebGL original does per-vertex transform + a
// fresnel fragment; here we fold both into a per-vertex CPU pass and draw with
// the fixed-function pipeline (vertex diffuse selected straight through). The
// camera-facing halo and the bloblets are added in a later slice.
// Build c4-c7 = transpose(view*proj), reused by the blob + bloblet vertex shaders.
static void BuildVpTranspose(const Mat4& view, const Mat4& proj, f32 out[16])
{
  Mat4 vp = view * proj;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      out[i * 4 + j] = vp.m[j * 4 + i];
}

void CFlubber::DrawBlob()
{
  if (!m_blob)
    return;

  // The blob sim was advanced in Draw(); the scene may have left lighting on.
  d3dSetRenderState(D3DRS_LIGHTING, FALSE);
  d3dSetRenderState(D3DRS_SPECULARENABLE, FALSE);

  // The blob/halo/bloblets bake world-space positions, so they need an identity
  // world transform. DrawScene left WORLD set to the last instance's matrix, so
  // reset it here (otherwise the blob is flung into the scene and vanishes).
  D3DMATRIX ident = Mat4::Identity().ToD3D();
  m_dev->SetTransform(D3DTS_WORLD, &ident);

  f32 pulse = m_ePulse < 0.0f ? 0.0f : m_ePulse;
  f32 curRad = BLOB_RADIUS * (1.0f + 1.3f * (f32)sqrt(pulse));

  // --- Halo: additive camera-facing glow billboard (drawn first) -----------
  if (m_glowTex && m_haloVB)
  {
    Vec3 z = Normalize(m_look - m_eye);
    Vec3 x = Cross(z, Vec3(0.0f, 0.0f, 1.0f));
    if (Length(x) < 1e-5f) x = Vec3(1.0f, 0.0f, 0.0f);
    x = Normalize(x);
    Vec3 y = Cross(x, z);
    f32 fRad = curRad * 5.2f;
    static const f32 cs[4][4] = { {-1,1,0,1}, {1,1,1,1}, {1,-1,1,0}, {-1,-1,0,0} };
    HaloVtx* hv = 0;
    if (SUCCEEDED(m_haloVB->Lock(0, 0, (BYTE**)&hv, 0)))
    {
      for (int c = 0; c < 4; c++)
      {
        f32 sx = cs[c][0], sy = cs[c][1];
        hv[c].x = (x.x * sx + y.x * sy) * fRad;
        hv[c].y = (x.y * sx + y.y * sy) * fRad;
        hv[c].z = (x.z * sx + y.z * sy) * fRad;
        hv[c].u = cs[c][2]; hv[c].v = cs[c][3];
      }
      m_haloVB->Unlock();

      f32 ha = Clampf(m_eBlob, 0.0f, 1.0f);
      CRGBA tf(m_theme.blobGlow.r, m_theme.blobGlow.g, m_theme.blobGlow.b, ha);
      d3dSetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
      d3dSetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
      d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
      d3dSetRenderState(D3DRS_ZWRITEENABLE, FALSE);
      d3dSetRenderState(D3DRS_TEXTUREFACTOR, tf.RenderColor());
      m_dev->SetTexture(0, m_glowTex);
      d3dSetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
      d3dSetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
      d3dSetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
      d3dSetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
      d3dSetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
      d3dSetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
      d3dSetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
      d3dSetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
      // The glow is a LINEAR-format texture: Xbox requires CLAMP addressing (WRAP
      // is rejected by the GPU) and no mip filter.
      d3dSetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
      d3dSetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
      d3dSetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
      d3dSetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
      d3dSetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
      m_dev->SetStreamSource(0, m_haloVB, sizeof(HaloVtx));
      m_dev->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);
      m_dev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    }
  }

  if (!m_vsBlob || !m_psBlob || !m_blobUsVB || !m_blobChVB)
    return;

  // --- Body: real vblob vertex+pixel shaders (fresnel via normalization cube).
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  d3dSetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  d3dSetRenderState(D3DRS_FILLMODE, m_theme.blobWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID);

  // Normalization cubemap on t0 (normal) and t1 (eye); CLAMP + LINEAR, no mip.
  for (int s = 0; s < 2; s++)
  {
    d3dSetTextureStageState(s, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    d3dSetTextureStageState(s, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    d3dSetTextureStageState(s, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    d3dSetTextureStageState(s, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    d3dSetTextureStageState(s, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    d3dSetTextureStageState(s, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
  }
  m_dev->SetTexture(0, m_normCube);
  m_dev->SetTexture(1, m_normCube);

  m_dev->SetVertexShader(m_vsBlob);
  m_dev->SetPixelShader(m_psBlob);

  f32 vpT[16]; BuildVpTranspose(m_view, m_proj, vpT);
  m_dev->SetVertexShaderConstant(4, vpT, 4);

  // c8=(0,1,2,0.5), c9=eye, c10=scaling, c11=1/scaling, c12=center.
  f32 vc5[20];
  vc5[0]=0; vc5[1]=1; vc5[2]=2; vc5[3]=0.5f;
  vc5[4]=m_eye.x; vc5[5]=m_eye.y; vc5[6]=m_eye.z; vc5[7]=1.0f;
  vc5[8]=curRad; vc5[9]=curRad; vc5[10]=curRad; vc5[11]=1.0f;
  vc5[12]=1.0f/curRad; vc5[13]=1.0f/curRad; vc5[14]=1.0f/curRad; vc5[15]=1.0f;
  vc5[16]=0; vc5[17]=0; vc5[18]=0; vc5[19]=0;
  // Split into 4+1 registers so we only use the exported SetVertexShaderConstant4
  // / SetVertexShaderConstant1 (the 5-count path would hit NotInline, which the
  // XBMC wrapper doesn't export).
  m_dev->SetVertexShaderConstant(8, vc5, 4);        // c8-c11
  m_dev->SetVertexShaderConstant(12, &vc5[16], 1);  // c12

  // colour intensity: steady (no start fade-in; m_eBase is held at a mature level).
  f32 colorIntensity = BLOB_BASE_INTENSITY + 4.0f * (1.2f * m_eBase + 0.8f * m_ePulse);
  f32 pc[8];
  pc[0]=m_theme.blobColor.r*colorIntensity; pc[1]=m_theme.blobColor.g*colorIntensity;
  pc[2]=m_theme.blobColor.b*colorIntensity; pc[3]=1.0f;      // c0 = blob colour (a=opaque)
  pc[4]=0; pc[5]=0; pc[6]=0; pc[7]=0;                        // c1 = ambient (black)
  m_dev->SetPixelShaderConstant(0, pc, 2);

  // stream1 (changing) in strip order.
  {
    const f32* ch = m_blob->Changing();
    const u16* idx = m_blob->StripIndices();
    ChVtx* cv = 0;
    if (SUCCEEDED(m_blobChVB->Lock(0, 0, (BYTE**)&cv, 0)))
    {
      for (int k = 0; k < m_blobStripVerts; k++)
      { int p = idx[k]; cv[k].x = ch[p*4]; cv[k].y = ch[p*4+1]; cv[k].z = ch[p*4+2]; cv[k].w = ch[p*4+3]; }
      m_blobChVB->Unlock();
    }
  }
  m_dev->SetStreamSource(0, m_blobUsVB, sizeof(UsVtx));
  m_dev->SetStreamSource(1, m_blobChVB, sizeof(ChVtx));
  m_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, m_blobStripVerts - 2);

  d3dSetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

  // --- Bloblets: real vbloblet shaders (anisotropic ellipsoid + fresnel) ----
  int nb = m_blob->numBloblets;
  if (nb > 0 && m_vsBloblet && m_psBloblet && m_blobletUsVB && m_blobletStripVerts >= 3)
  {
    m_dev->SetVertexShader(m_vsBloblet);
    m_dev->SetPixelShader(m_psBloblet);
    m_dev->SetVertexShaderConstant(4, vpT, 4);

    f32 vc2[8];
    vc2[0]=0; vc2[1]=1; vc2[2]=2; vc2[3]=0.5f;                 // c8
    vc2[4]=m_eye.x; vc2[5]=m_eye.y; vc2[6]=m_eye.z; vc2[7]=1.0f; // c9
    m_dev->SetVertexShaderConstant(8, vc2, 1);        // c8
    m_dev->SetVertexShaderConstant(9, &vc2[4], 1);    // c9

    f32 eb = m_eBlob;
    f32 bp[12];
    bp[0]=m_theme.blobColor.r*0.3f*eb; bp[1]=m_theme.blobColor.g*0.3f*eb; bp[2]=m_theme.blobColor.b*0.3f*eb; bp[3]=1.0f; // c0
    bp[4]=m_theme.blobColor.r*0.2f;    bp[5]=m_theme.blobColor.g*0.2f;    bp[6]=m_theme.blobColor.b*0.2f;    bp[7]=1.0f; // c1
    bp[8]=2.0f; bp[9]=2.0f; bp[10]=2.0f; bp[11]=2.0f;                                                                    // c2
    m_dev->SetPixelShaderConstant(0, bp, 3);

    m_dev->SetStreamSource(0, m_blobletUsVB, sizeof(UsVtx));

    for (int d = 0; d < nb; d++)
    {
      const Bloblet& drop = m_blob->bloblets[d];
      f32 w = drop.fWobble; if (w < 1e-6f) w = 1e-6f;
      f32 perp = drop.fRadius / (f32)sqrt(w);
      f32 pmp = drop.fRadius * drop.fWobble - perp;
      f32 bv[16];
      bv[0]=drop.vPosition[0]; bv[1]=drop.vPosition[1]; bv[2]=drop.vPosition[2]; bv[3]=0.0f;   // c10 center
      bv[4]=drop.vDirection[0]; bv[5]=drop.vDirection[1]; bv[6]=drop.vDirection[2]; bv[7]=1.0f; // c11 dir
      bv[8]=perp; bv[9]=perp; bv[10]=perp; bv[11]=1.0f;                                         // c12 perp
      bv[12]=pmp*drop.vDirection[0]; bv[13]=pmp*drop.vDirection[1]; bv[14]=pmp*drop.vDirection[2]; bv[15]=1.0f; // c13
      m_dev->SetVertexShaderConstant(10, bv, 4);
      m_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, m_blobletStripVerts - 2);
    }
  }

  // Restore: drop the shaders so later passes use fixed-function again.
  m_dev->SetPixelShader(NULL);
  m_dev->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE);
  m_dev->SetTexture(0, NULL);
  m_dev->SetTexture(1, NULL);
  m_dev->SetStreamSource(1, NULL, 0);
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

// --- scene geometry: 271 instances, hardware point light (scene_phong) -----
void CFlubber::DrawScene()
{
  if (!m_blob)
    return;
  f32 fpos = (m_time - SCENE_ANIM_START_TIME) / SCENE_ANIM_LEN;
  m_scene.Draw(m_dev, fpos, m_blob, m_theme, m_eye, m_eBlob);
}

// --- shields: 3 shields + 5 zshields, translucent glinting panels ----------
void CFlubber::DrawShields()
{
  m_shieldMgr.Draw(m_dev, m_time, m_theme, m_eye, m_look, m_eBlob);
}

// --- plasma / fog: additive radial glow centred on the blob's screen pos ---
// FS_FOG = C1*uI*exp(-d*3.2) + C2*uGlow*exp(-d*1.6) + C3*uGlow*exp(-d*5.0), with
// d the screen-UV distance from the blob origin. Realised as three additive
// screen-space quads sampling an exp() texture, each sized span=6/k so the
// texture's exp(-rho*6) profile reproduces exp(-d*k).
void CFlubber::DrawPlasma()
{
  if (!m_plasmaTex || !m_plasmaVB || m_w <= 0 || m_h <= 0)
    return;

  // Project the blob origin (0,0,0) to screen UV (D3D y-down).
  Mat4 vp = m_view * m_proj;
  f32 ow = vp.m[15]; if (ow == 0.0f) ow = 1.0f;
  f32 ndcx = vp.m[12] / ow, ndcy = vp.m[13] / ow;
  f32 ou = ndcx * 0.5f + 0.5f;
  f32 ov = 0.5f - ndcy * 0.5f;

  f32 uI = m_eBlob * 0.7f - 0.1f; if (uI < 0.0f) uI = 0.0f;
  f32 glow;
  if (m_time < BLOB_STATIC_END_TIME)
  {
    f32 u = m_time < 0.12f ? m_time / 0.12f : 1.0f - (m_time - 0.12f) / BLOB_STATIC_END_TIME;
    glow = Clampf(u, 0.0f, 1.0f);
  }
  else glow = 0.75f * Clampf((m_time - GLOW_FADE_SCREEN_START) / 0.25f, 0.0f, 1.0f);

  const CRGBA cols[3] = { m_theme.plasma1, m_theme.plasma2, m_theme.plasma3 };
  const f32 scale[3] = { uI, glow, glow };
  const f32 krate[3] = { 3.2f, 1.6f, 5.0f };

  bool any = false;
  for (int i = 0; i < 3; i++) if (scale[i] > 0.001f) any = true;
  if (!any) return;

  // Overlay state: additive, no depth test/write.
  d3dSetRenderState(D3DRS_LIGHTING, FALSE);
  d3dSetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  d3dSetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
  d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  m_dev->SetTexture(0, m_plasmaTex);
  d3dSetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
  d3dSetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  d3dSetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
  d3dSetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
  d3dSetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
  d3dSetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
  d3dSetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
  d3dSetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
  d3dSetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
  d3dSetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
  d3dSetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
  d3dSetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
  m_dev->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);

  f32 W = (f32)m_w, H = (f32)m_h;
  // screen corners as a triangle strip: TL, TR, BL, BR
  const f32 cx[4] = { 0.0f, W, 0.0f, W };
  const f32 cy[4] = { 0.0f, 0.0f, H, H };

  for (int i = 0; i < 3; i++)
  {
    if (scale[i] <= 0.001f) continue;
    f32 span = 6.0f / krate[i];
    CRGBA tf(Clampf(cols[i].r * scale[i], 0.0f, 1.0f),
             Clampf(cols[i].g * scale[i], 0.0f, 1.0f),
             Clampf(cols[i].b * scale[i], 0.0f, 1.0f), 1.0f);
    d3dSetRenderState(D3DRS_TEXTUREFACTOR, tf.RenderColor());

    PlasmaVtx* v = 0;
    if (FAILED(m_plasmaVB->Lock(0, 0, (BYTE**)&v, 0))) continue;
    for (int c = 0; c < 4; c++)
    {
      f32 u = cx[c] / W, vv = cy[c] / H;   // normalized screen (0..1)
      v[c].x = cx[c] - 0.5f; v[c].y = cy[c] - 0.5f; v[c].z = 0.0f; v[c].rhw = 1.0f;
      v[c].u = ((u - ou) / span) * 0.5f + 0.5f;
      v[c].v = ((vv - ov) / span) * 0.5f + 0.5f;
    }
    m_plasmaVB->Unlock();
    m_dev->SetStreamSource(0, m_plasmaVB, sizeof(PlasmaVtx));
    m_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }

  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  d3dSetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
}
