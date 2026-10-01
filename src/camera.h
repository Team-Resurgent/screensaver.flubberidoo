/*
 *  camera.h — TCB-Hermite camera spline, ported from FlubberForge camera.js
 *  (camera_controller.cpp). One of 15 built-in paths is chosen by the theme's
 *  CameraMode; position and look-at are splined independently. The 8 "finish"
 *  nodes (the dive into the logo) are still built so the last loop segment's
 *  tangents match, but the screensaver never samples past FINISH_START_TIME.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include "flubmath.h"

struct CamShot
{
  Vec3 pos;
  Vec3 look;
  bool renderGeom;
  bool renderSlash;
};

class CCamera
{
public:
  CCamera() : m_count(0), m_variableCount(0), m_lookStart(0), m_lookDelta(0) {}

  void    Build(int cameraMode);     // cameraMode as in the theme (<=0 => 1)
  CamShot Sample(f32 t) const;       // t in seconds

private:
  struct Node { f32 fTime; Vec3 pos, look, vel, lookW; };

  Node m_nodes[24];                  // up to 9 variable + 8 finish
  int  m_count;
  int  m_variableCount;
  Vec3 m_finalLook;
  f32  m_lookStart;
  f32  m_lookDelta;
};
