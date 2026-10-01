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

#include <math.h>

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

static Energy IntensityAt(f32 t, const FlubPulse* pulses)
{
  Energy e;
  e.base = 0.0f;
  if (t >= BLOB_ZERO_INTENSE_END)
  {
    f32 u = (t - BLOB_ZERO_INTENSE_END) / MAX_INTENSITY_DELTA;
    e.base = 0.5f * u * u + 0.5f * u;
  }
  e.pulse = (t < DEMO_TOTAL_TIME) ? SumPulses(pulses, t) : 0.0f;
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

/* --------------------------------------------------------------- CFlubber - */
CFlubber::CFlubber()
  : m_dev(null), m_x(0), m_y(0), m_w(0), m_h(0), m_time(0.0f),
    m_eBase(0.0f), m_ePulse(0.0f), m_eBlob(0.0f),
    m_blob(null), m_blobVB(null), m_blobStripVerts(0),
    m_glowTex(null), m_haloVB(null), m_blobletVB(null), m_blobletStripVerts(0),
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
  m_blobStripVerts = m_blob->StripIndexCount();
  m_blobUnique.resize(m_blob->VertexCount());

  if (FAILED(m_dev->CreateVertexBuffer(m_blobStripVerts * sizeof(BlobVtx),
                                       D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                                       D3DFVF_XYZ | D3DFVF_DIFFUSE,
                                       D3DPOOL_DEFAULT, &m_blobVB)))
  {
    m_blobVB = null;
    return false;
  }

  // Static scene meshes (per-mesh expanded triangle-list buffers). Non-fatal if
  // it fails: DrawScene guards each mesh, so the blob still renders.
  m_scene.Create(m_dev);

  // Shields: geometry + initial state (drawing from m_appRand, after the pulses).
  m_shieldMgr.Build(m_appRand);
  m_shieldMgr.CreateBuffers(m_dev);

  // Blob glow: procedural halo texture + a 4-vertex billboard, plus a dynamic
  // buffer for one bloblet's strip at a time. All non-fatal (DrawBlob guards).
  BuildGlowTexture(m_dev, &m_glowTex);
  m_dev->CreateVertexBuffer(4 * sizeof(HaloVtx), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                            D3DFVF_XYZ | D3DFVF_TEX1, D3DPOOL_DEFAULT, &m_haloVB);
  m_blobletStripVerts = m_blob->BlobletIndexCount();
  m_blobletUnique.resize(m_blob->BlobletVertCount());
  m_dev->CreateVertexBuffer(m_blobletStripVerts * sizeof(BlobVtx),
                            D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                            D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DPOOL_DEFAULT, &m_blobletVB);

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
  SAFE_RELEASE(m_plasmaVB);
  SAFE_RELEASE(m_plasmaTex);
  SAFE_RELEASE(m_blobletVB);
  SAFE_RELEASE(m_haloVB);
  SAFE_RELEASE(m_glowTex);
  SAFE_RELEASE(m_blobVB);
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
void CFlubber::DrawBlob()
{
  if (!m_blob || !m_blobVB)
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

  // --- Body (opaque) -------------------------------------------------------
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);

  f32 colorIntensity = BLOB_BASE_INTENSITY + 4.0f * (1.2f * m_eBase + 0.8f * m_ePulse);
  f32 tFade = m_time * 4.0f; if (tFade > 1.0f) tFade = 1.0f;
  colorIntensity *= tFade;

  f32 cr = m_theme.blobColor.r * colorIntensity;
  f32 cg = m_theme.blobColor.g * colorIntensity;
  f32 cb = m_theme.blobColor.b * colorIntensity;

  const f32* us = m_blob->UnitPos();
  const f32* ch = m_blob->Changing();
  int vc = m_blob->VertexCount();

  // Per unique vertex: world position (aUs*(curRad+disp)) + fresnel diffuse.
  for (int i = 0; i < vc; i++)
  {
    f32 ux = us[i * 3], uy = us[i * 3 + 1], uz = us[i * 3 + 2];
    f32 nx = ch[i * 4], ny = ch[i * 4 + 1], nz = ch[i * 4 + 2], disp = ch[i * 4 + 3];
    f32 rad = curRad + disp;
    f32 wx = ux * rad, wy = uy * rad, wz = uz * rad;

    f32 nlen = (f32)sqrt(nx * nx + ny * ny + nz * nz); if (nlen < 1e-6f) nlen = 1.0f;
    f32 Nx = nx / nlen, Ny = ny / nlen, Nz = nz / nlen;

    f32 ex = m_eye.x - wx, ey = m_eye.y - wy, ez = m_eye.z - wz;
    f32 elen = (f32)sqrt(ex * ex + ey * ey + ez * ez); if (elen < 1e-6f) elen = 1.0f;
    ex /= elen; ey /= elen; ez /= elen;

    f32 r1 = Nx * ex + Ny * ey + Nz * ez;
    if (r1 < 0.0f) r1 = 0.0f; else if (r1 > 1.0f) r1 = 1.0f;
    f32 r0 = (1.0f - r1) * (1.0f - r1);
    f32 fr = 1.0f - r0;

    CRGBA c(fr * cr, fr * cg, fr * cb, fr);   // uAmbient=0, uAlpha=1
    m_blobUnique[i].x = wx; m_blobUnique[i].y = wy; m_blobUnique[i].z = wz;
    m_blobUnique[i].color = c.RenderColor();
  }

  // Expand the shared-vertex triangle strip into DrawPrimitive order.
  const u16* idx = m_blob->StripIndices();
  BlobVtx* vb = 0;
  if (FAILED(m_blobVB->Lock(0, 0, (BYTE**)&vb, 0)))
    return;
  for (int k = 0; k < m_blobStripVerts; k++)
    vb[k] = m_blobUnique[idx[k]];
  m_blobVB->Unlock();

  // Fixed-function: pass the vertex diffuse straight through (no texture).
  m_dev->SetTexture(0, NULL);
  d3dSetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
  d3dSetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
  d3dSetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
  d3dSetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
  d3dSetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
  d3dSetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

  d3dSetRenderState(D3DRS_FILLMODE, m_theme.blobWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID);

  m_dev->SetStreamSource(0, m_blobVB, sizeof(BlobVtx));
  m_dev->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE);
  m_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, m_blobStripVerts - 2);

  d3dSetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

  // --- Bloblets (drops): anisotropic ellipsoids, alpha-blended -------------
  int nb = m_blob->numBloblets;
  if (nb > 0 && m_blobletVB && m_blobletStripVerts >= 3)
  {
    d3dSetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    d3dSetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_dev->SetTexture(0, NULL);
    d3dSetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    d3dSetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    d3dSetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    d3dSetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    m_dev->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE);

    const f32* bus = m_blob->BlobletPos();
    int bvc = m_blob->BlobletVertCount();
    const u16* bidx = m_blob->BlobletIndices();

    f32 eb = m_eBlob;
    f32 ucr = m_theme.blobColor.r * 0.3f * eb, ucg = m_theme.blobColor.g * 0.3f * eb, ucb = m_theme.blobColor.b * 0.3f * eb;
    f32 uar = m_theme.blobColor.r * 0.2f, uag = m_theme.blobColor.g * 0.2f, uab = m_theme.blobColor.b * 0.2f;
    f32 ualpha = 0.6f * eb;

    for (int d = 0; d < nb; d++)
    {
      const Bloblet& drop = m_blob->bloblets[d];
      f32 w = drop.fWobble; if (w < 1e-6f) w = 1e-6f;
      f32 perp = drop.fRadius / (f32)sqrt(w);
      f32 pmp = drop.fRadius * drop.fWobble - perp;
      f32 pmx = pmp * drop.vDirection[0], pmy = pmp * drop.vDirection[1], pmz = pmp * drop.vDirection[2];
      f32 cx = drop.vPosition[0], cy = drop.vPosition[1], cz = drop.vPosition[2];
      f32 dx = drop.vDirection[0], dy = drop.vDirection[1], dz = drop.vDirection[2];

      for (int i = 0; i < bvc; i++)
      {
        f32 ax = bus[i*3], ay = bus[i*3+1], az = bus[i*3+2];
        f32 adot = ax*dx + ay*dy + az*dz;
        f32 wx = perp*ax + adot*pmx + cx;
        f32 wy = perp*ay + adot*pmy + cy;
        f32 wz = perp*az + adot*pmz + cz;
        f32 ex = m_eye.x - wx, ey = m_eye.y - wy, ez = m_eye.z - wz;
        f32 el = (f32)sqrt(ex*ex + ey*ey + ez*ez); if (el < 1e-6f) el = 1.0f;
        ex /= el; ey /= el; ez /= el;
        f32 nlen = (f32)sqrt(ax*ax + ay*ay + az*az); if (nlen < 1e-6f) nlen = 1.0f;
        f32 r1 = (ax*ex + ay*ey + az*ez) / nlen; if (r1 < 0) r1 = 0; else if (r1 > 1) r1 = 1;
        f32 r0 = (1.0f - r1) * (1.0f - r1); f32 fr = 1.0f - r0;
        CRGBA c(fr*ucr + uar, fr*ucg + uag, fr*ucb + uab, fr*ualpha);
        m_blobletUnique[i].x = wx; m_blobletUnique[i].y = wy; m_blobletUnique[i].z = wz;
        m_blobletUnique[i].color = c.RenderColor();
      }

      BlobVtx* bv = 0;
      if (SUCCEEDED(m_blobletVB->Lock(0, 0, (BYTE**)&bv, 0)))
      {
        for (int k = 0; k < m_blobletStripVerts; k++) bv[k] = m_blobletUnique[bidx[k]];
        m_blobletVB->Unlock();
        m_dev->SetStreamSource(0, m_blobletVB, sizeof(BlobVtx));
        m_dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, m_blobletStripVerts - 2);
      }
    }
    d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  }
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
