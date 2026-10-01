/*
 *  scene.cpp — see scene.h. Ported from FlubberForge render.js.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "main.h"      // d3dSetRenderState wrapper
#include "scene.h"
#include "geometry_data.h"

#include <math.h>
#include <string.h>

// app.cpp blobLight (see blob.js). Ambient is black, so sceneAmbient drops out.
static const f32 BLOB_LIGHT_AMBIENT  = 0.0f;
static const f32 BLOB_LIGHT_DIFFUSE  = 0.13f;
static const f32 BLOB_LIGHT_SPECULAR = 1.0f;
static const f32 BLOB_LIGHT_ATTEN0   = 1.0f;
static const f32 BLOB_LIGHT_ATTEN1   = 0.001f;
static const f32 BLOB_LIGHT_ATTEN2   = 0.001f;

struct SceneVtx { f32 x, y, z, nx, ny, nz; };
#define SCENE_FVF (D3DFVF_XYZ | D3DFVF_NORMAL)

/* ---------------------------------------------- animation sampling -------- */
static Mat4 MatFromQuat(const f32 q[4])
{
  f32 x = q[0], y = q[1], z = q[2], w = q[3];
  Mat4 m;
  m.m[0]  = w*w + x*x - y*y - z*z; m.m[1]  = 2*x*y - 2*w*z;       m.m[2]  = 2*x*z + 2*w*y;       m.m[3]  = 0;
  m.m[4]  = 2*x*y + 2*w*z;         m.m[5]  = w*w - x*x + y*y - z*z; m.m[6]  = 2*y*z - 2*w*x;       m.m[7]  = 0;
  m.m[8]  = 2*x*z - 2*w*y;         m.m[9]  = 2*y*z + 2*w*x;       m.m[10] = w*w - x*x - y*y + z*z; m.m[11] = 0;
  m.m[12] = 0; m.m[13] = 0; m.m[14] = 0; m.m[15] = 1;
  return m;
}

static void Slerp(const f32* a, const f32* b, f32 t, f32 out[4])
{
  f32 dp = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
  f32 bx = b[0], by = b[1], bz = b[2], bw = b[3];
  if (dp < 0.0f) { dp = -dp; bx = -bx; by = -by; bz = -bz; bw = -bw; }
  if (dp > 0.9995f)
  {
    out[0] = a[0]*(1-t) + bx*t; out[1] = a[1]*(1-t) + by*t;
    out[2] = a[2]*(1-t) + bz*t; out[3] = a[3]*(1-t) + bw*t;
    return;
  }
  f32 angle = (f32)acos(dp < 1.0f ? dp : 1.0f);
  f32 s = (f32)sin(angle);
  f32 t0 = (f32)sin(angle * (1 - t)) / s;
  f32 t1 = (f32)sin(angle * t) / s;
  out[0] = a[0]*t0 + bx*t1; out[1] = a[1]*t0 + by*t1;
  out[2] = a[2]*t0 + bz*t1; out[3] = a[3]*t0 + bw*t1;
}

static void SampleVec3(const f32 list[][3], const u16* seq, f32 fpos, f32 out[3])
{
  if (fpos <= 0.0f) { const f32* a = list[seq[0]];  out[0]=a[0]; out[1]=a[1]; out[2]=a[2]; return; }
  if (fpos >= 1.0f) { const f32* a = list[seq[29]]; out[0]=a[0]; out[1]=a[1]; out[2]=a[2]; return; }
  f32 x = fpos * 28.0f;
  int i = (int)x;
  f32 f = x - i;
  int j = i + 1; if (j > 29) j = 29;
  const f32* a = list[seq[i]];
  const f32* b = list[seq[j]];
  out[0] = a[0]*(1-f) + b[0]*f; out[1] = a[1]*(1-f) + b[1]*f; out[2] = a[2]*(1-f) + b[2]*f;
}

