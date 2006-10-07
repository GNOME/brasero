/***************************************************************************
 *            brasero-md5.c
 *
 *  Sat Sep  9 17:16:16 2006
 *  Copyright  2006  philippe
 *  <philippe@algernon.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-md5.h"

struct _BraseroMD5Ctx {
	guint32 size [2];
	gint64 written_b;
	gboolean cancel;
};

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define SWAP(num)	GUINT32_SWAP_LE_BE (num)
#else
#define SWAP(num)	num
#endif

#define F(X,Y,Z)			(((X) & (Y)) | ((~X) & (Z)))
#define G(X,Y,Z)			(((X) & (Z)) | ((Y) & (~Z)))
#define H(X,Y,Z)			((X) ^ (Y) ^ (Z))
#define I(X,Y,Z)			((Y) ^ ((X) | (~Z)))

#define ROTATE(num, s)			((num << s) | (num >> (32 - s)))

#define STEP1(A, B, C, D, k, s, i)			\
{							\
	A += F (B, C, D) + SWAP (((guint32*) buffer) [k]) + i;	\
	A = ROTATE (A, s);				\
	A += B;						\
}
#define STEP2(A, B, C, D, k, s, i)			\
{							\
	A += G (B, C, D) + SWAP (((guint32*) buffer) [k]) + i;	\
	A = ROTATE (A, s);				\
	A += B;						\
}
#define STEP3(A, B, C, D, k, s, i)			\
{							\
	A += H (B, C, D) + SWAP (((guint32*) buffer) [k]) + i;	\
	A = ROTATE (A, s);				\
	A += B;						\
}
#define STEP4(A, B, C, D, k, s, i)			\
{							\
	A += I (B, C, D) + SWAP (((guint32*) buffer) [k]) + i;	\
	A = ROTATE (A, s);				\
	A += B;						\
}

#define NB_WORDS			16
#define BLOCK_WORDS_NUM			4
#define BLOCK_SIZE			64

static void
brasero_burn_sum_process_block_md5 (BraseroMD5 *md5,
				    guchar *buffer)
{
	guint32 A, B, C, D;

	A = md5->A;
	B = md5->B;
	C = md5->C;
	D = md5->D;

	/* step 1 */
	STEP1 (A, B, C, D, 0, 7, 0xd76aa478);
	STEP1 (D, A, B, C, 1, 12, 0xe8c7b756);
	STEP1 (C, D, A, B, 2, 17, 0x242070db);
	STEP1 (B, C, D, A, 3, 22, 0xc1bdceee);
	STEP1 (A, B, C, D, 4, 7, 0xf57c0faf);
	STEP1 (D, A, B, C, 5, 12, 0x4787c62a);
	STEP1 (C, D, A, B, 6, 17, 0xa8304613);
	STEP1 (B, C, D, A, 7, 22, 0xfd469501);
	STEP1 (A, B, C, D, 8, 7, 0x698098d8);
	STEP1 (D, A, B, C, 9, 12, 0x8b44f7af);
	STEP1 (C, D, A, B, 10, 17, 0xffff5bb1);
	STEP1 (B, C, D, A, 11, 22, 0x895cd7be);
	STEP1 (A, B, C, D, 12, 7, 0x6b901122);
	STEP1 (D, A, B, C, 13, 12, 0xfd987193);
	STEP1 (C, D, A, B, 14, 17, 0xa679438e);
	STEP1 (B, C, D, A, 15, 22, 0x49b40821);

	/* step 2 */
	STEP2 (A, B, C, D, 1, 5, 0xf61e2562);
	STEP2 (D, A, B, C, 6, 9, 0xc040b340);
	STEP2 (C, D, A, B, 11, 14, 0x265e5a51);
	STEP2 (B, C, D, A, 0, 20, 0xe9b6c7aa);
	STEP2 (A, B, C, D, 5, 5, 0xd62f105d);
	STEP2 (D, A, B, C, 10, 9, 0x02441453);
	STEP2 (C, D, A, B, 15, 14, 0xd8a1e681);
	STEP2 (B, C, D, A, 4, 20, 0xe7d3fbc8);
	STEP2 (A, B, C, D, 9, 5, 0x21e1cde6);
	STEP2 (D, A, B, C, 14, 9, 0xc33707d6);
	STEP2 (C, D, A, B, 3, 14, 0xf4d50d87);
	STEP2 (B, C, D, A, 8, 20, 0x455a14ed);
	STEP2 (A, B, C, D, 13, 5, 0xa9e3e905);
	STEP2 (D, A, B, C, 2, 9, 0xfcefa3f8);
	STEP2 (C, D, A, B, 7, 14, 0x676f02d9);
	STEP2 (B, C, D, A, 12, 20, 0x8d2a4c8a);

	/* step 3 */
	STEP3 (A, B, C, D, 5, 4, 0xfffa3942);
	STEP3 (D, A, B, C, 8, 11, 0x8771f681);
	STEP3 (C, D, A, B, 11, 16, 0x6d9d6122);
	STEP3 (B, C, D, A, 14, 23, 0xfde5380c);
	STEP3 (A, B, C, D, 1, 4, 0xa4beea44);
	STEP3 (D, A, B, C, 4, 11, 0x4bdecfa9);
	STEP3 (C, D, A, B, 7, 16, 0xf6bb4b60);
	STEP3 (B, C, D, A, 10, 23, 0xbebfbc70);
	STEP3 (A, B, C, D, 13, 4, 0x289b7ec6);
	STEP3 (D, A, B, C, 0, 11, 0xeaa127fa);
	STEP3 (C, D, A, B, 3, 16, 0xd4ef3085);
	STEP3 (B, C, D, A, 6, 23, 0x04881d05);
	STEP3 (A, B, C, D, 9, 4, 0xd9d4d039);
	STEP3 (D, A, B, C, 12, 11, 0xe6db99e5);
	STEP3 (C, D, A, B, 15, 16, 0x1fa27cf8);
	STEP3 (B, C, D, A, 2, 23, 0xc4ac5665);

	/* step 4 */
	STEP4 (A, B, C, D, 0, 6, 0xf4292244);
	STEP4 (D, A, B, C, 7, 10, 0x432aff97);
	STEP4 (C, D, A, B, 14, 15, 0xab9423a7);
	STEP4 (B, C, D, A, 5, 21, 0xfc93a039);
	STEP4 (A, B, C, D, 12, 6, 0x655b59c3);
	STEP4 (D, A, B, C, 3, 10, 0x8f0ccc92);
	STEP4 (C, D, A, B, 10, 15, 0xffeff47d);
	STEP4 (B, C, D, A, 1, 21, 0x85845dd1);
	STEP4 (A, B, C, D, 8, 6, 0x6fa87e4f);
	STEP4 (D, A, B, C, 15, 10, 0xfe2ce6e0);
	STEP4 (C, D, A, B, 6, 15, 0xa3014314);
	STEP4 (B, C, D, A, 13, 21, 0x4e0811a1);
	STEP4 (A, B, C, D, 4, 6, 0xf7537e82);
	STEP4 (D, A, B, C, 11, 10, 0xbd3af235);
	STEP4 (C, D, A, B, 2, 15, 0x2ad7d2bb);
	STEP4 (B, C, D, A, 9, 21, 0xeb86d391);

	md5->A += A;
	md5->B += B;
	md5->C += C;
	md5->D += D;
}

