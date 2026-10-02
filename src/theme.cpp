/*
 *  theme.cpp — bootanim.ini parser (see theme.h).
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

  shieldRender    = true;
  shieldWireframe = false;
  shield          = ColorFromRGB(0x66ff4d);

  plasmaRender    = true;
  plasma1         = ColorFromRGB(0x00ff00);
  plasma2         = ColorFromRGB(0x9fff66);
  plasma3         = ColorFromRGB(0xa0ff60);
}

/* --- small value parsers ------------------------------------------------- */
static void Trim(char* s)
{
  // strip trailing whitespace / CR / LF
  int n = (int)strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
    s[--n] = 0;
}

static bool ParseBool(const char* v, bool def)
{
  if (!v || !*v) return def;
  if (_strnicmp(v, "true", 4) == 0 || v[0] == '1') return true;
  if (_strnicmp(v, "false", 5) == 0 || v[0] == '0') return false;
  return def;
}

static unsigned long ParseColorRaw(const char* v, unsigned long def)
{
  if (!v || !*v) return def;
  if (v[0] == '#')                                   return strtoul(v + 1, 0, 16);
  if (v[0] == '0' && (v[1] == 'x' || v[1] == 'X'))   return strtoul(v + 2, 0, 16);
  return strtoul(v, 0, 10);
}

static int ParseIntClamped(const char* v, int def, int lo, int hi)
{
  if (!v || !*v) return def;
  int n = (int)strtol(v, 0, 10);
  if (n < lo) n = lo;
  if (n > hi) n = hi;
  return n;
}

bool CTheme::Load(const std::string& path)
{
  FILE* fp = fopen(path.c_str(), "r");
  if (!fp)
    return false;

  char line[512];
  while (fgets(line, sizeof(line), fp))
  {
    // comment / blank
    char* p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == ';' || *p == '#' || *p == 0 || *p == '\r' || *p == '\n')
      continue;

    char* eq = strchr(p, '=');
    if (!eq)
      continue;
    *eq = 0;
    char* key = p;
    char* val = eq + 1;

    Trim(key);
    // also trim trailing space just before '=' and leading space of value
    while (*val == ' ' || *val == '\t') ++val;
    Trim(val);

    // lower-case the key for case-insensitive compare
    for (char* k = key; *k; ++k) *k = (char)tolower((unsigned char)*k);

    if      (!strcmp(key, "blobrender"))      blobRender      = ParseBool(val, blobRender);
    else if (!strcmp(key, "blobwireframe"))   blobWireframe   = ParseBool(val, blobWireframe);
    else if (!strcmp(key, "blobcolor"))       blobColor       = ColorFromRGB(ParseColorRaw(val, 0x40ff26));
    else if (!strcmp(key, "blobglow"))        blobGlow        = ColorFromRGB(ParseColorRaw(val, 0xa0ff40));

    else if (!strcmp(key, "scenerender"))     sceneRender     = ParseBool(val, sceneRender);
    else if (!strcmp(key, "scenewireframe"))  sceneWireframe  = ParseBool(val, sceneWireframe);
    else if (!strcmp(key, "sceneintensity"))  sceneIntensity  = ParseIntClamped(val, sceneIntensity, 0, 8);
    else if (!strcmp(key, "sceneambient"))    sceneAmbient    = ColorFromRGB(ParseColorRaw(val, 0x35ff1a));
    else if (!strcmp(key, "scenediffuse"))    sceneDiffuse    = ColorFromRGB(ParseColorRaw(val, 0x35ff1a));
    else if (!strcmp(key, "scenespecular"))   sceneSpecular   = ColorFromRGB(ParseColorRaw(val, 0x35ff1a));

    else if (!strcmp(key, "shieldrender"))    shieldRender    = ParseBool(val, shieldRender);
    else if (!strcmp(key, "shieldwireframe")) shieldWireframe = ParseBool(val, shieldWireframe);
    else if (!strcmp(key, "shield"))          shield          = ColorFromRGB(ParseColorRaw(val, 0x66ff4d));

    else if (!strcmp(key, "plasmarender"))    plasmaRender    = ParseBool(val, plasmaRender);
    else if (!strcmp(key, "plasma1"))         plasma1         = ColorFromRGB(ParseColorRaw(val, 0x00ff00));
    else if (!strcmp(key, "plasma2"))         plasma2         = ColorFromRGB(ParseColorRaw(val, 0x9fff66));
    else if (!strcmp(key, "plasma3"))         plasma3         = ColorFromRGB(ParseColorRaw(val, 0xa0ff60));
    // anything else (end-logo keys, unknowns) is ignored by design.
  }

  fclose(fp);
  return true;
}
