# MatrixTrails standalone test runner

A small Xbox program that runs the MatrixTrails screensaver directly, so you can
iterate on and **debug** it without a full XBMC/Kodi install. Built with Visual
Studio .NET 2003 + the Xbox XDK.

It compiles the **unmodified** screensaver adapter + engine (`../src/main.cpp`,
`matrixtrails.cpp`, `column.cpp`) straight into the `.xbe` against the lightweight
Kodi stubs in [`stubs/`](stubs), then drives `Start()` / `Render()` / `Stop()`
itself. Because the screensaver code is compiled into the runner, it is **fully
source-level debuggable** in Visual Studio (breakpoints, stepping, watches).

> For validating the *packaged* `MatrixTrails.xbs` binary as XBMC4Xbox loads it,
> that's a separate concern (a PE-loading host) — this runner is the development /
> debugging vehicle.

## How it works

- `stubs/kodi/addon-instance/Screensaver.h` + `stubs/kodi/Filesystem.h` provide a
  minimal `kodi::addon::CAddonBase` / `CInstanceScreensaver` and the few free
  functions `main.cpp` uses (`GetSettingInt/Float`, `GetAddonPath`,
  `TranslateSpecialProtocol`). The project puts `stubs/` first on the include path.
- `ADDONCREATOR(...)` (normally the DLL entry point) is redefined by the stub to
  register a factory; `runner.cpp` calls it, hands the instance the D3D8 device +
  screen size, then runs the frame loop.
- The engine's `d3dSetRenderState` / `d3dSetTextureStageState` / `d3dGetRenderState`
  helpers are implemented in `runner.cpp` against the runner's own device (the
  project defines `MATRIXTRAILS_NO_DX8_LIB_PRAGMA` so `main.h`'s `xbox_dx8.lib`
  `#pragma` is skipped).

## Build & run

Open the solution in VS2003 and build **MatrixTrailsRunner** (Release|Xbox or
Debug|Xbox). The pre-build step copies `screensaver.matrixtrails\resources\
MatrixTrails.tga` into the output's `resources\`, and Xbox Deployment pushes it to
the console. Launch `MatrixTrailsRunner.xbe` (xemu / Cxbx-Reloaded / devkit); the
screensaver loads its texture from `D:\resources\MatrixTrails.tga`.

Set breakpoints anywhere in `main.cpp` / `matrixtrails.cpp` / `column.cpp` and
F5 to debug. There is no built-in quit — stop the emulator or reset.
