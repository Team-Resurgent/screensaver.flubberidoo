/*
 *  blob.cpp — see blob.h. Ported line-for-line from FlubberForge blob.js
 *  (itself a port of blob.cpp/blob_bump.cpp/bloblet.cpp) to keep the shape,
 *  timing and random sequence identical.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "blob.h"
#include "flubconst.h"

#include <math.h>

using namespace flubtime;

static const f32 kPI  = 3.14159265358979323846f;
static const f32 kTAU = 6.28318530717958647692f;

static inline f32 Hypot3(f32 x, f32 y, f32 z) { return (f32)sqrt(x * x + y * y + z * z); }

/* ----------------------------------------------------------------- Bloblet - */
Bloblet::Bloblet()
  : fRadius(0), fStartTime(0), fTimeMultiple(0), fMaxDist(0), fWobble(1),
    fWobbleDirection(0), fCurDist(0), bFarSide(false)
{
  vPosition[0] = vPosition[1] = vPosition[2] = 0.0f;
  vDirection[0] = vDirection[1] = 0.0f; vDirection[2] = 1.0f;
}

bool Bloblet::Update(CBlobSim* sim, f32 elapsed, f32 dt)
{
  fWobble = (f32)FMin(2.0, FMax(0.5, fWobble + fWobbleDirection * dt));
  if (fWobbleDirection > 0.0f)
  {
    if (fWobble < 0.95f || fWobble > 1.0f) fWobbleDirection -= (fWobble - 1.0f) * dt * 1000.0f;
  }
  else if (fWobble < 1.0f || fWobble > 1.05f)
  {
    fWobbleDirection -= (fWobble - 1.0f) * dt * 1000.0f;
  }

  f32 timeProg = FMax(0.0f, (elapsed - BLOB_STATIC_END_TIME) * OO_MAX_INTENSITY_DELTA);
  f32 t = fTimeMultiple * (elapsed - fStartTime);
  t *= 1.4f * (1.0f + elapsed / 10.0f);
  f32 s = (f32)sin(t);
  f32 sm = (f32)fabs(s);
  sm = 1.0f - (1.0f - sm) * (f32)sqrt(1.0f - sm);
  s = s > 0.0f ? sm : -sm;

  fCurDist = fMaxDist * s * timeProg;
  bFarSide = fCurDist < 0.0f;
  vPosition[0] = sim->center[0] + vDirection[0] * fCurDist;
  vPosition[1] = sim->center[1] + vDirection[1] * fCurDist;
  vPosition[2] = sim->center[2] + vDirection[2] * fCurDist;
  return (f32)fabs(fCurDist) + fRadius < BLOB_RADIUS * 0.5f;
}

/* ---------------------------------------------------------------- BlobBump - */
BlobBump::BlobBump()
  : fRadius(0), fRadius2(0), fOORadius2(0), fMagnitude(0), facesOfInterest(0),
    fStartTime(0), fTimeMul(0), fMaxMagnitude(0), bStillAttached(false), bloblet(0)
{
  vPosition[0] = vPosition[1] = vPosition[2] = 0.0f;
  vDirection[0] = vDirection[1] = 0.0f; vDirection[2] = 1.0f;
}

void BlobBump::RecalcFaces()
{
  const f32* d = vDirection;
  f32 r = fRadius;
  facesOfInterest =
    (d[0] - r < -0.57735f ? 0x01 : 0) +
    (d[1] - r < -0.57735f ? 0x02 : 0) +
    (d[2] - r < -0.57735f ? 0x04 : 0) +
    (d[0] + r >  0.57735f ? 0x08 : 0) +
    (d[1] + r >  0.57735f ? 0x10 : 0) +
    (d[2] + r >  0.57735f ? 0x20 : 0);
}

