/*
 *  flubmath.h — small vector/matrix helpers for the Flubberidoo port.
 *
 *  Conventions match FlubberForge / D3D: row-major matrices, row-vector
 *  multiplication (v' = v * M), left-handed. A Mat4 maps 1:1 onto D3DMATRIX, so
 *  ToD3D() is a plain copy. Quaternion->matrix and the spline/keyframe samplers
 *  live with the modules that own them (anim/camera) so their exact sign and
 *  interpolation conventions stay next to the source they mirror.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include <math.h>
#include <xtl.h>   // D3DMATRIX

struct Vec3
{
  f32 x, y, z;
  Vec3() : x(0), y(0), z(0) {}
  Vec3(f32 X, f32 Y, f32 Z) : x(X), y(Y), z(Z) {}

  Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
  Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
  Vec3 operator*(f32 s)         const { return Vec3(x * s, y * s, z * s); }
};

inline f32  Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(const Vec3& a, const Vec3& b)
{ return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline f32  Length(const Vec3& v) { return (f32)sqrt(Dot(v, v)); }
inline Vec3 Normalize(const Vec3& v)
{
  f32 len = Length(v);
  if (len < FLOATEPSILON) return Vec3(0, 0, 0);
  f32 inv = 1.0f / len;
  return Vec3(v.x * inv, v.y * inv, v.z * inv);
}
inline Vec3 Lerp(const Vec3& a, const Vec3& b, f32 t)
{ return Vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

/* Row-major 4x4, row-vector convention (matches D3D and the JS upload order). */
struct Mat4
{
  f32 m[16];   // m[row*4 + col]

  static Mat4 Identity()
  {
    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = 0.0f;
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
  }

  // C = this * b  (apply 'this' then 'b' to a row vector: v*(this*b) = (v*this)*b)
  Mat4 operator*(const Mat4& b) const
  {
    Mat4 c;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
      {
        f32 s = 0.0f;
        for (int k = 0; k < 4; ++k) s += m[i * 4 + k] * b.m[k * 4 + j];
        c.m[i * 4 + j] = s;
      }
    return c;
  }

  // Scale each basis row (object-space per-axis scale applied before 'this').
  // Equals scaleMat(s) * this in the JS (render.js worldOf).
  Mat4 ScaleRows(f32 sx, f32 sy, f32 sz) const
  {
    Mat4 c = *this;
    for (int k = 0; k < 4; ++k) { c.m[0 + k] *= sx; c.m[4 + k] *= sy; c.m[8 + k] *= sz; }
    return c;
  }

  Vec3 Translation() const { return Vec3(m[12], m[13], m[14]); }
  void SetTranslation(const Vec3& t) { m[12] = t.x; m[13] = t.y; m[14] = t.z; }

  D3DMATRIX ToD3D() const
  {
    D3DMATRIX d;
    d._11 = m[0];  d._12 = m[1];  d._13 = m[2];  d._14 = m[3];
    d._21 = m[4];  d._22 = m[5];  d._23 = m[6];  d._24 = m[7];
    d._31 = m[8];  d._32 = m[9];  d._33 = m[10]; d._34 = m[11];
    d._41 = m[12]; d._42 = m[13]; d._43 = m[14]; d._44 = m[15];
    return d;
  }
};

inline f32 Clampf(f32 x, f32 lo, f32 hi) { return x < lo ? lo : (x > hi ? hi : x); }

/* View matrix (row-major), matching FlubberForge camera.js lookAtMatrix exactly:
 * xAxis = normalize(cross(forward, up)), yAxis = cross(xAxis, forward). This is
 * NOT the stock D3DXMatrixLookAtLH basis (which uses cross(up, forward)); the
 * difference negates X, so using the stock one would render x-mirrored vs the
 * reference. 'up' is world Z (0,0,1) for this scene. */
inline Mat4 BuildLookAtLH(const Vec3& eye, const Vec3& at, const Vec3& up)
{
  Vec3 z = Normalize(at - eye);
  Vec3 x = Cross(z, up);
  if (Length(x) < 1e-5f) x = Vec3(1, 0, 0);
  x = Normalize(x);
  Vec3 y = Cross(x, z);
  Mat4 r = Mat4::Identity();
  r.m[0] = x.x; r.m[1] = y.x; r.m[2]  = z.x;
  r.m[4] = x.y; r.m[5] = y.y; r.m[6]  = z.y;
  r.m[8] = x.z; r.m[9] = y.z; r.m[10] = z.z;
  r.m[12] = -Dot(x, eye); r.m[13] = -Dot(y, eye); r.m[14] = -Dot(z, eye);
  return r;
}

/* Left-handed perspective (row-major), native D3D depth range [0,1].
 * aspect = width/height. fovY in radians. */
inline Mat4 BuildPerspectiveFovLH(f32 fovY, f32 aspect, f32 zn, f32 zf)
{
  f32 yScale = 1.0f / (f32)tan(fovY * 0.5f);
  f32 xScale = yScale / aspect;
  Mat4 r;
  for (int i = 0; i < 16; ++i) r.m[i] = 0.0f;
  r.m[0]  = xScale;
  r.m[5]  = yScale;
  r.m[10] = zf / (zf - zn);
  r.m[11] = 1.0f;
  r.m[14] = -zn * zf / (zf - zn);
  return r;
}
