/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * This file is derived from the copy distributed with FreeBSD
 * (http://svnweb.freebsd.org/base/head/sys/cddl/contrib/opensolaris/uts/common/fs/zfs/lzjb.c,
 * last synced with revision 256132).  Some modifications have been
 * made:
 *
 * - Functionality
 *   - Added header file
 *   - Added LZJB_MAX_COMPRESSED_SIZE macro
 *   - lzjb_decompress will now attempt to consume all input instead
 *     of fill all output.  Furthermore, instead of returning 0 on
 *     success and -1 on failure, it will now return 0 on failure and
 *     the number of bytes successfully decompressed on success.
 * - Portability
 *   - Changed included headers
 *   - s/uchar_t/uint8_t/
 *   - #define NBBY 8
 * - Performance
 *   - Add const keywords
 *   - Add restrict keyword to some arguments when using C99 or later
 *   - Add lots of LZJB_LIKELY/UNLIKELY information (based on frequency
 *     when running against the Canterbury Corpus).
 *
 * These changes are largely to help facilitate integration into the
 * Squash compression abstraction library
 * (http://quixdb.github.io/squash/), however, they should be useful
 * for other software as well.
 */

#include "lzjb.h"

/* For memcmp */
#include <string.h>

#define	MATCH_BITS	6
#define	MATCH_MIN	3
#define	MATCH_MAX	((1 << MATCH_BITS) + (MATCH_MIN - 1))
#define	OFFSET_MASK	((1 << (16 - MATCH_BITS)) - 1)
#define	LEMPEL_SIZE	1024
#ifndef NBBY
#define NBBY 8
#endif

#ifdef __GNUC__
#  define LZJB_LIKELY(expr) __builtin_expect(!!(expr), 1)
#  define LZJB_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
#  define LZJB_LIKELY(expr) (expr)
#  define LZJB_UNLIKELY(expr) (expr)
#endif

#ifndef LZJB_RESTRICT
	#if __STDC_VERSION__ >= 199901L
		#define LZJB_RESTRICT restrict
	#else
		#define LZJB_RESTRICT
	#endif
#endif

size_t
lzjb_compress(const uint8_t* LZJB_RESTRICT src, uint8_t* LZJB_RESTRICT dst, size_t s_len, size_t d_len)
{
	const uint8_t* s_start = src;
	const uint8_t* d_start = dst;
	const uint8_t *cpy;
	uint8_t *copymap = NULL;
	int copymask = 1 << (NBBY - 1);
	int mlen, offset, hash;
	uint16_t *hp;
	uint16_t lempel[LEMPEL_SIZE] = { 0 };

	while (LZJB_LIKELY(src < s_start + s_len)) {
		if ((copymask <<= 1) == (1 << NBBY)) {
			if (dst >= d_start + d_len - 1 - 2 * NBBY)
				return (s_len);
			copymask = 1;
			copymap = dst;
			*dst++ = 0;
		}
		if (LZJB_UNLIKELY(src > s_start + s_len - MATCH_MAX)) {
			*dst++ = *src++;
			continue;
		}
		hash = (src[0] << 16) + (src[1] << 8) + src[2];
		hash += hash >> 9;
		hash += hash >> 5;
		hp = &lempel[hash & (LEMPEL_SIZE - 1)];
		offset = (intptr_t)(src - *hp) & OFFSET_MASK;
		*hp = (uint16_t)(uintptr_t)src;
		cpy = src - offset;
		if (LZJB_LIKELY(cpy >= s_start) && /* Is there a situation where this isn't true? */
				memcmp(src, cpy, sizeof(uint16_t)) == 0 &&
				LZJB_LIKELY(src[2] == cpy[2]) &&
				LZJB_LIKELY(cpy != src)) {
			*copymap |= copymask;
			for (mlen = MATCH_MIN; LZJB_LIKELY(mlen < MATCH_MAX); mlen++)
				if (src[mlen] != cpy[mlen])
					break;
			*dst++ = ((mlen - MATCH_MIN) << (NBBY - MATCH_BITS)) |
			    (offset >> NBBY);
			*dst++ = (uint8_t)offset;
			src += mlen;
		} else {
			*dst++ = *src++;
		}
	}
	return (dst - d_start);
}

#include <stdio.h>
#include <assert.h>
LZJBResult
lzjb_decompress(const uint8_t* LZJB_RESTRICT src, uint8_t* LZJB_RESTRICT dst, size_t s_len, size_t* d_len)
{
	const uint8_t* s_start = src;
	const uint8_t* d_start = dst;
	const uint8_t *d_end = (uint8_t *)d_start + *d_len;
	const uint8_t *s_end = (uint8_t *)s_start + s_len;
	uint8_t *cpy;
	uint8_t copymap = 0;
	int copymask = 1 << (NBBY - 1);

	while (LZJB_LIKELY(src < s_end)) {
		if (dst >= d_end)
			return LZJB_WOULD_OVERFLOW;

		if ((copymask <<= 1) == (1 << NBBY)) {
			copymask = 1;
			copymap = *src++;
		}
		if (copymap & copymask) {
			int mlen = (src[0] >> (NBBY - MATCH_BITS)) + MATCH_MIN;
			int offset = ((src[0] << NBBY) | src[1]) & OFFSET_MASK;
			src += 2;
			if (LZJB_UNLIKELY((cpy = dst - offset) < (uint8_t *)d_start)) {
				return LZJB_BAD_DATA;
			}
			if (LZJB_UNLIKELY(mlen > (d_end - dst)))
				return LZJB_WOULD_OVERFLOW;
			while (--mlen >= 0)
				*dst++ = *cpy++;
		} else {
			*dst++ = *src++;
		}
	}
	*d_len = (size_t) (dst - d_start);
	return LZJB_OK;
}