BraseroBurnResult
brasero_md5_sum (BraseroMD5Ctx *ctx,
		 const gchar *path,
		 BraseroMD5 *md5,
		 gint64 limit,
		 GError **error)
{
	FILE *file;
	gint read_bytes = 0;
	guchar buffer [BLOCK_SIZE];
	
	md5->A = 0x67452301;
	md5->B = 0xefcdab89;
	md5->C = 0x98badcfe;
	md5->D = 0x10325476;
	ctx->size [0] = 0;
	ctx->size [1] = 0;

	file = fopen (path, "r");
	if (!file) {
		gchar *name;

		if (errno == ENOENT)
			return BRASERO_BURN_RETRY;

		name = g_path_get_basename (path);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the file %s couldn't be read (%s)"),
			     name,
			     strerror (errno));
		g_free (name);

		return BRASERO_BURN_ERR;
	}

	while (limit < 0 || limit >= BLOCK_SIZE) {
		if (ctx->cancel) {
			fclose (file);
			return BRASERO_BURN_CANCEL;
		}

		read_bytes = fread (buffer, 1, BLOCK_SIZE, file);
		ctx->written_b += read_bytes;
		limit -= read_bytes;

		ctx->size [0] += read_bytes;
		if (ctx->size [0] < read_bytes)
			ctx->size [1] ++;

		if (read_bytes != BLOCK_SIZE) {
			/* that's either the end or an error */
			if (feof (file))
				break;

			goto error;
		}

		brasero_burn_sum_process_block_md5 (md5, buffer);
	}

	if (limit > 0 && !feof (file)) {
		read_bytes = fread (buffer, 1, limit, file);
		ctx->written_b += read_bytes;

		ctx->size [0] += read_bytes;
		if (ctx->size [0] < read_bytes)
			ctx->size [1] ++;

		if (limit != read_bytes) {
			if (!feof (file))
				goto error;
		}
	}

	fclose (file);

	/* process the remaining bytes, pad them and add the size */
	bzero (buffer + read_bytes, BLOCK_SIZE - read_bytes);
	*(buffer + read_bytes) = 0x80;

	if (read_bytes >= 56) {
		brasero_burn_sum_process_block_md5 (md5, buffer);
		bzero (buffer, 56);
	}

	*((guint32 *) (buffer + 56)) = SWAP (ctx->size [0] << 3);
	*((guint32 *) (buffer + 60)) = SWAP (ctx->size [1] << 3 | ctx->size [0] >> 29);

	brasero_burn_sum_process_block_md5 (md5, buffer);

	return BRASERO_BURN_OK;

