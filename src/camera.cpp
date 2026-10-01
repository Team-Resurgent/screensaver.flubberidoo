/*
 *  camera.cpp — see camera.h. Ported from FlubberForge camera.js.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "camera.h"
#include "flubconst.h"

#include <math.h>

using namespace flubtime;

struct CamRow { f32 t, px, py, pz, lx, ly, lz; };

static const CamRow kPath0[] = {
  { 0.0f, 11.4f, -32.1f, 33.0f, 0.0f, 0.0f, 0.0f },
  { 20.0f, 13.4f, -37.7f, 25.6f, 0.0f, 0.0f, 0.0f },
  { 40.0f, 15.6f, -43.9f, 8.8f, 0.0f, 0.0f, 0.0f },
  { 60.0f, 16.0f, -45.0f, -12.8f, 0.0f, 0.0f, 0.0f },
  { 90.0f, 18.2f, -51.2f, -29.6f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath1[] = {
  { 0.0f, -55.4f, 19.7f, -31.5f, 0.0f, 0.0f, 0.0f },
  { 30.0f, -55.4f, 19.7f, -31.5f, 0.0f, 0.0f, 0.0f },
  { 45.0f, -39.5f, -0.6f, -7.8f, 0.0f, 0.0f, 0.0f },
  { 60.0f, -4.3f, -35.5f, 16.6f, 0.0f, 0.0f, 0.0f },
  { 70.0f, 31.1f, -32.6f, 17.6f, 0.0f, 0.0f, 0.0f },
  { 80.0f, 57.7f, -7.2f, 3.3f, 0.0f, 0.0f, 0.0f },
  { 95.0f, 70.9f, 1.8f, 3.1f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath2[] = {
  { 0.0f, 34.7f, 25.9f, 12.3f, 0.0f, 0.0f, 0.0f },
  { 25.0f, 42.3f, 9.3f, 12.3f, 0.0f, 0.0f, 0.0f },
  { 50.0f, 42.4f, -8.8f, 12.3f, 0.0f, 0.0f, 0.0f },
  { 75.0f, 34.4f, -26.3f, 12.3f, 0.0f, 0.0f, 0.0f },
  { 95.0f, 30.7f, -48.1f, 14.3f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath3[] = {
  { 0.0f, -50.1f, -0.3f, -51.5f, 0.0f, 0.0f, 0.0f },
  { 25.0f, -50.1f, -0.3f, -51.5f, 0.0f, 0.0f, 0.0f },
  { 75.0f, -50.1f, -0.3f, -51.5f, 0.0f, 0.0f, 0.0f },
  { 95.0f, -62.2f, -0.4f, -12.0f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath4[] = {
  { 0.0f, 3.2f, -17.4f, 13.0f, -1.9f, 7.8f, -15.3f },
  { 20.0f, 2.1f, -12.1f, 6.15f, -1.9f, 7.8f, -15.3f },
  { 40.0f, 1.0f, -6.8f, -0.7f, -1.9f, 7.8f, -15.3f },
  { 55.0f, -7.2f, -3.6f, -7.5f, -1.9f, 7.8f, -15.3f },
  { 70.0f, -18.5f, 14.1f, -18.4f, -1.9f, 7.8f, -15.3f },
  { 85.0f, -16.2f, 29.2f, -23.1f, -5.1f, 8.4f, -10.3f },
  { 95.0f, -10.6f, 49.3f, -21.2f, -5.1f, 8.4f, -10.3f },
};
static const CamRow kPath5[] = {
  { 0.0f, -1.5f, 2.8f, -14.0f, 0.0f, -1.3f, 0.0f },
  { 20.0f, -3.8f, 16.9f, -22.8f, 0.0f, -1.3f, 0.0f },
  { 45.0f, 20.7f, 27.6f, -21.6f, 0.0f, -1.3f, 0.0f },
  { 75.0f, 18.1f, 46.8f, -27.0f, 0.0f, -1.3f, 0.0f },
  { 95.0f, 28.9f, 75.1f, -36.7f, 0.0f, -1.3f, 0.0f },
};
static const CamRow kPath6[] = {
  { 0.0f, 15.2f, -3.3f, -15.9f, 0.0f, 0.0f, 0.0f },
  { 20.0f, 26.2f, -5.7f, -20.5f, 0.0f, 0.0f, 0.0f },
  { 45.0f, 40.2f, -8.7f, -23.6f, 0.0f, 0.0f, 0.0f },
  { 65.0f, 61.2f, -13.3f, -3.6f, 0.0f, 0.0f, 0.0f },
  { 85.0f, 84.4f, -4.8f, 11.7f, 0.0f, 0.0f, 0.0f },
  { 95.0f, 120.4f, -8.8f, 14.7f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath7[] = {
  { 0.0f, -20.5f, 48.8f, 12.0f, 0.0f, 0.0f, -18.6f },
  { 35.0f, -10.4f, 24.6f, -2.4f, 0.0f, 0.0f, -18.6f },
  { 70.0f, -10.4f, 24.6f, -2.4f, 0.0f, 0.0f, -18.6f },
  { 85.0f, -16.5f, 39.3f, 4.2f, -0.3f, 0.1f, -5.3f },
  { 95.0f, -26.3f, 75.4f, -4.6f, -0.3f, 0.1f, -0.5f },
};
static const CamRow kPath8[] = {
  { 0.0f, -92.5f, -10.1f, 20.9f, 0.0f, 0.0f, 0.0f },
  { 15.0f, -88.0f, 30.4f, 20.9f, 0.0f, 0.0f, 0.0f },
  { 30.0f, -64.7f, 67.0f, 20.9f, 0.0f, 0.0f, 0.0f },
  { 45.0f, -22.6f, 90.3f, 20.9f, 0.0f, 0.0f, 0.0f },
  { 60.0f, 21.7f, 90.5f, 20.9f, 0.0f, 0.0f, 0.0f },
  { 72.0f, 65.4f, 66.2f, 20.9f, 0.0f, 0.0f, 0.0f },
  { 85.0f, 97.5f, 23.7f, 16.1f, 0.0f, 0.0f, 0.0f },
  { 95.0f, 110.8f, -6.1f, 0.0f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath9[] = {
  { 0.0f, 62.3f, -28.5f, -20.2f, 0.0f, 0.0f, 0.0f },
  { 30.0f, 62.3f, -28.5f, -20.2f, 0.0f, 0.0f, 0.0f },
  { 50.0f, 55.2f, -25.3f, -10.1f, 0.0f, 0.0f, 0.0f },
  { 75.0f, 56.7f, -12.9f, 0.0f, 0.0f, 0.0f, 0.0f },
  { 95.0f, 73.9f, -13.0f, 0.0f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath10[] = {
  { 0.0f, 50.4f, 33.2f, 25.3f, 0.0f, 0.0f, 0.0f },
  { 30.0f, 50.4f, 33.2f, 25.3f, 0.0f, 0.0f, 0.0f },
  { 45.0f, 55.7f, 9.3f, 15.9f, 0.0f, 0.0f, 0.0f },
  { 65.0f, 39.1f, -35.8f, 3.3f, 0.0f, 0.0f, 0.0f },
  { 90.0f, 7.1f, -53.7f, 0.0f, 0.0f, 0.0f, 0.0f },
  { 95.0f, -16.8f, -78.0f, 0.0f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath11[] = {
  { 0.0f, -51.7f, -2.4f, -32.4f, 0.0f, 0.0f, 12.0f },
  { 25.0f, -51.7f, -2.4f, -32.4f, 0.0f, 0.0f, 12.0f },
  { 40.0f, -54.4f, -2.6f, -23.3f, 0.0f, 0.0f, 10.0f },
  { 50.0f, -53.2f, 6.7f, -14.3f, 0.0f, 0.0f, 7.0f },
  { 60.0f, -41.8f, 21.2f, -9.3f, 0.0f, 0.0f, 4.5f },
  { 70.0f, -27.1f, 34.2f, -9.3f, 0.0f, 0.0f, 2.0f },
  { 80.0f, -6.9f, 47.5f, -9.3f, 0.0f, 0.0f, 1.0f },
  { 88.0f, 11.3f, 52.3f, -9.2f, 0.0f, 0.0f, 0.5f },
  { 95.0f, 24.2f, 62.8f, -9.0f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath12[] = {
  { 0.0f, -2.6f, -85.0f, 17.7f, 0.0f, 0.0f, 0.0f },
  { 20.0f, 48.1f, -70.1f, 17.7f, 0.0f, 0.0f, 0.0f },
  { 40.0f, 80.1f, -28.6f, 17.7f, 0.0f, 0.0f, 0.0f },
  { 60.0f, 81.4f, 24.8f, 17.7f, 0.0f, 0.0f, 0.0f },
  { 80.0f, 56.5f, 63.6f, 17.7f, 0.0f, 0.0f, 0.0f },
  { 95.0f, 27.9f, 86.5f, 8.2f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath13[] = {
  { 0.0f, 47.3f, -76.6f, -10.0f, 0.0f, 0.0f, 0.0f },
  { 25.0f, 11.5f, -89.3f, -10.0f, 0.0f, 0.0f, 0.0f },
  { 50.0f, -35.7f, -82.6f, -10.0f, 0.0f, 0.0f, 0.0f },
  { 75.0f, -71.9f, -54.2f, -10.0f, 0.0f, 0.0f, 0.0f },
  { 95.0f, -89.5f, -9.6f, -10.0f, 0.0f, 0.0f, 0.0f },
};
static const CamRow kPath14[] = {
  { 0.0f, -11.2f, -42.9f, 17.0f, 0.0f, 0.0f, 0.0f },
  { 20.0f, 2.6f, -46.9f, 7.0f, 0.0f, 0.0f, 0.0f },
  { 40.0f, 24.3f, -40.5f, -4.2f, 0.0f, 0.0f, 0.0f },
  { 60.0f, 38.7f, -24.8f, -9.0f, 0.0f, 0.0f, 0.0f },
  { 80.0f, 44.3f, -12.1f, -7.0f, 0.0f, 0.0f, 0.0f },
  { 95.0f, 52.4f, 6.3f, 2.0f, 0.0f, 0.0f, 0.0f },
};
static const CamRow* const kPaths[15] = {
  kPath0, kPath1, kPath2, kPath3, kPath4, kPath5, kPath6, kPath7,
  kPath8, kPath9, kPath10, kPath11, kPath12, kPath13, kPath14
};
static const int kPathLen[15] = { 5, 7, 5, 4, 7, 5, 6, 5, 8, 5, 6, 9, 6, 5, 6 };
static const f32 kFinishY[8] = { 95.0f, 30.548f, -70.819f, -150.298f, -220.64f, -243.021f, -261.441f, -287.773f };
static const f32 kFinishZ[8] = { 0.0f, 0.322f, 1.821f, 2.323f, -11.926f, -39.973f, -60.774f, -90.795f };

void CCamera::Build(int cameraMode)
{
  int mode = cameraMode;
  if (mode <= 0) mode = 1;
  int pathIndex = ((mode - 1) % 15 + 15) % 15;
  const CamRow* path = kPaths[pathIndex];
  int varLen = kPathLen[pathIndex];

  // variable nodes
  for (int i = 0; i < varLen; i++)
  {
    Node& nd = m_nodes[i];
    nd.fTime = FINISH_START_TIME * path[i].t * 0.01f;
    nd.pos   = Vec3(path[i].px, path[i].py, path[i].pz);
    nd.look  = Vec3(path[i].lx, path[i].ly, path[i].lz);
    nd.vel   = Vec3(0, 0, 0);
    nd.lookW = Vec3(0, 0, 0);
  }
  m_variableCount = varLen;

  // 8 finish nodes (positions filled below)
  Node* finish = &m_nodes[varLen];
  for (int j = 0; j < 8; j++)
  {
    finish[j].fTime = FINISH_START_TIME + FINISH_TRANSITION_TIME * ((f32)j / 7.0f);
    finish[j].pos   = Vec3(0, 0, 0);
    finish[j].look  = Vec3(0, 0, 0);
    finish[j].vel   = Vec3(0, 0, 0);
    finish[j].lookW = Vec3(0, 0, 0);
  }
  m_count = varLen + 8;

  // --- finish[0] dive start, from the last variable node + its outgoing vel ---
  Node& plast = m_nodes[varLen - 1];
  Node& p0 = finish[0];
  p0.pos = plast.pos;
  Vec3 vel(0, 0, 0);
  if (varLen >= 2)
  {
    Node& prev = m_nodes[varLen - 2];
    f32 dt = plast.fTime - prev.fTime; if (dt == 0.0f) dt = 0.001f;
    vel = (plast.pos - prev.pos) * (1.0f / dt);
  }
  p0.pos = p0.pos + vel * ((p0.fTime - plast.fTime) * 0.7f);

  f32 velAdjLen = Length(p0.pos); if (velAdjLen == 0.0f) velAdjLen = 1.0f;
  Vec3 slashDir = p0.pos * (1.0f / velAdjLen);
  const f32 slashStart = -95.0f;
  const f32 slashEnd = 132.14f;
  f32 a = 100.0f - slashStart;
  f32 b = velAdjLen * 1.2f - slashStart;
  f32 slashYOffset = a > b ? a : b;
  p0.pos = slashDir * (slashYOffset + slashStart);

  Vec3 yDir = slashDir * -1.0f;
  Vec3 xDir = Cross(yDir, Vec3(0, 0, 1));
  if (Length(xDir) < 1e-4f) xDir = Vec3(1, 0, 0);
  xDir = Normalize(xDir);
  Vec3 zDir = Cross(xDir, yDir);
  Vec3 slashCenter = yDir * (-slashEnd - slashYOffset);

  f32 yBasis = slashEnd;
  for (int j = 1; j < 8; j++)
  {
    Vec3 local(0.0f, kFinishY[j] + yBasis, kFinishZ[j]);
    Vec3 world = xDir * local.x + yDir * local.y + zDir * local.z;
    finish[j].pos = world + slashCenter;
  }
  {
    Vec3 local(0.0f, slashEnd, 25.0f);
    Vec3 world = xDir * local.x + yDir * local.y + zDir * local.z;
    m_finalLook = world + slashCenter;
  }

  // tangents (TCB with tension=bias=0 => Catmull-Rom weights of 0.5)
  for (int j = 0; j < m_count; j++)
  {
    Node& cur = m_nodes[j];
    cur.vel = Vec3(0, 0, 0);
    cur.lookW = Vec3(0, 0, 0);
    if (j > 0)
    {
      Vec3 dpos = cur.pos - m_nodes[j - 1].pos;
      cur.vel = cur.vel + dpos * 0.5f;
      Vec3 dl = cur.look - m_nodes[j - 1].look;
      cur.lookW = cur.lookW + dl * 0.5f;
    }
    if (j < m_count - 1)
    {
      Vec3 dpos = m_nodes[j + 1].pos - cur.pos;
      cur.vel = cur.vel + dpos * 0.5f;
      Vec3 dl = m_nodes[j + 1].look - cur.look;
      cur.lookW = cur.lookW + dl * 0.5f;
    }
  }

  m_lookStart = finish[2].fTime;
  m_lookDelta = 1.0f / (finish[5].fTime - m_lookStart);
}

CamShot CCamera::Sample(f32 t) const
{
  CamShot out;
  int n = m_count;

  if (t > FINISH_STOP_TIME)
  {
    out.pos = m_nodes[n - 1].pos; out.look = m_finalLook;
    out.renderSlash = true; out.renderGeom = false;
    return out;
  }

  int i = 1;
  for (; i < n; i++) if (m_nodes[i].fTime > t) break;
  if (i >= n)
  {
    out.pos = m_nodes[n - 1].pos; out.look = m_finalLook;
    out.renderSlash = true; out.renderGeom = false;
    return out;
  }

  const Node& prev = m_nodes[i - 1];
  const Node& next = m_nodes[i];
  f32 dtc = next.fTime - prev.fTime; if (dtc < 0.001f) dtc = 0.001f;
  f32 dtp = (i >= 2) ? (prev.fTime - m_nodes[i - 2].fTime) : dtc; if (dtp < 0.001f) dtp = 0.001f;
  f32 uts = (t - prev.fTime) / dtc; if (uts < 0.0f) uts = 0.0f; else if (uts > 1.0f) uts = 1.0f;
  f32 frac = -2.0f * uts * uts * uts + 3.0f * uts * uts;
  f32 s = (t - prev.fTime) / ((1.0f - frac) * dtp + frac * dtc);
  f32 ss = s * s;
  f32 sss = ss * s;
  f32 cA = 2.0f * sss - 3.0f * ss + 1.0f;
  f32 cB = sss - 2.0f * ss + s;
  f32 cC = sss - ss;
  f32 cD = -2.0f * sss + 3.0f * ss;

  out.pos = prev.pos * cA + prev.vel * cB + next.vel * cC + next.pos * cD;
  Vec3 look = prev.look * cA + prev.lookW * cB + next.lookW * cC + next.look * cD;

  f32 sl = (t - m_lookStart) * m_lookDelta;
  if (sl < 0.0f) sl = 0.0f; else if (sl > 1.0f) sl = 1.0f;
  f32 interp = 0.5f * (1.0f - (f32)cos(sl * PI));
  out.look = look * (1.0f - interp) + m_finalLook * interp;

  out.renderSlash = i > m_variableCount - 1;
  out.renderGeom  = i < n - 1;
  return out;
}
