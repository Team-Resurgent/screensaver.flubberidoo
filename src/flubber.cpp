/*
 *  flubber.cpp — Flubberidoo engine (see flubber.h).
 *
 *  This slice establishes the frame contract: acquire the device, load the
 *  theme, run the looping clock, set up the camera transforms and baseline
 *  render state, and clear to black. The individual layers (scene, blob,
 *  shields, plasma) are wired in as separate slices behind the Draw* hooks.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "main.h"      // d3dSetRenderState / d3dSetTextureStageState wrappers
#include "flubber.h"

using namespace flubtime;

CFlubber::CFlubber()
  : m_dev(null), m_x(0), m_y(0), m_w(0), m_h(0), m_time(0.0f)
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

  // Theme is optional: a missing/garbled ini just leaves the stock green look.
  m_theme.Load(iniPath);

  m_time = 0.0f;

  // Fixed projection: 45 deg vertical FOV, near 0.4, far 800 (FlubberForge).
  m_proj = BuildPerspectiveFovLH(PI * 0.25f, (f32)m_w / (f32)m_h, 0.4f, 800.0f);

  return true;
}

void CFlubber::Release()
{
  // We do not own the device (XBMC / the runner does); just drop the reference.
  m_dev = null;
}

void CFlubber::Update(f32 dtSeconds)
{
  // Guard against pauses / bad frames producing huge or negative steps.
  if (dtSeconds < 0.0f)       dtSeconds = 0.0f;
  else if (dtSeconds > 0.25f) dtSeconds = 0.25f;

  m_time += dtSeconds;

  // Loop the "variable" window: cut before the finish dive + logo. The blob and
  // shield sims restart deterministically when their seek() sees time rewind.
  while (m_time >= FINISH_START_TIME)
    m_time -= FINISH_START_TIME;
}

void CFlubber::SetupFrame()
{
  // TODO(camera slice): replace this fixed shot with the TCB-Hermite camera
  // spline (camera.js) sampled at m_time. For now, framing from path 0's first
  // node so the scene/blob land roughly centred once those slices arrive.
  Vec3 eye(11.4f, -32.1f, 33.0f);
  Vec3 at(0.0f, 0.0f, 0.0f);
  Vec3 up(0.0f, 0.0f, 1.0f);   // world is Z-up
  m_view = BuildLookAtLH(eye, at, up);

  if (m_dev)
  {
    D3DMATRIX world = Mat4::Identity().ToD3D();
    D3DMATRIX view  = m_view.ToD3D();
    D3DMATRIX proj  = m_proj.ToD3D();
    m_dev->SetTransform(D3DTS_WORLD, &world);
    m_dev->SetTransform(D3DTS_VIEW, &view);
    m_dev->SetTransform(D3DTS_PROJECTION, &proj);
  }
}

void CFlubber::SetBaseState()
{
  // Baseline matches drawOfficial's per-frame reset: depth on, no cull, no blend.
  d3dSetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  d3dSetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  d3dSetRenderState(D3DRS_LIGHTING, FALSE);
  d3dSetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}

bool CFlubber::Draw()
{
  if (!m_dev)
    return false;

  SetupFrame();

  // Pure black background; depth cleared for the opaque scene/blob.
  m_dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

  SetBaseState();

  // Layer order (FlubberForge drawOfficial): scene -> blob -> shields -> plasma.
  // Hooks are no-ops until their slices land.
  if (m_theme.sceneRender)  DrawScene();
  if (m_theme.blobRender)   DrawBlob();
  if (m_theme.shieldRender) DrawShields();
  if (m_theme.plasmaRender) DrawPlasma();

  return true;
}

// --- pass hooks (filled in by later slices) ------------------------------
void CFlubber::DrawScene()   {}
void CFlubber::DrawBlob()    {}
void CFlubber::DrawShields() {}
void CFlubber::DrawPlasma()  {}
