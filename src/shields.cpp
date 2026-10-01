/*
 *  shields.cpp — see shields.h. Ported from FlubberForge shields.js / render.js.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "main.h"      // d3dSetRenderState / d3dSetTextureStageState
#include "shields.h"
#include "blob.h"      // QuickRand
#include "flubconst.h"

#include <math.h>
#include <string.h>

using namespace flubtime;

static const f32 kPI = 3.14159265358979323846f;
static const f32 MOOD_LIGHT[3] = { 0.0f, -40.0f, 30.0f };

struct ShieldVtx { f32 x, y, z; DWORD color; };
#define SHIELD_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)

static inline f32 Hyp3(f32 x, f32 y, f32 z) { return (f32)sqrt(x*x + y*y + z*z); }

/* ----------------------------------------------------- matrix helpers ----- */
static Mat4 YRotation(f32 r)
{
  f32 s = (f32)sin(r), c = (f32)cos(r);
  Mat4 m = Mat4::Identity();
  m.m[0]=c; m.m[2]=s; m.m[8]=-s; m.m[10]=c;
  return m;
}
static Mat4 ZRotation(f32 r)
{
  f32 s = (f32)sin(r), c = (f32)cos(r);
  Mat4 m = Mat4::Identity();
  m.m[0]=c; m.m[1]=s; m.m[4]=-s; m.m[5]=c;
  return m;
}
// math::SetRotationFromRHQuat — note the signs DIFFER from render.js matFromQuat.
static Mat4 RHQuatMatrix(const f32 q[4])
{
  f32 x=q[0], y=q[1], z=q[2], w=q[3];
  Mat4 m;
  m.m[0]=w*w+x*x-y*y-z*z; m.m[1]=2*x*y+2*w*z;     m.m[2]=2*x*z-2*w*y;     m.m[3]=0;
  m.m[4]=2*x*y-2*w*z;     m.m[5]=w*w-x*x+y*y-z*z; m.m[6]=2*y*z+2*w*x;     m.m[7]=0;
  m.m[8]=2*x*z+2*w*y;     m.m[9]=2*y*z-2*w*x;     m.m[10]=w*w-x*x-y*y+z*z; m.m[11]=0;
  m.m[12]=0; m.m[13]=0; m.m[14]=0; m.m[15]=1;
  return m;
}

/* ------------------------------------------------- strip index builder ---- */
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
  for (int i = 1; i <= xQuads; i++) { out.push_back(start + i * hstride); out.push_back(start + i * hstride); }
  for (int j = 0; j < yQuads; j++)
  {
    out.push_back(start + j * vstride);
    for (int i = 0; i <= xQuads; i++)
    { out.push_back(start + j * vstride + i * hstride); out.push_back(start + (j + 1) * vstride + i * hstride); }
    if (j < yQuads - 1) out.push_back(start + (j + 1) * vstride + xQuads * hstride);
  }
  if (doubleLast) out.push_back(start + yQuads * vstride + xQuads * hstride);
}

/* ------------------------------------------------- panel construction ----- */
typedef void (*DirFn)(const void* ctx, int i, int j, f32 out[3]);
typedef void (*SideFn)(const void* ctx, int i, int j, int border, f32 out[3]);

