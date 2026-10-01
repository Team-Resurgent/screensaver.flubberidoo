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

/* --------------------------------------------------------------- CFlubber - */
CFlubber::CFlubber()
  : m_dev(null), m_x(0), m_y(0), m_w(0), m_h(0), m_time(0.0f),
    m_eBase(0.0f), m_ePulse(0.0f), m_eBlob(0.0f),
    m_blob(null), m_blobVB(null), m_blobStripVerts(0)
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
  return true;
}

void CFlubber::Release()
{
  m_shieldMgr.Release();
  m_scene.Release();
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

  f32 pulse = m_ePulse < 0.0f ? 0.0f : m_ePulse;
  f32 curRad = BLOB_RADIUS * (1.0f + 1.3f * (f32)sqrt(pulse));

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

// --- pass hooks (filled in by later slices) ------------------------------
void CFlubber::DrawPlasma()  {}