static void SampleQuat(const f32 quats[][4], const u16* seq, f32 fpos, f32 out[4])
{
  if (fpos <= 0.0f) { const f32* a = quats[seq[0]];  out[0]=a[0]; out[1]=a[1]; out[2]=a[2]; out[3]=a[3]; return; }
  if (fpos >= 1.0f) { const f32* a = quats[seq[29]]; out[0]=a[0]; out[1]=a[1]; out[2]=a[2]; out[3]=a[3]; return; }
  f32 x = fpos * 28.0f;
  int i = (int)x;
  int j = i + 1; if (j > 29) j = 29;
  Slerp(quats[seq[i]], quats[seq[j]], x - i, out);
}

static Mat4 WorldOf(const FlubInstance& it, f32 fpos)
{
  Mat4 basis;
  memcpy(basis.m, it.m, 16 * sizeof(f32));
  if (it.rot >= 0)
  {
    f32 q[4];
    SampleQuat(kFlubQuats, kFlubQuatSeq[it.rot], fpos, q);
    basis = basis * MatFromQuat(q);          // mul4(inst.m, quatMat)
  }
  if (it.pos >= 0)
  {
    f32 p[3];
    SampleVec3(kFlubPositions, kFlubPosSeq[it.pos], fpos, p);
    basis.m[12] += p[0]; basis.m[13] += p[1]; basis.m[14] += p[2];
  }
  return basis.ScaleRows(it.s[0], it.s[1], it.s[2]);   // scaleMat(s) * basis
}

/* ------------------------------------------------------------- CScene ----- */
CScene::CScene() : m_meshCount(0)
{
  for (int i = 0; i < 32; i++) { m_vb[i] = null; m_vertCount[i] = 0; }
}

CScene::~CScene() { Release(); }

void CScene::Release()
{
  for (int i = 0; i < 32; i++) SAFE_RELEASE(m_vb[i]);
  m_meshCount = 0;
}

bool CScene::Create(LPDIRECT3DDEVICE8 dev)
{
  m_meshCount = kFlubMeshCount;
  for (int mi = 0; mi < m_meshCount; mi++)
  {
    const FlubMesh& mesh = kFlubMeshes[mi];

    // Expand all parts (strip/fan/triangles) into one triangle list. The scene
    // draws with culling disabled, so winding/order does not matter and
    // degenerate triangles (repeated indices in a strip) are harmless.
    std::vector<SceneVtx> verts;
    for (int pi = 0; pi < mesh.partCount; pi++)
    {
      const FlubPart& part = mesh.parts[pi];
      const u16* id = part.idx;
      int c = part.count;
      const f32* P = mesh.pos;
      const f32* N = mesh.nrm;

      #define EMIT(v) { SceneVtx sv; int _v=(v); \
        sv.x=P[_v*3]; sv.y=P[_v*3+1]; sv.z=P[_v*3+2]; \
        sv.nx=N[_v*3]; sv.ny=N[_v*3+1]; sv.nz=N[_v*3+2]; verts.push_back(sv); }

      if (part.prim == FLUB_TRIS)
      {
        for (int n = 0; n + 2 < c; n += 3) { EMIT(id[n]); EMIT(id[n+1]); EMIT(id[n+2]); }
      }
      else if (part.prim == FLUB_FAN)
      {
        for (int n = 1; n + 1 < c; n++) { EMIT(id[0]); EMIT(id[n]); EMIT(id[n+1]); }
      }
      else // FLUB_STRIP
      {
        for (int n = 0; n + 2 < c; n++) { EMIT(id[n]); EMIT(id[n+1]); EMIT(id[n+2]); }
      }
      #undef EMIT
    }

    int vcount = (int)verts.size();
    m_vertCount[mi] = vcount;
    if (vcount == 0) continue;

    if (FAILED(dev->CreateVertexBuffer(vcount * sizeof(SceneVtx),
                                       D3DUSAGE_WRITEONLY, SCENE_FVF,
                                       D3DPOOL_DEFAULT, &m_vb[mi])))
    {
      m_vb[mi] = null;
      return false;
    }
    SceneVtx* dst = 0;
    if (FAILED(m_vb[mi]->Lock(0, 0, (BYTE**)&dst, 0)))
      return false;
    memcpy(dst, &verts[0], vcount * sizeof(SceneVtx));
    m_vb[mi]->Unlock();
  }
  return true;
}

