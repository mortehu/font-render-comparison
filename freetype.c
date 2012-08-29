/*
  FreeType wrapper
  Copyright (C) 2012  Morten Hustveit <morten.hustveit@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <err.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <wchar.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H

#include <otf.h>

#include "font.h"

struct FONT_Data
{
  OTF **otfs;
  FT_Face *faces;
  size_t faceCount;

  unsigned int spaceWidth;
};

struct FONT_Glyph
{
  uint16_t width, height;
  int16_t  x, y;
  int16_t  xOffset, yOffset;

  uint8_t data[1];
};

static FT_Library ft_library;

static FT_GlyphSlot
font_FreeTypeGlyphForCharacter (struct FONT_Data *font, wint_t character,
                                FT_UInt *previous, FT_Face *face, FT_Vector *delta,
                                unsigned int loadFlags);

struct FONT_Glyph *
FONT_GlyphWithSize (unsigned int width, unsigned int height);

int
FONT_PathsForFont (char ***paths, const char *name, unsigned int size)
{
  FcPattern *pattern;
  FcCharSet *charSet;
  FcFontSet *fontSet;
  FcResult fcResult;
  unsigned int i, result = 0;

  if (!FcInit ())
    return -1;

  pattern = FcNameParse ((FcChar8 *) name);

  FcPatternAddDouble (pattern, FC_SIZE, (double) size);

  FcConfigSubstitute (0, pattern, FcMatchPattern);
  FcDefaultSubstitute (pattern);

  fontSet = FcFontSort (0, pattern, FcTrue, &charSet, &fcResult);

  FcPatternDestroy (pattern);

  if (!fontSet)
    return -1;

  *paths = calloc (fontSet->nfont, sizeof (**paths));

  for (i = 0; i < fontSet->nfont; ++i)
    {
      FcChar8 *path = 0;

      FcPatternGetString (fontSet->fonts[i], FC_FILE, 0, &path);

      if (path)
        {
          (*paths)[result] = malloc (strlen ((const char *) path) + 1);
          strcpy ((*paths)[result], (const char *) path);

          ++result;
        }
    }

  FcFontSetDestroy (fontSet);

  return result;
}

struct FONT_Data *
FONT_Load (const char *name, unsigned int size)
{
  struct FONT_Data *result;
  FT_GlyphSlot tmpGlyph;
  char **paths;
  int i, pathCount;
  int ok = 0;

  pathCount = FONT_PathsForFont (&paths, name, size);

  if (pathCount <= 0)
    return NULL;

  result = calloc (1, sizeof (*result));

  if (!(result->faces = calloc (pathCount, sizeof (*result->faces))))
    goto fail;

  if (!(result->otfs = calloc (pathCount, sizeof (*result->otfs))))
    goto fail;

  result->faceCount = 0;

  for (i = 0; i < pathCount; ++i)
    {
      FT_Face face;
      int ret;

      if (0 != (ret = FT_New_Face (ft_library, paths[i], 0, &face)))
        {
          fprintf (stderr, "FT_New_Face on %s failed with code %d\n", paths[i], ret);

          continue;
        }

      if (0 != (ret = FT_Set_Char_Size (face, 0, size << 6, 0, 0)))
        {
          FT_Done_Face (face);

          fprintf (stderr, "FT_New_Face on %s failed with code %d\n", paths[i], ret);

          continue;
        }

      result->otfs[result->faceCount] = OTF_open_ft_face (face); /* Fails on non-OTF fonts */
      result->faces[result->faceCount++] = face;
    }

  if (!result->faceCount)
    {
      fprintf (stderr, "Failed to load any font faces for `%s'\n", name);

      goto fail;
    }

  tmpGlyph = font_FreeTypeGlyphForCharacter (result, ' ', NULL, NULL, 0, 0);
  result->spaceWidth = tmpGlyph->advance.x >> 6;

  ok = 1;

fail:

  if (!ok)
    {
      free (result);
      result = NULL;
    }

  for (i = 0; i < pathCount; ++i)
    free (paths[i]);
  free (paths);

  return result;
}

void
FONT_Free (struct FONT_Data *font)
{
  int i;

  for (i = 0; i < font->faceCount; ++i)
    FT_Done_Face (font->faces[i]);

  free (font->faces);
}

unsigned int
FONT_Ascent (struct FONT_Data *font)
{
  if (!font->faceCount)
    return 0.0;

  return font->faces[0]->size->metrics.ascender >> 6;
}

unsigned int
FONT_Descent (struct FONT_Data *font)
{
  if (!font->faceCount)
    return 0.0;

  return -font->faces[0]->size->metrics.descender >> 6;
}

