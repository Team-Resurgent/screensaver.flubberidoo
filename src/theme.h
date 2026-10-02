/*
 *  theme.h — Flubberidoo runtime theming, parsed from bootanim.ini.
 *
 *  Mirrors the FlubberForge theme.js defaults and key set. Colors are stored as
 *  CRGBA (0..1 per channel). Only keys that affect the looping screensaver are
 *  kept; the original animation's end-logo keys (the Slash, TradeMark, Xbox and
 *  Brand families) parse-and-ignore so existing INIs still load.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "types.h"
#include <string>

class CTheme
{
public:
  CTheme() { SetDefaults(); }

  void SetDefaults();
  // Load overrides from an .ini file. Missing file / missing keys keep defaults.
  // Returns false only if the path could not be opened (defaults remain in place).
  bool Load(const std::string& path);

  // Blob
  bool   blobRender;
  bool   blobWireframe;
  CRGBA  blobColor;
  CRGBA  blobGlow;

  // Scene
  bool   sceneRender;
  bool   sceneWireframe;
  int    sceneIntensity;  // 0..8
  CRGBA  sceneAmbient;
  CRGBA  sceneDiffuse;
  CRGBA  sceneSpecular;

  // Shields
  bool   shieldRender;
  bool   shieldWireframe;
  CRGBA  shield;

  // Plasma / background glow
  bool   plasmaRender;
  CRGBA  plasma1;
  CRGBA  plasma2;
  CRGBA  plasma3;
};
