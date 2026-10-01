/*
 *  Stub of <kodi/addon-instance/Screensaver.h> for the standalone MatrixTrails runner.
 *
 *  This is NOT the real Kodi addon dev-kit. It provides just enough of the
 *  kodi::addon::CAddonBase / CInstanceScreensaver surface for the unmodified
 *  screensaver adapter (src/main.cpp) to compile and run as a plain Xbox .xbe,
 *  driven directly by test/runner.cpp - so the screensaver logic is fully
 *  source-level debuggable. The production addon never sees this file.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <string>

#ifndef ATTR_DLL_LOCAL
#define ATTR_DLL_LOCAL
#endif
#ifndef ATTR_DLL_EXPORT
#define ATTR_DLL_EXPORT
#endif

namespace kodi
{
// Real dev-kit: using HardwareContext = ADDON_HARDWARE_CONTEXT; main.cpp casts it
// to LPDIRECT3DDEVICE8, so void* is enough.
typedef void* HardwareContext;

namespace addon
{

// Settings come from the addon's documented defaults (resources/settings.xml).
inline int ATTR_DLL_LOCAL GetSettingInt(const std::string& id, int defaultValue = 0)
{
  if (id == "columns") return 100;
  if (id == "rows")    return 60;
  if (id == "speed")   return 2;
  return defaultValue;
}

inline float ATTR_DLL_LOCAL GetSettingFloat(const std::string& id, float defaultValue = 0.0f)
{
  if (id == "rain-red")    return 0.0f;
  if (id == "rain-green")  return 100.0f;
  if (id == "rain-blue")   return 0.0f;
  if (id == "event-red")   return 80.0f;
  if (id == "event-green") return 100.0f;
  if (id == "event-blue")  return 90.0f;
  return defaultValue;
}

inline bool ATTR_DLL_LOCAL GetSettingBoolean(const std::string&, bool defaultValue = false)
{ return defaultValue; }
inline std::string ATTR_DLL_LOCAL GetSettingString(const std::string&, const std::string& def = "")
{ return def; }

// On Xbox the running image is mounted at D:\, so assets live under D:\resources\.
inline std::string ATTR_DLL_LOCAL GetAddonPath(const std::string& append = "")
{ return std::string("D:\\") + append; }

class ATTR_DLL_LOCAL CAddonBase
{
public:
  CAddonBase() {}
  virtual ~CAddonBase() {}
};

class ATTR_DLL_LOCAL CInstanceScreensaver
{
public:
  CInstanceScreensaver() : m_device(0), m_x(0), m_y(0), m_width(0), m_height(0) {}
  virtual ~CInstanceScreensaver() {}

  virtual bool Start() { return true; }
  virtual void Stop() {}
  virtual void Render() {}

  // The runner hands over the render-target properties Kodi would normally supply.
  void SetScreensaverProps(kodi::HardwareContext device, int x, int y, int width, int height)
  { m_device = device; m_x = x; m_y = y; m_width = width; m_height = height; }

protected:
  kodi::HardwareContext Device() { return m_device; }
  int X() { return m_x; }
  int Y() { return m_y; }
  int Width() { return m_width; }
  int Height() { return m_height; }

private:
  kodi::HardwareContext m_device;
  int m_x, m_y, m_width, m_height;
};

// Factory registration - the real ADDONCREATOR emits DLL entry points; here it
// records how to construct the addon so the runner can instantiate it.
typedef CInstanceScreensaver* (*ScreensaverCreateFn)();
inline ScreensaverCreateFn& ScreensaverFactory()
{ static ScreensaverCreateFn fn = 0; return fn; }

} // namespace addon
} // namespace kodi

#define ADDONCREATOR(AddonClass)                                                    \
  static kodi::addon::CInstanceScreensaver* AddonClass##_TestCreate()               \
  { return new AddonClass(); }                                                      \
  namespace                                                                         \
  {                                                                                 \
    const bool AddonClass##_TestReg =                                               \
        (kodi::addon::ScreensaverFactory() = &AddonClass##_TestCreate, true);       \
  }
