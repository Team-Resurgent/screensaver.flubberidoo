@echo off
setlocal
pushd "%~dp0"

REM Builds the MatrixTrails screensaver addon and packages it as a zip.
REM
REM No arguments / no "CORE" path needed: the dev-kit and Boost come from the
REM external/kodi4xbox submodule (xbmc4xbox-redux), referenced directly by the
REM project's include path, and the matrixtrails project's post-build step copies
REM MatrixTrails.xbs into screensaver.matrixtrails\. Requires Visual Studio .NET
REM 2003 (VS71COMNTOOLS) + the Xbox XDK, git, and 7z on PATH.

SET ADDON_NAME=MatrixTrails
SET ADDON_NAME_ID=screensaver.matrixtrails
SET VS_SOLUTION=src\matrixtrails.sln
SET DLL_ADDON=%ADDON_NAME%.xbs
REM TODO: parse from addon.xml
SET VERSION=2.0.0

if not exist "external\kodi4xbox\xbmc\addons\kodi-dev-kit\include\kodi\addon-instance\Screensaver.h" (
  ECHO Initializing submodule...
  git submodule update --init --recursive || goto failed
)

ECHO Building addon...
"%VS71COMNTOOLS%\..\IDE\devenv.com" "%VS_SOLUTION%" /rebuild Release || goto failed
if not exist "src\Release\%DLL_ADDON%" goto failed
if not exist "%ADDON_NAME_ID%\%DLL_ADDON%" goto failed

ECHO Packaging addon...
if exist "%ADDON_NAME_ID%-%VERSION%.zip" del /q "%ADDON_NAME_ID%-%VERSION%.zip"
7z a -tzip "%ADDON_NAME_ID%-%VERSION%.zip" "%ADDON_NAME_ID%\*" || goto failed

ECHO Finished: %ADDON_NAME_ID%-%VERSION%.zip
popd
exit /b 0

:failed
ECHO Build failed.
popd
exit /b 1