void CScene::Draw(LPDIRECT3DDEVICE8 dev, f32 fpos, const CBlobSim* blob,
                  const CTheme& theme, const Vec3& camPos, f32 energyBlob)
{
  f32 si = theme.sceneIntensity ? (f32)theme.sceneIntensity : 1.0f;
  f32 intense = (energyBlob > 0.0f ? energyBlob : 0.0f) * si;
  f32 ooIntensity = intense > 0.0f ? 1.0f / intense : 0.0f;

  // scene_phong colours: theme * blob-light * intensity, folded into the light.
  D3DLIGHT8 light;
  memset(&light, 0, sizeof(light));
  light.Type = D3DLIGHT_POINT;
  light.Diffuse.r  = theme.sceneDiffuse.r  * BLOB_LIGHT_DIFFUSE  * intense;
  light.Diffuse.g  = theme.sceneDiffuse.g  * BLOB_LIGHT_DIFFUSE  * intense;
  light.Diffuse.b  = theme.sceneDiffuse.b  * BLOB_LIGHT_DIFFUSE  * intense;
  light.Specular.r = theme.sceneSpecular.r * BLOB_LIGHT_SPECULAR * intense;
  light.Specular.g = theme.sceneSpecular.g * BLOB_LIGHT_SPECULAR * intense;
  light.Specular.b = theme.sceneSpecular.b * BLOB_LIGHT_SPECULAR * intense;
  light.Range = 800.0f;
  light.Attenuation0 = BLOB_LIGHT_ATTEN0;
  light.Attenuation1 = BLOB_LIGHT_ATTEN1 * ooIntensity;
  light.Attenuation2 = BLOB_LIGHT_ATTEN2 * ooIntensity;

  D3DMATERIAL8 mat;
  memset(&mat, 0, sizeof(mat));
  mat.Diffuse.r = mat.Diffuse.g = mat.Diffuse.b = mat.Diffuse.a = 1.0f;
  mat.Specular.r = mat.Specular.g = mat.Specular.b = 1.0f;
  mat.Power = 32.0f;
  // Ambient / Emissive stay black (sceneAmbient is multiplied by 0).

  dev->SetMaterial(&mat);

  // Hardware point light == scene_phong (minus the inner saturate before falloff).
  d3dSetRenderState(D3DRS_LIGHTING, TRUE);
  d3dSetRenderState(D3DRS_SPECULARENABLE, TRUE);
  d3dSetRenderState(D3DRS_LOCALVIEWER, TRUE);
  d3dSetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
  d3dSetRenderState(D3DRS_COLORVERTEX, FALSE);
  d3dSetRenderState(D3DRS_AMBIENT, 0);
  d3dSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  d3dSetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  d3dSetRenderState(D3DRS_FILLMODE, theme.sceneWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
  (void)BLOB_LIGHT_AMBIENT;

  // No texture: the texture cascade just passes the lit diffuse through; the
  // hardware adds the specular component on top (D3DRS_SPECULARENABLE).
  dev->SetTexture(0, NULL);
  d3dSetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
  d3dSetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
  d3dSetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
  d3dSetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
  d3dSetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
  d3dSetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

  dev->SetVertexShader(SCENE_FVF);

  for (int ii = 0; ii < kFlubInstanceCount; ii++)
  {
    const FlubInstance& it = kFlubInstances[ii];
    if (it.mesh < 0 || it.mesh >= m_meshCount || !m_vb[it.mesh]) continue;

    Mat4 world = WorldOf(it, fpos);

    f32 wpos[3] = { world.m[12], world.m[13], world.m[14] };
    f32 lp[3];
    blob->LightFor(wpos, lp);
    light.Position.x = lp[0]; light.Position.y = lp[1]; light.Position.z = lp[2];
    dev->SetLight(0, &light);
    dev->LightEnable(0, TRUE);

    D3DMATRIX wm = world.ToD3D();
    dev->SetTransform(D3DTS_WORLD, &wm);

    dev->SetStreamSource(0, m_vb[it.mesh], sizeof(SceneVtx));
    dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, m_vertCount[it.mesh] / 3);
  }

  d3dSetRenderState(D3DRS_LIGHTING, FALSE);
  d3dSetRenderState(D3DRS_SPECULARENABLE, FALSE);
  d3dSetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}
