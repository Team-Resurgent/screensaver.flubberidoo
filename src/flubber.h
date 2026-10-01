/*
 *  flubber.h — Flubberidoo engine: a looping D3D8 re-creation of the Xbox boot
 *  animation (blob + scene + shields + plasma), themed from bootanim.ini.
 *
 *  Analogous to the old CMatrixTrails engine: the addon adapter (main.cpp) owns
 *  one CFlubber, hands it the device + screen rect + ini path in RestoreDevice(),
 *  then calls Update(dt)/Draw() each frame. The clock loops in [0, FINISH_START)
 *  so there is no ending Xbox logo and no sound.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include "flubmath.h"
#include "flubconst.h"
#include "theme.h"
#include "blob.h"
#include "camera.h"
#include "scene.h"
#include "shields.h"

#include <xtl.h>
#include <string>

// The loopable window is [0, flubtime::FINISH_START_TIME); everything at/after
// it is the finish dive + logo, which this screensaver never renders.

// Fixed-function vertex for the blob body: world-space position + packed diffuse.
struct BlobVtx { f32 x, y, z; DWORD color; };
// Halo billboard vertex: world-space position + glow-texture UV.
struct HaloVtx { f32 x, y, z, u, v; };
// Pre-transformed (screen-space) vertex for the plasma overlay quad.
struct PlasmaVtx { f32 x, y, z, rhw, u, v; };

// One of the twelve intensity pulses (anim.js makePulses).
struct FlubPulse { f32 x, y, z; };

class CFlubber
{
public:
  CFlubber();
  ~CFlubber();

  // Acquire the render target and load the theme. 'iniPath' is the full path to
  // bootanim.ini (may be empty / missing -> stock green theme).
  bool RestoreDevice(LPDIRECT3DDEVICE8 device, int x, int y, int width, int height,
                     const std::string& iniPath);
  void Release();

  void Update(f32 dtSeconds);   // advance + loop the clock
  bool Draw();                  // render one frame (false => fatal, caller stops)

private:
  void SetupFrame();            // camera -> view/proj, clear, baseline state
  void SetBaseState();

  // Render passes. Filled in across slices; each gated by the theme.
  void DrawScene();
  void DrawBlob();
  void DrawShields();
  void DrawPlasma();

  LPDIRECT3DDEVICE8 m_dev;
  int   m_x, m_y, m_w, m_h;
  CTheme m_theme;

  f32   m_time;                 // current time in [0, FINISH_START_TIME)
  Mat4  m_view;
  Mat4  m_proj;
  Vec3  m_eye;                  // current camera position (for blob fresnel)
  Vec3  m_look;                 // current camera look-at point (shield sort)

  CCamera   m_camera;
  QuickRand m_appRand;          // app RNG: pulses first, then shields draw from it
  FlubPulse m_pulses[12];
  f32       m_eBase, m_ePulse, m_eBlob;   // intensityAt(m_time), computed per frame

  CScene                  m_scene;
  CShieldManager          m_shieldMgr;
  CBlobSim*               m_blob;
  int                     m_blobStripVerts;
  int                     m_blobletStripVerts;

  // Real-shader blob pipeline (vblob / vbloblet) + normalization cubemap.
  DWORD                   m_vsBlob, m_psBlob, m_vsBloblet, m_psBloblet;
  LPDIRECT3DCUBETEXTURE8  m_normCube;     // normalization cubemap (t0/t1)
  LPDIRECT3DVERTEXBUFFER8 m_blobUsVB;     // stream0 static: unit-sphere pos (strip)
  LPDIRECT3DVERTEXBUFFER8 m_blobChVB;     // stream1 dynamic: changing nx,ny,nz,disp
  LPDIRECT3DVERTEXBUFFER8 m_blobletUsVB;  // bloblet stream0 static: unit-sphere pos

  LPDIRECT3DTEXTURE8      m_glowTex;   // procedural radial glow (halo)
  LPDIRECT3DVERTEXBUFFER8 m_haloVB;    // 4-vertex camera-facing billboard

  LPDIRECT3DTEXTURE8      m_plasmaTex; // radial exp() falloff for the plasma
  LPDIRECT3DVERTEXBUFFER8 m_plasmaVB;  // 4-vertex screen-space overlay quad
};
