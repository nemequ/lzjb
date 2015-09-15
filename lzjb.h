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

#ifndef _LZJB_H_
#define _LZJB_H_

#ifndef LZJB_RESTRICT
	#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
		#define LZJB_RESTRICT restrict
	#else
		#define LZJB_RESTRICT
	#endif
#endif

#include <stddef.h>
#include <stdint.h>

/* Determined based on testing with random data as ((s_len * 1.125) +
 * (2 * NBBY)).  If you are aware of a situation where this is
 * insufficient please let me know. */
#define LZJB_MAX_COMPRESSED_SIZE(s_len) \
	(s_len +															\
	 (s_len / 8) +												\
	 (((s_len % 8) != 0) ? 1 : 0) +				\
	 (2 * 8))

typedef enum {
	LZJB_OK,
	LZJB_BAD_DATA,
	LZJB_WOULD_OVERFLOW
} LZJBResult;

size_t lzjb_compress(const uint8_t* LZJB_RESTRICT src, uint8_t* LZJB_RESTRICT dst, size_t s_len, size_t d_len);
LZJBResult lzjb_decompress(const uint8_t* LZJB_RESTRICT src, uint8_t* LZJB_RESTRICT dst, size_t s_len, size_t* d_len);

#endif /* _LZJB_H_ */
