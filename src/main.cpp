/*
 *  Flubberidoo — Kodi/XBMC4Xbox screensaver adapter.
 *
 *  Thin bridge between Kodi's CInstanceScreensaver and the CFlubber engine:
 *  Start() acquires the device + loads resources/bootanim.ini, Render() steps
 *  the clock and draws one frame. All the interesting work is in the engine.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "main.h"
#include "flubber.h"
#include "timer.h"

#include <kodi/addon-instance/Screensaver.h>
#include <kodi/Filesystem.h>

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

  std::string iniPath = kodi::vfs::TranslateSpecialProtocol(
      kodi::addon::GetAddonPath().append("resources\\bootanim.ini"));

  if (!m_flubber->RestoreDevice((LPDIRECT3DDEVICE8)Device(), X(), Y(), Width(), Height(), iniPath))
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