bool BlobBump::Create(CBlobSim* sim, f32 curTime, Bloblet* spare)
{
  QuickRand& rnd = sim->rand;
  if (curTime < 0.0f) bloblet = 0;

  f32 d[3];
  d[0] = rnd.Rand11(); d[1] = rnd.Rand11(); d[2] = rnd.Rand11();
  if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < 0.001f)
  { d[0] = rnd.Rand11(); d[1] = rnd.Rand11(); d[2] = 1.0f; }
  f32 len = Hypot3(d[0], d[1], d[2]); if (len == 0.0f) len = 1.0f;
  vDirection[0] = d[0] / len; vDirection[1] = d[1] / len; vDirection[2] = d[2] / len;
  vPosition[0] = vPosition[1] = vPosition[2] = 0.0f;

  f32 timeProg = FMax(0.0f, (curTime - BLOB_STATIC_END_TIME) * OO_MAX_INTENSITY_DELTA);
  f32 radMagRand = rnd.Rand01();

  fRadius = radMagRand * 0.4f + 0.4f;
  fRadius2 = fRadius * fRadius;
  fOORadius2 = 1.0f / fRadius2;
  fMagnitude = 0.0f;
  RecalcFaces();

  fStartTime = curTime + 0.4f * rnd.Rand01();
  fMaxMagnitude = (1.0f - radMagRand) * 0.5f + 0.2f;
  fMaxMagnitude *= 0.5f + 0.5f * timeProg;

  if (!bloblet) bloblet = spare;   // spare may be 0

  if (bloblet)
  {
    Bloblet* b = bloblet;
    b->fRadius = (rnd.Rand01() + 1.0f) * 0.25f * BLOB_RADIUS * fRadius;
    b->vDirection[0] = vDirection[0]; b->vDirection[1] = vDirection[1]; b->vDirection[2] = vDirection[2];
    b->fMaxDist = BLOB_RADIUS * (5.0f + rnd.Rand11() * 2.0f);
    b->fMaxDist *= 0.6f;
    b->fStartTime = curTime < -1.0f ? -rnd.Rand01() * 0.3f : curTime;
    f32 period = 0.8f + 0.3f * rnd.Rand01();
    period *= 1.0f / 0.6f;
    b->fTimeMultiple = kTAU / period;
    b->fWobble = 1.2f;
    b->fWobbleDirection = 0.0f;
    bStillAttached = (curTime - fStartTime) < 0.4f * period;
    b->Update(sim, curTime, 0.0f);
    Update(sim, curTime, 0.0f, 0);
    return true;
  }

  f32 sequenceLen = fMaxMagnitude * 0.3f + rnd.Rand01() * 0.3f;
  fTimeMul = kPI / sequenceLen;
  fTimeMul *= timeProg * 0.2f + 0.8f;
  if (curTime < -1.0f) fStartTime = (-rnd.Rand01() * kPI) / fTimeMul;
  return false;
}

bool BlobBump::Update(CBlobSim* sim, f32 elapsed, f32 dt, Bloblet* spare)
{
  if (bloblet)
  {
    Bloblet* b = bloblet;
    f32 mag = ((f32)fabs(b->fCurDist) + b->fRadius) / BLOB_RADIUS;
    fMagnitude = FMin(2.0f, FMax(0.0f, mag - 1.0f));
    if (bStillAttached)
    {
      if (fMagnitude > 0.8f)
      {
        bStillAttached = false;
        fMaxMagnitude = fMagnitude;
        f32 sequenceLen = 0.3f * fMagnitude;
        fTimeMul = kTAU / sequenceLen;
        fStartTime = elapsed - 0.25f * sequenceLen;
        b->fWobble = FMax(0.6f, FMin(0.8f, fMagnitude - 0.5f));
        b->fWobbleDirection = 0.0f;
      }
      else
      {
        f32 dot = vDirection[0] * b->vDirection[0] +
                  vDirection[1] * b->vDirection[1] +
                  vDirection[2] * b->vDirection[2];
        if ((dot < 0.0f) != b->bFarSide)
        {
          vDirection[0] = -vDirection[0]; vDirection[1] = -vDirection[1]; vDirection[2] = -vDirection[2];
          vPosition[0] = vDirection[0]; vPosition[1] = vDirection[1]; vPosition[2] = vDirection[2];
          RecalcFaces();
        }
        return false;
      }
    }
    if (!bStillAttached && mag < 0.9f) bStillAttached = true;
  }

  f32 t = (elapsed - fStartTime) * fTimeMul;
  if (t > kPI)
  {
    if (!bloblet) return Create(sim, elapsed, spare);
    fMagnitude = 0.0f;
    return false;
  }
  if (t < 0.0f) return false;

  fMagnitude = fMaxMagnitude * (f32)sin(t);
  vPosition[0] = vDirection[0]; vPosition[1] = vDirection[1]; vPosition[2] = vDirection[2];
  RecalcFaces();
  return false;
}