error:
	{
	gchar *name;

	fclose (file);

	name = g_path_get_basename (path);
	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     _("the file %s couldn't be read (%s)"),
		     name,
		     strerror (errno));
	g_free (name);

	return BRASERO_BURN_ERR;
	}
}

void
brasero_md5_string (BraseroMD5 *md5, gchar *string)
{
	gint i, j;

	/* write to the string */
	for (i = 0; i < 4; i ++) {
		guchar *number = NULL;

		switch (i) {
		case 0:
			number = (guchar*) &md5->A;
			break;
		case 1:
			number = (guchar*) &md5->B;
			break;
		case 2:
			number = (guchar*) &md5->C;
			break;
		case 3:
			number = (guchar*) &md5->D;
			break;
		default:
			break;
		}
			
		for (j = 0; j < 4; j ++) {
			sprintf (string, "%02x", number [j]);
			string += 2;
		}
	}
}

BraseroBurnResult
brasero_md5_sum_to_string (BraseroMD5Ctx *ctx,
			   const gchar *path,
			   gchar *string,
			   GError **error)
{
	BraseroMD5 md5;
	BraseroBurnResult result;

	if (!string)
		return BRASERO_BURN_ERR;

	result = brasero_md5_sum (ctx,
				  path,
				  &md5,
				  -1,
				  error);

	if (result != BRASERO_BURN_OK)
		return result;

	brasero_md5_string (&md5, string);

	return BRASERO_BURN_OK;
}

BraseroMD5Ctx *
brasero_md5_new (void)
{
	BraseroMD5Ctx *ctx;

	ctx = g_new0 (BraseroMD5Ctx, 1);

	return (BraseroMD5Ctx *) ctx;
}

void
brasero_md5_free (BraseroMD5Ctx *ctx)
{
	g_free (ctx);
}

void
brasero_md5_cancel (BraseroMD5Ctx *ctx)
{
	ctx->cancel = TRUE;
}

gint64
brasero_md5_get_written (BraseroMD5Ctx *ctx)
{
	return ctx->written_b;
}

void
brasero_md5_reset (BraseroMD5Ctx *ctx)
{
	ctx->cancel = FALSE;
}

gboolean
brasero_md5_equal (BraseroMD5 *a, BraseroMD5 *b)
{
	if (a->A != b->A)
		return FALSE;
	if (a->B != b->B)
		return FALSE;
	if (a->C != b->C)
		return FALSE;
	if (a->D != b->D)
		return FALSE;

	return TRUE;
}
