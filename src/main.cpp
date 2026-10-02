/*
 *  Flubberidoo — Kodi/XBMC4Xbox screensaver adapter.
 *
 *  Thin bridge between Kodi's CInstanceScreensaver and the CFlubber engine:
 *  Start() acquires the device + builds the theme (from Kodi addon settings in
 *  production, compiled-in defaults in the runner), Render() steps the clock and
 *  draws one frame. All the interesting work is in the engine.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "main.h"
#include "flubber.h"
#include "timer.h"

#include <kodi/addon-instance/Screensaver.h>

#include <string>

class ATTR_DLL_LOCAL CScreensaverFlubberidoo
  : public kodi::addon::CAddonBase,
    public kodi::addon::CInstanceScreensaver
{
public:
  CScreensaverFlubberidoo() : m_flubber(null), m_timer(null) {}
  virtual ~CScreensaverFlubberidoo() { Stop(); }

  virtual bool Start();
  virtual void Stop();
  virtual void Render();

private:
  CFlubber* m_flubber;
  CTimer*   m_timer;
};

////////////////////////////////////////////////////////////////////////////
// Kodi tells us to get ready to render. Acquire the device, load the theme.
//
bool CScreensaverFlubberidoo::Start()
{
  Stop();

  m_timer = new CTimer();
  m_timer->Init();
  m_timer->SetSpeed(1.0f);

  m_flubber = new CFlubber();

  // Build the theme. CTheme's ctor sets the compiled-in defaults (shields off).
  // The production addon overlays the Kodi addon settings (resources/settings.xml)
  // on top -- any setting not present keeps its default. The standalone runner has
  // no settings system, so it simply uses the compiled-in defaults (no files).
  CTheme theme;
#ifndef FLUBBERIDOO_NO_DX8_LIB_PRAGMA
  theme.blobRender      = kodi::addon::GetSettingBoolean("BlobRender",      theme.blobRender);
  theme.blobWireframe   = kodi::addon::GetSettingBoolean("BlobWireframe",   theme.blobWireframe);
  theme.blobColor       = CTheme::HexColor(kodi::addon::GetSettingString("BlobColor", "40ff26").c_str(), 0x40ff26);
  theme.blobGlow        = CTheme::HexColor(kodi::addon::GetSettingString("BlobGlow",  "a0ff40").c_str(), 0xa0ff40);
  theme.sceneRender     = kodi::addon::GetSettingBoolean("SceneRender",     theme.sceneRender);
  theme.sceneWireframe  = kodi::addon::GetSettingBoolean("SceneWireframe",  theme.sceneWireframe);
  theme.sceneIntensity  = kodi::addon::GetSettingInt("SceneIntensity",      theme.sceneIntensity);
  theme.sceneAmbient    = CTheme::HexColor(kodi::addon::GetSettingString("SceneAmbient",  "35ff1a").c_str(), 0x35ff1a);
  theme.sceneDiffuse    = CTheme::HexColor(kodi::addon::GetSettingString("SceneDiffuse",  "35ff1a").c_str(), 0x35ff1a);
  theme.sceneSpecular   = CTheme::HexColor(kodi::addon::GetSettingString("SceneSpecular", "35ff1a").c_str(), 0x35ff1a);
  theme.shieldRender    = kodi::addon::GetSettingBoolean("ShieldRender",    theme.shieldRender);
  theme.shieldWireframe = kodi::addon::GetSettingBoolean("ShieldWireframe", theme.shieldWireframe);
  theme.shield          = CTheme::HexColor(kodi::addon::GetSettingString("Shield", "66ff4d").c_str(), 0x66ff4d);
  theme.plasmaRender    = kodi::addon::GetSettingBoolean("PlasmaRender",    theme.plasmaRender);
  theme.plasma1         = CTheme::HexColor(kodi::addon::GetSettingString("Plasma1", "00ff00").c_str(), 0x00ff00);
  theme.plasma2         = CTheme::HexColor(kodi::addon::GetSettingString("Plasma2", "9fff66").c_str(), 0x9fff66);
  theme.plasma3         = CTheme::HexColor(kodi::addon::GetSettingString("Plasma3", "a0ff60").c_str(), 0xa0ff60);
#endif

  if (!m_flubber->RestoreDevice((LPDIRECT3DDEVICE8)Device(), X(), Y(), Width(), Height(), theme))
  {
    Stop();
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////
// Free everything.
//
void CScreensaverFlubberidoo::Stop()
{
  SAFE_DELETE(m_flubber);
  SAFE_DELETE(m_timer);
}

////////////////////////////////////////////////////////////////////////////
// Render one frame.
//
void CScreensaverFlubberidoo::Render()
{
  if (!m_flubber)
    return;
  m_timer->Update();
  m_flubber->Update(m_timer->GetDeltaTime());
  if (!m_flubber->Draw())
    Stop();
}

ADDONCREATOR(CScreensaverFlubberidoo);