/* ------------------------------------------------------- unit-sphere build - */
// tristripMesh from blob.js: one strip over an (xQuads x yQuads) grid, splitting
// runs longer than 14 quads and stitching with doubled/degenerate taps.
static void TriStrip(std::vector<int>& out, int xQuads, int yQuads,
                     bool doubleFirst, bool doubleLast, int start, int vstride, int hstride)
{
  if (vstride == 0) vstride = xQuads + 1;
  if (hstride == 0) hstride = 1;
  if (xQuads > 14)
  {
    TriStrip(out, 14, yQuads, doubleFirst, true, start, vstride, hstride);
    TriStrip(out, xQuads - 14, yQuads, true, doubleLast, start + 14 * hstride, vstride, hstride);
    return;
  }
  if (doubleFirst) out.push_back(start);
  out.push_back(start);
  for (int i = 1; i <= xQuads; i++)
  {
    out.push_back(start + i * hstride);
    out.push_back(start + i * hstride);
  }
  for (int j = 0; j < yQuads; j++)
  {
    out.push_back(start + j * vstride);
    for (int i = 0; i <= xQuads; i++)
    {
      out.push_back(start + j * vstride + i * hstride);
      out.push_back(start + (j + 1) * vstride + i * hstride);
    }
    if (j < yQuads - 1) out.push_back(start + (j + 1) * vstride + xQuads * hstride);
  }
  if (doubleLast) out.push_back(start + yQuads * vstride + xQuads * hstride);
}

// blob::generateUnitSphere — fills pos (count*3) + one triangle strip (idx).
static void BuildSphereGeom(int resolution, std::vector<f32>& pos, std::vector<u16>& idx)
{
  int subdiv = resolution / 2; if (subdiv < 1) subdiv = 1;
  f32 step = 2.0f / subdiv;

  pos.clear();
  for (int k = 0; k < 6; k++)
    for (int j = 0; j <= subdiv; j++)
      for (int i = 0; i <= subdiv; i++)
      {
        f32 fu = (i == subdiv) ? 1.0f : (-1.0f + step * i);
        f32 fv = (j == subdiv) ? 1.0f : (-1.0f + step * j);
        f32 p0, p1, p2;
        if      (k == 0) { p0 = -1.0f; p1 = -fu;   p2 = fv;   }
        else if (k == 1) { p0 = fv;    p1 = -1.0f; p2 = -fu;  }
        else if (k == 2) { p0 = -fu;   p1 = fv;    p2 = -1.0f;}
        else if (k == 3) { p0 = 1.0f;  p1 = fu;    p2 = fv;   }
        else if (k == 4) { p0 = fv;    p1 = 1.0f;  p2 = fu;   }
        else             { p0 = fu;    p1 = fv;    p2 = 1.0f; }
        f32 l = Hypot3(p0, p1, p2); if (l == 0.0f) l = 1.0f;
        pos.push_back(p0 / l); pos.push_back(p1 / l); pos.push_back(p2 / l);
      }

  int perFace = (subdiv + 1) * (subdiv + 1);
  std::vector<int> ib;
  for (int k = 0; k < 6; k++)
    TriStrip(ib, subdiv, subdiv, k > 0, k < 5, k * perFace, 0, 0);
  idx.clear();
  idx.reserve(ib.size());
  for (size_t n = 0; n < ib.size(); n++) idx.push_back((u16)ib[n]);
}

void CBlobSim::BuildUnitSphere(int resolution)
{
  m_subdiv = resolution / 2; if (m_subdiv < 1) m_subdiv = 1;
  m_perFace = (m_subdiv + 1) * (m_subdiv + 1);
  m_count = m_perFace * 6;
  BuildSphereGeom(resolution, m_pos, m_idx);
}

/* ------------------------------------------------------------- CBlobSim ---- */
CBlobSim::CBlobSim()
  : numBloblets(0), numBumps(0), m_count(0), m_perFace(0), m_subdiv(0), m_time(0)
{
  center[0] = center[1] = center[2] = 0.0f;
  scale[0] = scale[1] = scale[2] = 1.0f;
  BuildUnitSphere(BLOB_DIM);
  m_changing.resize(m_count * 4);
  BuildSphereGeom(BLOBLET_DIM, m_bPos, m_bIdx);   // drop geometry (unitSphere(8))
  Restart();
}

void CBlobSim::Restart()
{
  rand.Init(0x76543210);
  numBloblets = 0;
  numBumps = 0;
  while (numBumps < MAX_BLOBBUMPS)
  {
    Bloblet* spare = (numBloblets < MAX_BLOBLETS) ? &bloblets[numBloblets] : 0;
    if (bumps[numBumps++].Create(this, -0.3f, spare)) numBloblets++;
  }
  ZeroChanging();
  m_time = 0.0f;
}

