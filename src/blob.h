/*
 *  blob.h — the "flubber": a cube-mapped unit sphere whose vertices are pushed
 *  out each frame by up to 32 travelling bumps, 8 of which drag a bloblet out
 *  through the surface. Port of blob.cpp / blob_bump.cpp / bloblet.cpp from
 *  BootAnimRXDK (via FlubberForge blob.js). The QuickRand sequence, the fixed
 *  1/60s stepping and the create/update order are reproduced exactly so the
 *  shape reads the same as on hardware.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include <vector>

const int   BLOB_DIM          = 32;
const int   BLOBLET_DIM       = 8;
const int   MAX_BLOBBUMPS     = 32;
const int   MAX_BLOBLETS      = 8;
const f32   BLOB_RADIUS       = 2.3f;
const f32   BLOB_SIM_DT       = 1.0f / 60.0f;
const f32   BLOB_BASE_INTENSITY = 0.3f;

// quick_rand.cpp: new_seed = ror(seed,13) - (seed - 11), unsigned 32-bit.
class QuickRand
{
public:
  QuickRand()           { Init(0x76543210); }
  explicit QuickRand(u32 seed) { Init(seed); }
  void Init(u32 seed)   { m_seed = seed; }
  u32  Rand()
  {
    u32 s = m_seed;
    u32 rotated = (s >> 13) | (s << 19);
    u32 next = rotated - (s - 11u);
    m_seed = next;
    return next;
  }
  f32  Rand01() { return (f32)(Rand() & 0xffff) / 65536.0f; }
  f32  Rand11() { return ((f32)(Rand() & 0xffff) * 2.0f) / 65536.0f - 1.0f; }
private:
  u32 m_seed;
};

class CBlobSim;

class Bloblet
{
public:
  Bloblet();
  // returns true when the bloblet has retracted back inside (detach candidate)
  bool Update(CBlobSim* sim, f32 elapsed, f32 dt);

  f32 fRadius;
  f32 fStartTime;
  f32 fTimeMultiple;
  f32 fMaxDist;
  f32 fWobble;
  f32 fWobbleDirection;
  f32 fCurDist;
  bool bFarSide;
  f32 vPosition[3];
  f32 vDirection[3];
};

class BlobBump
{
public:
  BlobBump();
  void RecalcFaces();
  // returns true if this call grabbed the 'spare' bloblet (caller bumps count)
  bool Create(CBlobSim* sim, f32 curTime, Bloblet* spare);
  bool Update(CBlobSim* sim, f32 elapsed, f32 dt, Bloblet* spare);

  f32 fRadius, fRadius2, fOORadius2;
  f32 fMagnitude;
  f32 vPosition[3];
  int facesOfInterest;
  f32 vDirection[3];
  f32 fStartTime;
  f32 fTimeMul;
  f32 fMaxMagnitude;
  bool bStillAttached;
  Bloblet* bloblet;
};

class CBlobSim
{
public:
  CBlobSim();

  void Restart();
  // Deterministic fixed-step catch-up: lands on the same shape regardless of fps.
  void Seek(f32 target);

  // inverse-distance-squared weighted light position (blob center + bloblets).
  void LightFor(const f32 pos[3], f32 outLight[3]) const;

  // --- geometry the renderer consumes --------------------------------------
  int          VertexCount()    const { return m_count; }
  const f32*   UnitPos()        const { return &m_pos[0]; }      // count*3
  const f32*   Changing()       const { return &m_changing[0]; } // count*4 (nx,ny,nz,disp)
  const u16*   StripIndices()   const { return &m_idx[0]; }
  int          StripIndexCount()const { return (int)m_idx.size(); }

  // Bloblet (drop) geometry: a smaller unit sphere, shared by all live drops.
  int          BlobletVertCount()  const { return (int)(m_bPos.size() / 3); }
  const f32*   BlobletPos()        const { return &m_bPos[0]; }   // vc*3 unit sphere
  const u16*   BlobletIndices()    const { return &m_bIdx[0]; }
  int          BlobletIndexCount() const { return (int)m_bIdx.size(); }

  // --- state for later passes (bloblets / lighting) ------------------------
  int     numBloblets;
  int     numBumps;
  f32     center[3];
  f32     scale[3];
  Bloblet bloblets[MAX_BLOBLETS];
  BlobBump bumps[MAX_BLOBBUMPS];
  QuickRand rand;

private:
  void BuildUnitSphere(int resolution);   // fills m_pos / m_idx / m_count
  void ZeroChanging();
  void PrepareChanging();
  void Advance(f32 elapsed, f32 dt);

  std::vector<f32> m_pos;        // unit sphere positions, count*3
  std::vector<u16> m_idx;        // one triangle strip (with degenerate stitches)
  std::vector<f32> m_changing;   // count*4
  std::vector<f32> m_bPos;       // bloblet unit sphere positions
  std::vector<u16> m_bIdx;       // bloblet triangle strip
  int   m_count;
  int   m_perFace;
  int   m_subdiv;
  f32   m_time;
};
