/*
 *  theme.cpp — theme defaults + colour helpers (see theme.h). Production fills
 *  the fields from Kodi addon settings in the adapter; the runner uses defaults.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "theme.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Pack an 0xRRGGBB integer into a CRGBA (alpha forced opaque). */
static CRGBA ColorFromRGB(unsigned long v)
{
  return CRGBA(((v >> 16) & 0xff) / 255.0f,
               ((v >> 8)  & 0xff) / 255.0f,
               ( v        & 0xff) / 255.0f,
               1.0f);
}

void CTheme::SetDefaults()
{

  blobRender      = true;
  blobWireframe   = false;
  blobColor       = ColorFromRGB(0x40ff26);
  blobGlow        = ColorFromRGB(0xa0ff40);

  sceneRender     = true;
  sceneWireframe  = false;
  sceneIntensity  = 2;
  sceneAmbient    = ColorFromRGB(0x35ff1a);
  sceneDiffuse    = ColorFromRGB(0x35ff1a);
  sceneSpecular   = ColorFromRGB(0x35ff1a);

  shieldRender    = false;
  shieldWireframe = false;
  shield          = ColorFromRGB(0x66ff4d);

  plasmaRender    = true;
  plasma1         = ColorFromRGB(0x00ff00);
  plasma2         = ColorFromRGB(0x9fff66);
  plasma3         = ColorFromRGB(0xa0ff60);
}

CRGBA CTheme::HexColor(const char* s, unsigned long def)
{
  unsigned long v = def;
  if (s && *s)
  {
    const char* p = s;
    if (*p == '#')                                 ++p;
    else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    if (*p) v = strtoul(p, 0, 16);
  }
  return ColorFromRGB(v);
}
