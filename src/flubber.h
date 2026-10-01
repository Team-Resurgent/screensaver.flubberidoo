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
#include "theme.h"

#include <xtl.h>
#include <string>

// Timeline (seconds), from FlubberForge camera.js. The loopable window is
// [0, FINISH_START_TIME); everything at/after FINISH_START_TIME is the finish
// dive + logo, which this screensaver never renders.
namespace flubtime
{
  const f32 FINISH_START_TIME    = 5.2f;   // loop point
  const f32 SCENE_ANIM_START     = 0.85f;  // scene keyframe window start
  const f32 SCENE_ANIM_LEN       = 4.5f;   // ... length (ends 5.35)
  const f32 BLOB_STATIC_END_TIME = 0.6f;   // blob frozen before this
  const f32 DEMO_TOTAL_TIME      = 8.0f;   // full original demo (camera clamps to this)
}

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
};
