/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_SVG_SURFACE_PRIVATE_H
#define CAIRO_SVG_SURFACE_PRIVATE_H

#include "cairo-svg.h"

#include "cairo-surface-private.h"
#include "cairo-surface-clipper-private.h"

typedef struct cairo_svg_document cairo_svg_document_t;

typedef struct cairo_svg_surface {
    cairo_surface_t base;

    cairo_content_t content;

    double width;
    double height;

    cairo_svg_document_t *document;

    cairo_output_stream_t *xml_node;
    cairo_array_t	   page_set;

    cairo_surface_clipper_t clipper;
    unsigned int clip_level;
    unsigned int base_clip;
    cairo_bool_t is_base_clip_emitted;

    cairo_paginated_mode_t paginated_mode;

    cairo_bool_t force_fallbacks;
} cairo_svg_surface_t;

#endif /* CAIRO_SVG_SURFACE_PRIVATE_H */