static void BuildPanel(ShieldMesh& M, DirFn dirOf, SideFn sideOf, const void* ctx, f32 inR, f32 outR)
{
  int width = SHIELD_WIDTH, height = SHIELD_HEIGHT;
  int perFace = (height + 1) * (width + 1);
  int sideCount = 2 * 2 * (height + 1) + 2 * 2 * (width + 1);
  int total = 2 * perFace + sideCount;

  M.pos.resize(total * 3);
  M.nrm.resize(total * 3);
  f32* P = &M.pos[0];
  f32* Nn = &M.nrm[0];
  #define PUT(at, d, radius, nn) { int _a=(at); \
    P[_a*3]=(d)[0]*(radius); P[_a*3+1]=(d)[1]*(radius); P[_a*3+2]=(d)[2]*(radius); \
    Nn[_a*3]=(nn)[0]; Nn[_a*3+1]=(nn)[1]; Nn[_a*3+2]=(nn)[2]; }

  int outer = 0;
  int inner = perFace + sideCount;
  for (int j = 0; j <= height; j++)
    for (int i = 0; i <= width; i++)
    {
      f32 n[3]; dirOf(ctx, i, j, n);
      f32 ni[3] = { -n[0], -n[1], -n[2] };
      PUT(outer, n, outR, n); outer++;
      PUT(inner, n, inR, ni); inner++;
    }

  int e = perFace;
  #define EDGE(i, j, border) { f32 n[3]; dirOf(ctx, (i), (j), n); f32 s[3]; sideOf(ctx, (i), (j), (border), s); \
    PUT(e, n, outR, s); e++; PUT(e, n, inR, s); e++; }
  for (int j = 0; j <= height; j++) EDGE(0, j, 0);
  for (int i = 0; i <= width; i++)  EDGE(i, height, 1);
  for (int j = height; j >= 0; j--) EDGE(width, j, 2);
  for (int i = width; i >= 0; i--)  EDGE(i, 0, 3);
  #undef EDGE
  #undef PUT

  std::vector<int> idx;
  TriStrip(idx, width, height, false, true, 0, 0, 0);
  TriStrip(idx, width, height, true, true, perFace + sideCount + width, 0, -1);
  int vertexIndex = perFace;
  for (int i = 0; i < 4; i++)
  {
    idx.push_back(vertexIndex);
    int length = (i & 1) ? width : height;
    for (int j = 0; j <= length; j++)
    { idx.push_back(vertexIndex); idx.push_back(vertexIndex + 1); vertexIndex += 2; }
    if (i < 3) idx.push_back(vertexIndex - 1);
  }

  M.idx.resize(idx.size());
  for (size_t k = 0; k < idx.size(); k++) M.idx[k] = (u16)idx[k];
}

/* shield (flat grid at x=1 normalised onto the sphere) */
static void ShieldDir(const void*, int i, int j, f32 out[3])
{
  f32 left = -0.5f * SHIELD_HORIZ_DIM, bottom = -0.5f * SHIELD_VERT_DIM;
  f32 hStep = SHIELD_HORIZ_DIM / SHIELD_WIDTH, vStep = SHIELD_VERT_DIM / SHIELD_HEIGHT;
  f32 v0 = 1.0f, v1 = left + hStep * i, v2 = bottom + vStep * j;
  f32 l = Hyp3(v0, v1, v2); if (l == 0) l = 1;
  out[0] = v0 / l; out[1] = v1 / l; out[2] = v2 / l;
}
static void ShieldSide(const void*, int, int, int border, f32 out[3])
{
  static const f32 sides[4][3] = { {0,-1,0}, {0,0,1}, {0,1,0}, {0,0,-1} };
  out[0]=sides[border][0]; out[1]=sides[border][1]; out[2]=sides[border][2];
}

/* zshield (real spherical lat/long band) */
struct ZCtx { f32 startRad, endRad; };
static void ZAngles(const ZCtx* z, int i, int j, f32& fj, f32& fi)
{
  f32 left = -0.5f * SHIELD_HORIZ_RADIANS;
  f32 hStep = SHIELD_HORIZ_RADIANS / SHIELD_WIDTH;
  f32 vStep = (z->endRad - z->startRad) / SHIELD_HEIGHT;
  fj = z->startRad + vStep * j;
  fi = left + hStep * i;
}
static void ZDir(const void* ctx, int i, int j, f32 out[3])
{
  const ZCtx* z = (const ZCtx*)ctx;
  f32 fj, fi; ZAngles(z, i, j, fj, fi);
  out[0] = (f32)cos(fj) * (f32)cos(fi);
  out[1] = (f32)cos(fj) * (f32)sin(fi);
  out[2] = (f32)sin(fj);
}
static void ZSide(const void* ctx, int i, int j, int border, f32 out[3])
{
  const ZCtx* z = (const ZCtx*)ctx;
  f32 fj, fi; ZAngles(z, i, j, fj, fi);
  f32 vs=(f32)sin(fj), vc=(f32)cos(fj), hs=(f32)sin(fi), hc=(f32)cos(fi);
  if (border == 0)      { out[0]=hs;     out[1]=-hc;    out[2]=0;  }
  else if (border == 1) { out[0]=-vs*hc; out[1]=-vs*hs; out[2]=vc; }
  else if (border == 2) { out[0]=-hs;    out[1]=hc;     out[2]=0;  }
  else                  { out[0]=vs*hc;  out[1]=vs*hs;  out[2]=-vc;}
}

