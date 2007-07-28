/***************************************************************************
 *            brasero-md5.h
 *
 *  Sat Sep  9 17:16:43 2006
 *  Copyright  2006  philippe
 *  <philippe@Rouquier Philippe.localdomain>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>

#include <glib.h>

#include "burn-basics.h"

#ifndef _BRASERO_MD5_H
#define _BRASERO_MD5_H

#ifdef __cplusplus
extern "C"
{
#endif

#define MD5_STRING_LEN			32
#define BLOCK_SIZE			64

struct _BraseroMD5 {
	guint32 A;
	guint32 B;
	guint32 C;
	guint32 D;
};
typedef struct _BraseroMD5 BraseroMD5;

typedef struct _BraseroMD5Ctx BraseroMD5Ctx;

BraseroMD5Ctx *
brasero_md5_new (void);

void
brasero_md5_free (BraseroMD5Ctx *ctx);

/**
 * the three following functions are useful to do it live
 */

BraseroBurnResult
brasero_md5_init (BraseroMD5Ctx *ctx,
		  BraseroMD5 *md5);

guint
brasero_md5_sum (BraseroMD5Ctx *ctx,
		 BraseroMD5 *md5,
		 guchar *buffer,
		 guint bytes);

BraseroBurnResult
brasero_md5_end (BraseroMD5Ctx *ctx,
		 BraseroMD5 *md5,
		 guchar *buffer,
		 guint bytes);

BraseroBurnResult
brasero_md5_file (BraseroMD5Ctx *ctx,
		  const gchar *path,
		  BraseroMD5 *md5,
		  gint64 limit,
		  GError **error);

BraseroBurnResult
brasero_md5_file_to_string (BraseroMD5Ctx *ctx,
			    const gchar *path,
			    gchar *string,
			    gint64 limit,
			    GError **error);

void
brasero_md5_cancel (BraseroMD5Ctx *ctx);

void
brasero_md5_reset (BraseroMD5Ctx *ctx);

gint64
brasero_md5_get_written (BraseroMD5Ctx *ctx);

gboolean
brasero_md5_equal (BraseroMD5 *a, BraseroMD5 *b);

void
brasero_md5_string (BraseroMD5 *md5, gchar *string);

#ifdef __cplusplus
}
#endif

#endif /* _BRASERO_MD5_H */

 
