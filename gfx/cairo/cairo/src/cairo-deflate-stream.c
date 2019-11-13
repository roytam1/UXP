/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cairoint.h"
#include "cairo-error-private.h"
#include "cairo-output-stream-private.h"
#include <zlib.h>

#define BUFFER_SIZE 16384

typedef struct _cairo_deflate_stream {
    cairo_output_stream_t  base;
    cairo_output_stream_t *output;
    z_stream               zlib_stream;
    unsigned char          input_buf[BUFFER_SIZE];
    unsigned char          output_buf[BUFFER_SIZE];
} cairo_deflate_stream_t;

static void
cairo_deflate_stream_deflate (cairo_deflate_stream_t *stream, cairo_bool_t flush)
{
    int ret;
    cairo_bool_t finished;

    do {
        ret = deflate (&stream->zlib_stream, flush ? Z_FINISH : Z_NO_FLUSH);
        if (flush || stream->zlib_stream.avail_out == 0)
        {
            _cairo_output_stream_write (stream->output,
                                        stream->output_buf,
                                        BUFFER_SIZE - stream->zlib_stream.avail_out);
            stream->zlib_stream.next_out = stream->output_buf;
            stream->zlib_stream.avail_out = BUFFER_SIZE;
        }

        finished = TRUE;
        if (stream->zlib_stream.avail_in != 0)
            finished = FALSE;
        if (flush && ret != Z_STREAM_END)
            finished = FALSE;

    } while (!finished);

    stream->zlib_stream.next_in = stream->input_buf;
}

static cairo_status_t
_cairo_deflate_stream_write (cairo_output_stream_t *base,
                             const unsigned char   *data,
                             unsigned int	    length)
{
    cairo_deflate_stream_t *stream = (cairo_deflate_stream_t *) base;
    unsigned int count;
    const unsigned char *p = data;

    while (length) {
        count = length;
        if (count > BUFFER_SIZE - stream->zlib_stream.avail_in)
            count = BUFFER_SIZE - stream->zlib_stream.avail_in;
        memcpy (stream->input_buf + stream->zlib_stream.avail_in, p, count);
        p += count;
        stream->zlib_stream.avail_in += count;
        length -= count;

        if (stream->zlib_stream.avail_in == BUFFER_SIZE)
            cairo_deflate_stream_deflate (stream, FALSE);
    }

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_status_t
_cairo_deflate_stream_close (cairo_output_stream_t *base)
{
    cairo_deflate_stream_t *stream = (cairo_deflate_stream_t *) base;

    cairo_deflate_stream_deflate (stream, TRUE);
    deflateEnd (&stream->zlib_stream);

    return _cairo_output_stream_get_status (stream->output);
}

cairo_output_stream_t *
_cairo_deflate_stream_create (cairo_output_stream_t *output)
{
    cairo_deflate_stream_t *stream;

    if (output->status)
	return _cairo_output_stream_create_in_error (output->status);

    stream = malloc (sizeof (cairo_deflate_stream_t));
    if (unlikely (stream == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_output_stream_t *) &_cairo_output_stream_nil;
    }

    _cairo_output_stream_init (&stream->base,
			       _cairo_deflate_stream_write,
			       NULL,
			       _cairo_deflate_stream_close);
    stream->output = output;

    stream->zlib_stream.zalloc = Z_NULL;
    stream->zlib_stream.zfree  = Z_NULL;
    stream->zlib_stream.opaque  = Z_NULL;

    if (deflateInit (&stream->zlib_stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
	free (stream);
	return (cairo_output_stream_t *) &_cairo_output_stream_nil;
    }

    stream->zlib_stream.next_in = stream->input_buf;
    stream->zlib_stream.avail_in = 0;
    stream->zlib_stream.next_out = stream->output_buf;
    stream->zlib_stream.avail_out = BUFFER_SIZE;

    return &stream->base;
}