void CBlobSim::ZeroChanging()
{
  const f32* us = &m_pos[0];
  f32* out = &m_changing[0];
  int n = m_count * 4;
  for (int i = 0, v = 0; i < n; i += 4, v += 3)
  {
    out[i]     = us[v];
    out[i + 1] = us[v + 1];
    out[i + 2] = us[v + 2];
    out[i + 3] = 1.0f;
  }
}

void CBlobSim::PrepareChanging()
{
  const f32* us = &m_pos[0];
  f32* out = &m_changing[0];
  int v = 0;
  for (int face = 0; face < 6; face++)
  {
    BlobBump* boi[MAX_BLOBBUMPS];
    int nboi = 0;
    for (int i = 0; i < numBumps; i++)
      if (bumps[i].facesOfInterest & (1 << face)) boi[nboi++] = &bumps[i];

    for (int i = 0; i < m_perFace; i++, v++)
    {
      f32 nx = us[v * 3], ny = us[v * 3 + 1], nz = us[v * 3 + 2];
      f32 ax = nx, ay = ny, az = nz, aw = 0.0f;
      for (int j = nboi - 1; j >= 0; j--)
      {
        BlobBump* bump = boi[j];
        f32 dx = nx - bump->vPosition[0];
        f32 dy = ny - bump->vPosition[1];
        f32 dz = nz - bump->vPosition[2];
        f32 dist2 = dx * dx + dy * dy + dz * dz;
        if (dist2 >= bump->fRadius2) continue;
        f32 dist2mo = dist2 * bump->fOORadius2 - 1.0f;
        f32 displacement = BLOB_RADIUS * bump->fMagnitude * dist2mo * dist2mo;
        f32 perturb = -4.0f * bump->fMagnitude * bump->fOORadius2 * dist2mo;
        f32 lx = nx + dx * perturb;
        f32 ly = ny + dy * perturb;
        f32 lz = nz + dz * perturb;
        f32 l = Hypot3(lx, ly, lz); if (l == 0.0f) l = 1.0f;
        ax += lx / l; ay += ly / l; az += lz / l;
        aw += displacement;
      }
      int o = v * 4;
      out[o] = ax; out[o + 1] = ay; out[o + 2] = az; out[o + 3] = aw;
    }
  }
}

void CBlobSim::Advance(f32 elapsed, f32 dt)
{
  if (elapsed < BLOB_STATIC_END_TIME) return;
  for (int i = 0; i < numBloblets; i++) bloblets[i].Update(this, elapsed, dt);
  for (int i = 0; i < numBumps; i++)
  {
    Bloblet* spare = (numBloblets < MAX_BLOBLETS) ? &bloblets[numBloblets] : 0;
    if (bumps[i].Update(this, elapsed, dt, spare)) numBloblets++;
  }
  PrepareChanging();
}

void CBlobSim::Seek(f32 target)
{
  if (target < m_time) Restart();
  int guard = 0;
  while (m_time + BLOB_SIM_DT <= target && guard++ < 4000)
  {
    m_time += BLOB_SIM_DT;
    Advance(m_time, BLOB_SIM_DT);
  }
}

void CBlobSim::LightFor(const f32 pos[3], f32 outLight[3]) const
{
  f32 totalWeight = 0.0f;
  f32 av[3] = { 0.0f, 0.0f, 0.0f };

  // center, then each live bloblet
  const f32* pts[1 + MAX_BLOBLETS];
  int np = 0;
  pts[np++] = center;
  for (int i = 0; i < numBloblets; i++) pts[np++] = bloblets[i].vPosition;

  for (int k = 0; k < np; k++)
  {
    const f32* p = pts[k];
    f32 dx = pos[0] - p[0], dy = pos[1] - p[1], dz = pos[2] - p[2];
    f32 dist2 = dx * dx + dy * dy + dz * dz;
    if (dist2 < 1e-6f) dist2 = 1e-6f;
    f32 w = 1.0f / dist2;
    av[0] += p[0] * w; av[1] += p[1] * w; av[2] += p[2] * w;
    totalWeight += w;
  }
  f32 oo = 1.0f / totalWeight;
  outLight[0] = av[0] * oo; outLight[1] = av[1] * oo; outLight[2] = av[2] * oo;
}
