/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CAIRO_MALLOC_PRIVATE_H
#define CAIRO_MALLOC_PRIVATE_H

#include "cairo-wideint-private.h"

#if HAVE_MEMFAULT
#include <memfault.h>
#define CAIRO_INJECT_FAULT() MEMFAULT_INJECT_FAULT()
#else
#define CAIRO_INJECT_FAULT() 0
#endif

/**
 * _cairo_malloc:
 * @size: size in bytes
 *
 * Allocate @size memory using malloc().
 * The memory should be freed using free().
 * malloc is skipped, if 0 bytes are requested, and %NULL will be returned.
 *
 * Return value: A pointer to the newly allocated memory, or %NULL in
 * case of malloc() failure or size is 0.
 */

#define _cairo_malloc(size) \
   ((size) ? malloc((unsigned) (size)) : NULL)

/**
 * _cairo_malloc_ab:
 * @n: number of elements to allocate
 * @size: size of each element
 *
 * Allocates @n*@size memory using _cairo_malloc(), taking care to not
 * overflow when doing the multiplication.  Behaves much like
 * calloc(), except that the returned memory is not set to zero.
 * The memory should be freed using free().
 *
 * @size should be a constant so that the compiler can optimize
 * out a constant division.
 *
 * Return value: A pointer to the newly allocated memory, or %NULL in
 * case of malloc() failure or overflow.
 */

#define _cairo_malloc_ab(a, size) \
  ((size) && (unsigned) (a) >= INT32_MAX / (unsigned) (size) ? NULL : \
   _cairo_malloc((unsigned) (a) * (unsigned) (size)))

/**
 * _cairo_realloc_ab:
 * @ptr: original pointer to block of memory to be resized
 * @n: number of elements to allocate
 * @size: size of each element
 *
 * Reallocates @ptr a block of @n*@size memory using realloc(), taking
 * care to not overflow when doing the multiplication.  The memory
 * should be freed using free().
 *
 * @size should be a constant so that the compiler can optimize
 * out a constant division.
 *
 * Return value: A pointer to the newly allocated memory, or %NULL in
 * case of realloc() failure or overflow (whereupon the original block
 * of memory * is left untouched).
 */

#define _cairo_realloc_ab(ptr, a, size) \
  ((size) && (unsigned) (a) >= INT32_MAX / (unsigned) (size) ? NULL : \
   realloc(ptr, (unsigned) (a) * (unsigned) (size)))

/**
 * _cairo_malloc_abc:
 * @n: first factor of number of elements to allocate
 * @b: second factor of number of elements to allocate
 * @size: size of each element
 *
 * Allocates @n*@b*@size memory using _cairo_malloc(), taking care to not
 * overflow when doing the multiplication.  Behaves like
 * _cairo_malloc_ab().  The memory should be freed using free().
 *
 * @size should be a constant so that the compiler can optimize
 * out a constant division.
 *
 * Return value: A pointer to the newly allocated memory, or %NULL in
 * case of malloc() failure or overflow.
 */

#define _cairo_malloc_abc(a, b, size) \
  ((b) && (unsigned) (a) >= INT32_MAX / (unsigned) (b) ? NULL : \
   (size) && (unsigned) ((a)*(b)) >= INT32_MAX / (unsigned) (size) ? NULL : \
   _cairo_malloc((unsigned) (a) * (unsigned) (b) * (unsigned) (size)))

/**
 * _cairo_malloc_ab_plus_c:
 * @n: number of elements to allocate
 * @size: size of each element
 * @k: additional size to allocate
 *
 * Allocates @n*@ksize+@k memory using _cairo_malloc(), taking care to not
 * overflow when doing the arithmetic.  Behaves like
 * _cairo_malloc_ab().  The memory should be freed using free().
 *
 * Return value: A pointer to the newly allocated memory, or %NULL in
 * case of malloc() failure or overflow.
 */

#define _cairo_malloc_ab_plus_c(n, size, k) \
  ((size) && (unsigned) (n) >= INT32_MAX / (unsigned) (size) ? NULL : \
   (unsigned) (k) >= INT32_MAX - (unsigned) (n) * (unsigned) (size) ? NULL : \
   _cairo_malloc((unsigned) (n) * (unsigned) (size) + (unsigned) (k)))

#endif /* CAIRO_MALLOC_PRIVATE_H */
