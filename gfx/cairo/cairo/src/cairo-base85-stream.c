/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cairoint.h"
#include "cairo-error-private.h"
#include "cairo-output-stream-private.h"

typedef struct _cairo_base85_stream {
    cairo_output_stream_t base;
    cairo_output_stream_t *output;
    unsigned char four_tuple[4];
    int pending;
} cairo_base85_stream_t;

static void
_expand_four_tuple_to_five (unsigned char four_tuple[4],
			    unsigned char five_tuple[5],
			    cairo_bool_t *all_zero)
{
    uint32_t value;
    int digit, i;

    value = four_tuple[0] << 24 | four_tuple[1] << 16 | four_tuple[2] << 8 | four_tuple[3];
    if (all_zero)
	*all_zero = TRUE;
    for (i = 0; i < 5; i++) {
	digit = value % 85;
	if (digit != 0 && all_zero)
	    *all_zero = FALSE;
	five_tuple[4-i] = digit + 33;
	value = value / 85;
    }
}

static cairo_status_t
_cairo_base85_stream_write (cairo_output_stream_t *base,
			    const unsigned char	  *data,
			    unsigned int	   length)
{
    cairo_base85_stream_t *stream = (cairo_base85_stream_t *) base;
    const unsigned char *ptr = data;
    unsigned char five_tuple[5];
    cairo_bool_t is_zero;

    while (length) {
	stream->four_tuple[stream->pending++] = *ptr++;
	length--;
	if (stream->pending == 4) {
	    _expand_four_tuple_to_five (stream->four_tuple, five_tuple, &is_zero);
	    if (is_zero)
		_cairo_output_stream_write (stream->output, "z", 1);
	    else
		_cairo_output_stream_write (stream->output, five_tuple, 5);
	    stream->pending = 0;
	}
    }

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_status_t
_cairo_base85_stream_close (cairo_output_stream_t *base)
{
    cairo_base85_stream_t *stream = (cairo_base85_stream_t *) base;
    unsigned char five_tuple[5];

    if (stream->pending) {
	memset (stream->four_tuple + stream->pending, 0, 4 - stream->pending);
	_expand_four_tuple_to_five (stream->four_tuple, five_tuple, NULL);
	_cairo_output_stream_write (stream->output, five_tuple, stream->pending + 1);
    }

    return _cairo_output_stream_get_status (stream->output);
}

cairo_output_stream_t *
_cairo_base85_stream_create (cairo_output_stream_t *output)
{
    cairo_base85_stream_t *stream;

    if (output->status)
	return _cairo_output_stream_create_in_error (output->status);

    stream = malloc (sizeof (cairo_base85_stream_t));
    if (unlikely (stream == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_output_stream_t *) &_cairo_output_stream_nil;
    }

    _cairo_output_stream_init (&stream->base,
			       _cairo_base85_stream_write,
			       NULL,
			       _cairo_base85_stream_close);
    stream->output = output;
    stream->pending = 0;

    return &stream->base;
}