/* ------------------------------------------------- shading ramps ---------- */
static f32 ShieldShading(f32 t)
{
  f32 shading = 0.75f;
  if (t < SHIELD_FADE_IN_START + SHIELD_FADE_IN_DELTA)
    shading *= (t - SHIELD_FADE_IN_START) / SHIELD_FADE_IN_DELTA;
  else if (t > SHIELD_FADE_OUT_START)
    shading *= (SHIELD_FADE_OUT_START + SHIELD_FADE_OUT_DELTA - t) / SHIELD_FADE_OUT_DELTA;
  return Clampf(shading, 0.0f, 1.0f);
}
static f32 ShieldIntensity(f32 t, f32 blob)
{
  f32 scale = Clampf((t - PUSHOUT_START_TIME) / PUSHOUT_DELTA, 0.0f, 1.0f);
  f32 v = blob * 2.0f * scale * scale;
  return v < 1.0f ? v : 1.0f;
}

/* ----------------------------------------------------- CShieldManager ----- */
CShieldManager::CShieldManager()
  : m_rand(0), m_midRadius(0), m_radiusScale(0), m_time(0), m_vb(0), m_vbVerts(0)
{
  m_center[0]=0; m_center[1]=0; m_center[2]=1;
}
CShieldManager::~CShieldManager() { Release(); }

void CShieldManager::Release()
{
  SAFE_RELEASE(m_vb);
}

void CShieldManager::Build(QuickRand& rand)
{
  m_rand = &rand;
  m_midRadius = (SHIELD_INSIDE_RADIUS + SHIELD_OUTSIDE_RADIUS) * 0.5f;
  m_radiusScale = 1.0f - (1.2f * (SHIELD_OUTSIDE_RADIUS - SHIELD_INSIDE_RADIUS)) / SHIELD_OUTSIDE_RADIUS;

  BuildPanel(m_panel, ShieldDir, ShieldSide, 0, SHIELD_INSIDE_RADIUS, SHIELD_OUTSIDE_RADIUS);

  // zshield radii use the scale left after the 3 shields (radiusScale^3).
  f32 zscale = m_radiusScale * m_radiusScale * m_radiusScale;
  f32 minRad = -0.45f * kPI;
  f32 step = (0.9f * kPI) / MAX_ZSHIELDS;
  for (int i = 0; i < MAX_ZSHIELDS; i++)
  {
    f32 midRad = minRad + step;
    ZCtx z; z.startRad = minRad; z.endRad = midRad;
    BuildPanel(m_zmesh[i], ZDir, ZSide, &z, (zscale * m_midRadius) - 0.5f, zscale * m_midRadius);
    minRad = midRad;
  }

  Restart();
}

bool CShieldManager::CreateBuffers(LPDIRECT3DDEVICE8 dev)
{
  // size to the largest strip among panel + zmeshes
  int maxv = (int)m_panel.idx.size();
  for (int i = 0; i < MAX_ZSHIELDS; i++)
    if ((int)m_zmesh[i].idx.size() > maxv) maxv = (int)m_zmesh[i].idx.size();
  m_vbVerts = maxv;
  if (FAILED(dev->CreateVertexBuffer(maxv * sizeof(ShieldVtx),
                                     D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, SHIELD_FVF,
                                     D3DPOOL_DEFAULT, &m_vb)))
  { m_vb = 0; return false; }
  return true;
}

void CShieldManager::NewShield(Shield& s)
{
  f32 crossing = m_rand->Rand01() * 2.09f * kPI;
  f32 arc = kPI * 1.2f;
  bool flipped = false;
  f32 rz = m_rand->Rand01() * 2.0f * kPI;
  f32 ry = m_rand->Rand01() * arc * 2.0f - arc * 0.5f;
  if (ry > arc * 0.5f) { ry += kPI - arc; flipped = true; }
  s.startRotation = YRotation(ry) * ZRotation(rz);   // shieldMul(yRot, zRot)
  s.rotationDir[0] = s.startRotation.m[8];
  s.rotationDir[1] = s.startRotation.m[9];
  s.rotationDir[2] = s.startRotation.m[10];
  s.thetaZero = flipped ? (rz + kPI - crossing) : (-rz - crossing);
  s.speed = 0.0f;
  s.radiusScale = 1.0f;
  s.objectCenter[0] = s.objectCenter[1] = s.objectCenter[2] = 0.0f;
}

