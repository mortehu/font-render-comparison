#include <stdio.h>
#include <stdlib.h>

#include "font.h"

const char *testString = "db qp - The quick brown fox jumped over the lazy dog. Sniffle AWAY";

int
main (int argc, char **argv)
{
  cairo_t *cairo;
  cairo_surface_t *surface;

  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 600, 100);
  cairo = cairo_create (surface);

  cairo_set_source_rgba (cairo, 1.0f, 1.0f, 1.0f, 1.0f);
  cairo_rectangle (cairo, 0, 0, 600, 100);
  cairo_fill (cairo);

  cairo_set_source_rgba (cairo, 0.0f, 0.0f, 0.0f, 1.0f);

  pango_render_string (cairo, "Source Sans Pro", 10, testString);

  freetype_render_string (cairo, 0, 30, "Source Sans Pro", 13, testString, 1.0);

  cairo_surface_write_to_png (surface, "output.png");

  return EXIT_SUCCESS;
}