unsigned int
FONT_LineHeight (struct FONT_Data *font)
{
  if (!font->faceCount)
    return 0.0;

  return font->faces[0]->size->metrics.height >> 6;
}

unsigned int
FONT_SpaceWidth (struct FONT_Data *font)
{
  return font->spaceWidth;
}

struct FONT_Glyph *
FONT_GlyphWithSize (unsigned int width, unsigned int height)
{
  struct FONT_Glyph *result;

  result = calloc (1, offsetof (struct FONT_Glyph, data) + width * height * 4);
  result->width = width;
  result->height = height;

  return result;
}

static FT_GlyphSlot
font_FreeTypeGlyphForCharacter (struct FONT_Data *font, wint_t character,
                                FT_UInt *previous, FT_Face *face, FT_Vector *delta,
                                unsigned int loadFlags)

{
  int faceIndex;

  for (faceIndex = 0; faceIndex < font->faceCount; ++faceIndex)
    {
      FT_Face currentFace;
      FT_UInt glyph;

      currentFace = font->faces[faceIndex];

      if (delta)
        FT_Set_Transform (currentFace, 0, delta);

      if (!(glyph = FT_Get_Char_Index (currentFace, character)))
        continue;

      if (!FT_Load_Glyph (currentFace, glyph, loadFlags))
        {
          FT_Render_Glyph (currentFace->glyph, FT_RENDER_MODE_NORMAL);

          if (face)
            *face = currentFace;

          if (previous)
            {
              if (*previous)
                {
                  FT_Vector delta;

                  FT_Get_Kerning (currentFace, *previous, glyph,
                                  FT_KERNING_UNSCALED, &delta);
                }

              *previous = glyph;
            }

          return currentFace->glyph;
        }
    }

  return 0;
}

void
freetype_render_string (cairo_t *cairo,
                        int x, int y,
                        const char *font,
                        unsigned int size,
                        const char *string,
                        float gamma)
{
  struct FONT_Data *fontData;
  int status;
  unsigned int charIndex = 0;
  FT_Vector pen;
  FT_UInt previous = 0;

  OTF_GlyphString otfGlyphString;

  if (0 != (status = FT_Init_FreeType (&ft_library)))
    errx (EXIT_FAILURE, "Failed to initialize FreeType with status %d", status);

  fontData = FONT_Load (font, size);

  pen.x = x << 6;
  pen.y = 0;

  otfGlyphString.size = strlen (string);
  otfGlyphString.used = otfGlyphString.size;
  otfGlyphString.glyphs = calloc (otfGlyphString.size, sizeof (*otfGlyphString.glyphs));

  for (charIndex = 0; charIndex < otfGlyphString.size; ++charIndex)
    otfGlyphString.glyphs[charIndex].c = string[charIndex];

  OTF_drive_tables (fontData->otfs[0],
                    &otfGlyphString,
                    NULL, NULL,
                    NULL,
                    "ligature,kerning");

  for (charIndex = 0; *string; ++string, ++charIndex)
    {
      FT_GlyphSlot glyph;
      FT_Face face;
      cairo_surface_t *textSurface;
      unsigned int i, j, stride;
      unsigned char *bytes;

      switch (otfGlyphString.glyphs[charIndex].positioning_type & 0xF)
        {
        case 1:
        case 2:

          pen.x += (otfGlyphString.glyphs[charIndex].f.f1.value->XAdvance << 12) / fontData->faces[0]->units_per_EM;

          break;
        }

      pen.x = (pen.x + 31) & ~63;

      if (!(glyph = font_FreeTypeGlyphForCharacter (fontData, *string, &previous, &face, &pen, 0)))
        errx (EXIT_FAILURE, "Failed to get glyph for character %d", *string);

      pen.x += glyph->advance.x;
      pen.y += glyph->advance.y;

      stride = cairo_format_stride_for_width (CAIRO_FORMAT_A8, glyph->bitmap.width);
      bytes = calloc (glyph->bitmap.rows, stride);

      for (i = 0; i < glyph->bitmap.rows; ++i)
        {
          for (j = 0; j < glyph->bitmap.width; ++j)
            bytes[i * stride + j] = pow (glyph->bitmap.buffer[i * glyph->bitmap.pitch + j] / 255.0, gamma) * 255;
        }


      textSurface = cairo_image_surface_create_for_data (bytes,
                                                         CAIRO_FORMAT_A8,
                                                         glyph->bitmap.width, glyph->bitmap.rows,
                                                         stride);

      cairo_mask_surface (cairo, textSurface,
                          glyph->bitmap_left, y - glyph->bitmap_top);
    }
}