void CShieldManager::Restart()
{
  f32 scale = 1.0f;
  for (int i = 0; i < MAX_SHIELDS; i++)
  {
    NewShield(m_shields[i]);
    m_shields[i].radiusScale = scale;
    m_shields[i].objectCenter[0] = m_midRadius * scale;
    m_shields[i].objectCenter[1] = 0.0f;
    m_shields[i].objectCenter[2] = 0.0f;
    AdvanceShield(m_shields[i], 0.0f);
    scale *= m_radiusScale;
  }
  for (int i = 0; i < MAX_ZSHIELDS; i++)
  {
    m_zshields[i].theta = 0.0f;
    m_zshields[i].speed = 0.0f;
    AdvanceZShield(m_zshields[i], 0.0f);
  }
  m_time = 0.0f;
}

void CShieldManager::AdvanceShield(Shield& s, f32 dt)
{
  s.speed += dt * 0.8f;
  s.thetaZero = dt * SHIELD_ROTATION_RATE * s.speed + s.thetaZero;
  f32 half = s.thetaZero * 0.5f;
  f32 sn = (f32)sin(half);
  f32 q[4] = { s.rotationDir[0]*sn, s.rotationDir[1]*sn, s.rotationDir[2]*sn, (f32)cos(half) };
  Mat4 m = s.startRotation * RHQuatMatrix(q);
  for (int k = 0; k < 3; k++) { m.m[k] *= s.radiusScale; m.m[4+k] *= s.radiusScale; m.m[8+k] *= s.radiusScale; }
  m.m[12] = m_center[0] + m.m[0] * 2.0f;
  m.m[13] = m_center[1] + m.m[1] * 2.0f;
  m.m[14] = m_center[2] + m.m[2] * 2.0f;
  s.matrix = m;
  const f32* c = s.objectCenter;
  s.center[0] = c[0]*m.m[0] + c[1]*m.m[4] + c[2]*m.m[8]  + m.m[12];
  s.center[1] = c[0]*m.m[1] + c[1]*m.m[5] + c[2]*m.m[9]  + m.m[13];
  s.center[2] = c[0]*m.m[2] + c[1]*m.m[6] + c[2]*m.m[10] + m.m[14];
}

void CShieldManager::AdvanceZShield(ZShield& z, f32 dt)
{
  z.speed += dt * 0.8f;
  z.theta += z.speed * dt;
  Mat4 m = ZRotation(z.theta);
  m.m[12] += m.m[0] * 2.0f;
  m.m[13] += m.m[1] * 2.0f;
  m.m[14] += m.m[2] * 2.0f;
  z.matrix = m;
}

void CShieldManager::Seek(f32 target)
{
  if (target < m_time) Restart();
  int guard = 0;
  while (m_time + BLOB_SIM_DT <= target && guard++ < 4000)
  {
    m_time += BLOB_SIM_DT;
    for (int i = 0; i < MAX_SHIELDS; i++) AdvanceShield(m_shields[i], BLOB_SIM_DT);
    for (int i = 0; i < MAX_ZSHIELDS; i++) AdvanceZShield(m_zshields[i], BLOB_SIM_DT);
  }
}

