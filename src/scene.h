/*
 *  scene.h — the static boot-animation scene: 271 instances of 24 rigid meshes,
 *  lit by scene_phong (a point light at the blob's weighted centre). Ported from
 *  FlubberForge render.js drawScene/worldOf/drawMesh.
 *
 *  The meshes never deform (only their per-instance world matrix animates), so
 *  each mesh is pre-expanded once into a static triangle-list vertex buffer
 *  (position + normal) and drawn with the 3-arg Xbox DrawPrimitive. Lighting is
 *  the fixed-function hardware point light, whose a0+a1*d+a2*d^2 attenuation is
 *  exactly scene_phong's falloff. The per-instance light position comes from
 *  CBlobSim::LightFor.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include "flubmath.h"
#include "theme.h"
#include "blob.h"

#include <xtl.h>

class CScene
{
public:
  CScene();
  ~CScene();

  bool Create(LPDIRECT3DDEVICE8 dev);    // build the static mesh buffers
  void Release();

  // Draw every instance. fpos is the scene animation parameter (clamped 0..1),
  // energyBlob is intensityAt(t).blob, camPos is the current eye.
  void Draw(LPDIRECT3DDEVICE8 dev, f32 fpos, const CBlobSim* blob,
            const CTheme& theme, const Vec3& camPos, f32 energyBlob);

  // Depth-only pass for the fog intensity map: re-draw every instance with the
  // scene_zr shader already bound by the caller (which also set c16-c19). Each
  // instance's FINAL_MAT = transpose(world * viewProj) goes to c0-c3. The bound
  // shader reads only position (REG0), so the pos+normal buffers are reused.
  void DrawZ(LPDIRECT3DDEVICE8 dev, const Mat4& viewProj, f32 fpos);

private:
  LPDIRECT3DVERTEXBUFFER8 m_vb[32];   // one per mesh (kFlubMeshCount <= 32)
  int m_vertCount[32];                // expanded triangle-list vertex count
  int m_meshCount;
};
