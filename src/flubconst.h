/*
 *  flubconst.h — timeline constants, verbatim from FlubberForge camera.js
 *  (which mirrors BootAnimRXDK defines.h). Shared by the blob sim, camera, and
 *  the engine so the phases line up exactly.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"

namespace flubtime
{
  const f32 DEMO_TOTAL_TIME          = 8.0f;
  const f32 FINAL_HOLD_TIME          = 2.0f;
  const f32 FINISH_TRANSITION_TIME   = 0.8f;
  const f32 FINISH_START_TIME        = DEMO_TOTAL_TIME - FINAL_HOLD_TIME - FINISH_TRANSITION_TIME; // 5.2
  const f32 FINISH_STOP_TIME         = DEMO_TOTAL_TIME - FINAL_HOLD_TIME;                          // 6.0

  const f32 BLOB_STATIC_END_TIME     = 0.6f;
  const f32 BLOB_ZERO_INTENSE_END    = BLOB_STATIC_END_TIME + 0.5f;                                // 1.1
  const f32 MAX_INTENSITY_DELTA      = FINISH_START_TIME - BLOB_ZERO_INTENSE_END;                  // 4.1
  const f32 OO_MAX_INTENSITY_DELTA   = 1.0f / MAX_INTENSITY_DELTA;

  const f32 BLOB_PULSE_START         = BLOB_STATIC_END_TIME;                                       // 0.6
  const f32 BLOB_PULSE_END           = FINISH_STOP_TIME - 0.4f;                                    // 5.6
  const f32 BLOB_PULSE_ELAPSED       = BLOB_PULSE_END - BLOB_PULSE_START;                          // 5.0

  const f32 SCENE_ANIM_LEN           = 4.5f;
  const f32 SCENE_ANIM_START_TIME    = BLOB_STATIC_END_TIME + 0.25f;                               // 0.85

  const f32 SHIELD_FADE_IN_START     = BLOB_STATIC_END_TIME;                                       // 0.6
  const f32 SHIELD_FADE_IN_DELTA     = 1.2f;
  const f32 SHIELD_FADE_OUT_START    = FINISH_START_TIME - 0.1f;                                   // 5.1
  const f32 SHIELD_FADE_OUT_DELTA    = FINISH_TRANSITION_TIME * 0.2f;                              // 0.16

  const f32 PUSHOUT_START_TIME       = 0.5f;
  const f32 PUSHOUT_DELTA            = 2.7f;
}