void CShieldManager::DrawPanel(LPDIRECT3DDEVICE8 dev, const ShieldMesh& mesh, const Mat4& world,
                               const CRGBA& tint, f32 intensity, f32 alpha)
{
  int n = (int)mesh.idx.size();
  if (!m_vb || n < 3 || n > m_vbVerts) return;
  const f32* P = &mesh.pos[0];
  const f32* N = &mesh.nrm[0];
  const u16* id = &mesh.idx[0];
  const f32* m = world.m;

  ShieldVtx* vb = 0;
  if (FAILED(m_vb->Lock(0, 0, (BYTE**)&vb, 0))) return;
  for (int k = 0; k < n; k++)
  {
    int v = id[k];
    f32 px = P[v*3], py = P[v*3+1], pz = P[v*3+2];
    f32 nx = N[v*3], ny = N[v*3+1], nz = N[v*3+2];

    f32 wx = px*m[0] + py*m[4] + pz*m[8]  + m[12];
    f32 wy = px*m[1] + py*m[5] + pz*m[9]  + m[13];
    f32 wz = px*m[2] + py*m[6] + pz*m[10] + m[14];
    f32 wnx = nx*m[0] + ny*m[4] + nz*m[8];
    f32 wny = nx*m[1] + ny*m[5] + nz*m[9];
    f32 wnz = nx*m[2] + ny*m[6] + nz*m[10];
    f32 nl = Hyp3(wnx, wny, wnz); if (nl < 1e-6f) nl = 1.0f;
    wnx /= nl; wny /= nl; wnz /= nl;

    // b = (N.L_blob)^16 (blob light at origin), m = (N.L_mood)^16.
    f32 lbx = -wx, lby = -wy, lbz = -wz;                 // dir to blob (0,0,0)
    f32 ll = Hyp3(lbx, lby, lbz); if (ll < 1e-6f) ll = 1.0f;
    f32 b = (wnx*lbx + wny*lby + wnz*lbz) / ll;
    if (b < 0) b = 0; else if (b > 1) b = 1;
    b *= b; b *= b; b *= b; b *= b;                      // ^16

    f32 lmx = MOOD_LIGHT[0]-wx, lmy = MOOD_LIGHT[1]-wy, lmz = MOOD_LIGHT[2]-wz;
    f32 lm = Hyp3(lmx, lmy, lmz); if (lm < 1e-6f) lm = 1.0f;
    f32 mm = (wnx*lmx + wny*lmy + wnz*lmz) / lm;
    if (mm < 0) mm = 0; else if (mm > 1) mm = 1;
    mm *= mm; mm *= mm; mm *= mm; mm *= mm;              // ^16

    // hi = b*(0.2,0.5,0.2)+b = b*(1.2,1.5,1.2); lit = sat(sat(hi)+m)
    f32 hr = Clampf(b*1.2f,0,1), hg = Clampf(b*1.5f,0,1), hb = Clampf(b*1.2f,0,1);
    f32 lr = Clampf(hr+mm,0,1), lg = Clampf(hg+mm,0,1), lbc = Clampf(hb+mm,0,1);

    CRGBA c(lr*tint.r*intensity, lg*tint.g*intensity, lbc*tint.b*intensity, alpha);
    vb[k].x = wx; vb[k].y = wy; vb[k].z = wz; vb[k].color = c.RenderColor();
  }
  m_vb->Unlock();

  dev->SetStreamSource(0, m_vb, sizeof(ShieldVtx));
  dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, n - 2);
}

void CShieldManager::Draw(LPDIRECT3DDEVICE8 dev, f32 t, const CTheme& theme,
                          const Vec3& eye, const Vec3& look, f32 energyBlob)
{
  Seek(t);
  f32 alpha = ShieldShading(t);
  if (alpha <= 0.002f) return;
  f32 intensity = ShieldIntensity(t, energyBlob);
  bool wire = theme.shieldWireframe;

  // translucent, no depth write, back-face cull (D3DCULL_CCW keeps the outer face)
  d3dSetRenderState(D3DRS_LIGHTING, FALSE);
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  d3dSetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  d3dSetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  // The reference culls back faces (GL frontFace CW + cullBack). Our view matrix
  // matches camera.js (x = cross(forward,up), a reflection) and D3D's viewport is
  // Y-flipped vs WebGL; both invert screen winding, so the D3D equivalent is CW,
  // not CCW. With CCW we were showing the bright inner face instead of the dark
  // outer one.
  d3dSetRenderState(D3DRS_CULLMODE, wire ? D3DCULL_NONE : D3DCULL_CW);
  d3dSetRenderState(D3DRS_FILLMODE, wire ? D3DFILL_WIREFRAME : D3DFILL_SOLID);

  dev->SetTexture(0, NULL);
  d3dSetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
  d3dSetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
  d3dSetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
  d3dSetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
  d3dSetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
  d3dSetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

  dev->SetVertexShader(SHIELD_FVF);

  // WORLD identity: panel vertices are transformed to world on the CPU.
  D3DMATRIX id = Mat4::Identity().ToD3D();
  dev->SetTransform(D3DTS_WORLD, &id);

  Vec3 dir = look - eye;
  f32 blobDot = m_center[0]*dir.x + m_center[1]*dir.y + m_center[2]*dir.z;

  // Far side first, then near, so the translucent panels layer right.
  for (int pass = 0; pass < 2; pass++)   // pass 0 = far (d>=blobDot), 1 = near
  {
    for (int i = 0; i < MAX_SHIELDS; i++)
    {
      Shield& s = m_shields[i];
      f32 d = s.center[0]*dir.x + s.center[1]*dir.y + s.center[2]*dir.z;
      if (pass == 0 ? (d < blobDot) : (d >= blobDot)) continue;
      DrawPanel(dev, m_panel, s.matrix, theme.shield, intensity, alpha);
    }
  }
  for (int i = 0; i < MAX_ZSHIELDS; i++)
    DrawPanel(dev, m_zmesh[i], m_zshields[i].matrix, theme.shield, intensity, alpha);

  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  d3dSetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  d3dSetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}
