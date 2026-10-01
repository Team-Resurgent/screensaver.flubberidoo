/*
 *  shields.h — three Shields and five ZShields orbiting the blob. Each is a
 *  piece of a spherical shell (outer face + inner face + capped rim), dark and
 *  translucent, picking up tight (N.L)^16 glints from the blob light and a mood
 *  light, tinted by the Shield colour, plus an environment-cube reflection.
 *  Port of Shield.cpp / shields.js.
 *
 *  This slice draws the panels with the glint + fade + intensity; the cube-map
 *  reflection (the env term) is added in the following slice.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include "flubmath.h"
#include "theme.h"

#include <xtl.h>
#include <vector>

const f32 SHIELD_ROTATION_RATE  = 2.0f;
const int SHIELD_WIDTH          = 8;
const int SHIELD_HEIGHT         = 6;
const f32 SHIELD_INSIDE_RADIUS  = 13.1f;
const f32 SHIELD_OUTSIDE_RADIUS = 14.0f;
const f32 SHIELD_VERT_DIM       = 0.9f;
const f32 SHIELD_HORIZ_DIM      = 1.2f;
const f32 SHIELD_HORIZ_RADIANS  = 1.2f;
const int MAX_SHIELDS           = 3;
const int MAX_ZSHIELDS          = 5;

class QuickRand;   // from blob.h

// A built shell panel: outer face + rim + inner face, as one triangle strip.
struct ShieldMesh
{
  std::vector<f32> pos;   // count*3
  std::vector<f32> nrm;   // count*3
  std::vector<u16> idx;   // triangle strip (with degenerate stitches)
};

struct Shield
{
  Mat4 startRotation;
  f32  rotationDir[3];
  f32  thetaZero;
  f32  speed;
  f32  radiusScale;
  f32  objectCenter[3];
  Mat4 matrix;
  f32  center[3];
};

struct ZShield
{
  f32  theta;
  f32  speed;
  Mat4 matrix;
};

class CShieldManager
{
public:
  CShieldManager();
  ~CShieldManager();

  // Build geometry + initial state. 'rand' is the app RNG AFTER the 12 pulses
  // have been drawn (shields.js: state.pulseRand is shared with makePulses).
  void Build(QuickRand& rand);
  bool CreateBuffers(LPDIRECT3DDEVICE8 dev);   // dynamic strip VB
  void Release();

  void Seek(f32 target);   // fixed 1/60s stepping; restarts on rewind

  // Draw all panels (far pass, near pass, then zshields) at time t.
  void Draw(LPDIRECT3DDEVICE8 dev, f32 t, const CTheme& theme,
            const Vec3& eye, const Vec3& look, f32 energyBlob);

  // Test hooks (used by the host numeric-validation harness).
  const Mat4& DbgShieldMatrix(int i) const { return m_shields[i].matrix; }
  const Mat4& DbgZMatrix(int i)      const { return m_zshields[i].matrix; }
  void DbgShieldCenter(int i, f32 o[3]) const { o[0]=m_shields[i].center[0]; o[1]=m_shields[i].center[1]; o[2]=m_shields[i].center[2]; }
  int  DbgPanelVerts() const { return (int)(m_panel.pos.size() / 3); }
  int  DbgPanelIdx()   const { return (int)m_panel.idx.size(); }
  int  DbgZVerts(int i) const { return (int)(m_zmesh[i].pos.size() / 3); }

private:
  void Restart();
  void NewShield(Shield& s);
  void AdvanceShield(Shield& s, f32 dt);
  void AdvanceZShield(ZShield& z, f32 dt);
  void DrawPanel(LPDIRECT3DDEVICE8 dev, const ShieldMesh& mesh, const Mat4& world,
                 const CRGBA& tint, f32 intensity, f32 alpha);

  ShieldMesh m_panel;          // shared by the 3 shields
  ShieldMesh m_zmesh[MAX_ZSHIELDS];
  Shield     m_shields[MAX_SHIELDS];
  ZShield    m_zshields[MAX_ZSHIELDS];

  QuickRand* m_rand;           // app RNG (not owned)
  f32  m_center[3];
  f32  m_midRadius;
  f32  m_radiusScale;
  f32  m_time;

  LPDIRECT3DVERTEXBUFFER8 m_vb;   // reused per-panel dynamic buffer
  int  m_vbVerts;                 // capacity in vertices
};
