#include "font.h"

void
pango_render_string (cairo_t *cairo,
                     int x, int y,
                     const char *font,
                     unsigned int size,
                     const char *string)
{
  PangoLayout *layout;
  PangoFontDescription *fontDescription;

  layout = pango_cairo_create_layout (cairo);

  cairo_move_to (cairo, x, y);

  fontDescription = pango_font_description_new ();
  pango_font_description_set_family (fontDescription, font);
  pango_font_description_set_size (fontDescription, size * PANGO_SCALE);
  pango_layout_set_font_description (layout, fontDescription);
  pango_font_description_free (fontDescription);

  pango_layout_set_text (layout, string, -1);

  pango_cairo_update_layout (cairo, layout);
  pango_cairo_show_layout (cairo, layout);

  g_object_unref (layout);
}
