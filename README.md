# screensaver.flubberidoo addon for XBMC4Xbox

A faithful D3D8 re-creation of the classic Xbox boot animation as an
[XBMC4Xbox](https://github.com/Team-Resurgent) screensaver: the gooey "flubber"
blob pulsing at the heart of a slowly rotating scene while the camera drifts
around it, wrapped in shimmering shields. It loops endlessly — **no ending Xbox
logo, no sound** — and is fully themeable from the addon settings.

Ported from [flubberforge](https://github.com/Team-Resurgent/flubberforge) (itself
a WebGL re-port of the original Xbox `BootAnim` C++), back to native Xbox D3D8.

[![License: GPL-2.0-or-later](https://img.shields.io/badge/License-GPL%20v2+-blue.svg)](LICENSE.md)

## Build instructions

1. Install Visual Studio .NET 2003 and the Xbox XDK.
2. From a CMD prompt run `build.bat` — it initialises the `external/kodi4xbox`
   submodule (xbmc4xbox-redux), builds `src\flubberidoo.sln` in Release, and
   packages `screensaver.flubberidoo-<version>.zip`.

To iterate on the rendering with a source-level debugger, build the
**FlubberidooRunner** project (see [`test/README.md`](test/README.md)) instead of
the full addon.

## Theming

Open the screensaver's **Configure** dialog in Kodi (its settings are defined in
[`screensaver.flubberidoo/resources/settings.xml`](screensaver.flubberidoo/resources/settings.xml))
to recolour the blob / scene / shields / plasma, or toggle individual layers.
The camera is a fixed cinematic fly-by (not configurable). The standalone runner
has no settings UI and always uses the compiled-in defaults.
