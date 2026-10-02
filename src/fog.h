/*
 *  fog.h — the real green-fog subsystem, ported from BootAnimRXDK green_fog.cpp.
 *
 *  Two passes per frame:
 *   (1) RenderIntensity(): render a fog-density map into an off-screen A8R8G8B8
 *       render target. A full-screen backdrop quad fills it with a camera-centred
 *       radial falloff at the far plane (scene_zr), then the scene geometry is
 *       re-drawn with scene_zr so nearer surfaces carve the density down — this is
 *       what makes the mist read as volumetric (occluded by geometry).
 *   (2) Composite(): draw one full-screen quad with the greenfog shader, sampling
 *       the density map (t0) modulated by three scroll-transformed samples of one
 *       procedural plasma texture (t1..t3), times the blob intensity, additively.
 *
 *  The boot-anim's start/finish yellow-flash pass is intentionally dropped: this
 *  is a loopable cinematic screensaver with steady illumination and no logo.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include "flubmath.h"
#include "theme.h"

#include <xtl.h>

class CScene;

// Full-screen fog composite vertex: clip-space pos + intensity UV + plasma UV.
struct FogVtx { f32 x, y, z; f32 tu0, tv0; f32 tu1, tv1; };
// Backdrop quad for the intensity pass: clip-space position only.
struct FogBackVtx { f32 x, y, z; };

// Per-frame camera state the fog needs (computed by CFlubber from the camera).
struct FogCam
{
  Mat4 viewProj;     // view * proj (world->clip)
  f32  radBlob;      // distance from eye to the blob centre
  f32  theta, phi;   // spherical angles of the eye (unwrapped across frames)
  f32  aspect;       // width / height
};

class CFog
{
public:
  CFog();
  ~CFog();

  bool Create(LPDIRECT3DDEVICE8 dev, int screenW, int screenH);
  void Release();

  // Render the fog. 'scene'/'fpos' drive the depth-carving pass; 'cam' positions
  // the plasma scroll; 'blobIntensity' is intensityAt(t).blob; theme gives colours.
  void Render(LPDIRECT3DDEVICE8 dev, CScene& scene, f32 fpos,
              const FogCam& cam, f32 blobIntensity, const CTheme& theme);

private:
  void RenderIntensity(LPDIRECT3DDEVICE8 dev, CScene& scene, f32 fpos, const FogCam& cam);

  LPDIRECT3DDEVICE8 m_dev;      // not owned; for freeing shader handles
  int m_w, m_h;                 // intensity RT size (<= backbuffer so main depth fits)

  LPDIRECT3DVERTEXBUFFER8 m_quadVB;     // FogVtx composite quad
  LPDIRECT3DVERTEXBUFFER8 m_backVB;     // FogBackVtx backdrop quad
  LPDIRECT3DTEXTURE8      m_plasmaTex;  // one A8 diamond-square plasma (bound x3)
  LPDIRECT3DTEXTURE8      m_intensityU; // written this frame
  LPDIRECT3DTEXTURE8      m_intensityR; // sampled this frame (swapped)

  DWORD m_vsFog, m_psFog;   // greenfog shaders
  DWORD m_vsZ,   m_psZ;     // scene_zr (depth) shaders
};
