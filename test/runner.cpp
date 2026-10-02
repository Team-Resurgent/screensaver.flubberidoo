/*
 *  Standalone Flubberidoo test runner (recompile host).
 *
 *  Compiles the unmodified screensaver adapter + engine (src/main.cpp,
 *  flubber.cpp, theme.cpp, geometry_data.cpp) straight into this .xbe, against
 *  the lightweight Kodi stubs in test/stubs, and drives Start()/Render()/Stop()
 *  directly. Because the screensaver code is compiled into the runner, it is
 *  fully source-level debuggable in Visual Studio (breakpoints, stepping).
 *
 *  The runner has no Kodi settings UI, so it just renders with the engine's
 *  compiled-in theme defaults -- no asset files need to be deployed.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <xtl.h>

#include <kodi/addon-instance/Screensaver.h>

static const int kW = 640;
static const int kH = 480;

static LPDIRECT3D8       g_pD3D    = 0;
static LPDIRECT3DDEVICE8 g_pDevice = 0;

/* ---------------------------------------------------------------------------
 * D3D8 state wrappers. main.h declares these extern "C" helpers; in the real
 * .xbs they resolve to xbox_dx8.dll (routing to XBMC's host device). A standalone
 * .xbe owns its device, so we implement them directly against it. The project
 * defines FLUBBERIDOO_NO_DX8_LIB_PRAGMA so main.h's #pragma is skipped and we
 * don't link xbox_dx8.lib.
 * ------------------------------------------------------------------------- */
extern "C" void d3dSetRenderState(DWORD state, DWORD value)
{ if (g_pDevice) g_pDevice->SetRenderState((D3DRENDERSTATETYPE)state, value); }

extern "C" void d3dGetRenderState(DWORD state, DWORD* value)
{ if (g_pDevice && value) g_pDevice->GetRenderState((D3DRENDERSTATETYPE)state, value); }

extern "C" void d3dSetTextureStageState(int stage, DWORD type, DWORD value)
{ if (g_pDevice) g_pDevice->SetTextureStageState(stage, (D3DTEXTURESTAGESTATETYPE)type, value); }

static bool CreateDevice()
{
  g_pD3D = Direct3DCreate8(D3D_SDK_VERSION);
  if (!g_pD3D) return false;
  D3DPRESENT_PARAMETERS pp;
  ZeroMemory(&pp, sizeof(pp));
  pp.BackBufferWidth = kW; pp.BackBufferHeight = kH;
  pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.BackBufferCount = 1;
  pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = FALSE;
  pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
  pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;
  HRESULT hr = g_pD3D->CreateDevice(0, D3DDEVTYPE_HAL, NULL,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_pDevice);
  return SUCCEEDED(hr) && g_pDevice != 0;
}

void __cdecl main()
{
  if (!CreateDevice())
    return;

  // Construct the real screensaver adapter via the factory that ADDONCREATOR
  // registered in src/main.cpp.
  kodi::addon::CInstanceScreensaver* screensaver = 0;
  if (kodi::addon::ScreensaverFactory())
    screensaver = kodi::addon::ScreensaverFactory()();

  if (screensaver)
  {
    // Hand over the render-target properties Kodi would normally supply, then start.
    screensaver->SetScreensaverProps((kodi::HardwareContext)g_pDevice, 0, 0, kW, kH);
    screensaver->Start();
  }

  // Render loop. No exit path by design; stop the emulator or reset to quit.
  for (;;)
  {
    g_pDevice->BeginScene();
    if (screensaver)
      screensaver->Render();   // engine clears + draws the frame
    g_pDevice->EndScene();
    g_pDevice->Present(NULL, NULL, NULL, NULL);
  }
}
