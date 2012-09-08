#ifndef FONT_H_
#define FONT_H_ 1

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

void
pango_render_string (cairo_t *cairo,
                     int x, int y,
                     const char *font,
                     unsigned int size,
                     const char *string);

void
freetype_render_string (cairo_t *cairo,
                        int x, int y,
                        const char *font,
                        unsigned int size,
                        const char *string,
                        float gamma);

#endif /* !FONT_H_ */
