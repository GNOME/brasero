/***************************************************************************
 *            burn-susp.c
 *
 *  Sun Nov 26 19:06:31 2006
 *  Copyright  2006  Rouquier Philippe
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <string.h>

#include <glib.h>

#include "burn-iso-field.h"
#include "burn-susp.h"

struct _BraseroSusp {
	gchar signature		[2];
	gchar len;
	gchar version;
	gchar data		[0];
};
typedef struct _BraseroSusp BraseroSusp;

struct _BraseroSuspSP {
	BraseroSusp susp	[1];
	gchar magic		[2];
	gchar skip;
};
typedef struct _BraseroSuspSP BraseroSuspSP;

struct _BraseroRockNM {
	BraseroSusp susp	[1];
	gchar flags;
	gchar name		[0];
};
typedef struct _BraseroRockNM BraseroRockNM;

struct _BraseroRockCL {
	BraseroSusp susp	[1];
	guchar location		[8];
};
typedef struct _BraseroRockCL BraseroRockCL;

struct _BraseroRockPL {
	BraseroSusp susp	[1];
	guchar location		[8];
};
typedef struct _BraseroRockPL BraseroRockPL;

typedef enum {
	BRASERO_NM_CONTINUE	= 1,
	BRASERO_NM_CURRENT	= 1 << 1,
	BRASERO_NM_PARENT	= 1 << 2,
	BRASERO_NM_NETWORK	= 1 << 5
} BraseroNMFlag;

void
brasero_susp_ctx_clean (BraseroSuspCtx *ctx)
{
	if (ctx->rr_name)
		g_free (ctx->rr_name);
}

static gboolean
brasero_susp_SP (BraseroSusp *susp,
		 BraseroSuspCtx *ctx)
{
	gchar magic [2] = { 0xBE, 0xEF };
	BraseroSuspSP *sp;

	sp = (BraseroSuspSP *) susp;

	if (memcmp (sp->magic, magic, 2))
		return FALSE;

	ctx->skip = sp->skip;
	ctx->has_SP = TRUE;
	return TRUE;
}

static gboolean
brasero_susp_NM (BraseroSusp *susp,
		 BraseroSuspCtx *ctx)
{
	gint len;
	gchar *rr_name;
	BraseroRockNM *nm;

	nm = (BraseroRockNM *) susp;
	if (nm->flags & (BRASERO_NM_CURRENT|BRASERO_NM_PARENT|BRASERO_NM_NETWORK))
		return TRUE;

	len = susp->len - sizeof (BraseroRockNM);

	if (!len)
		return TRUE;

	/* should we concatenate ? */
	if (ctx->rr_name
	&&  ctx->rr_name_continue)
		rr_name = g_strdup_printf ("%s%.*s",
					   ctx->rr_name,
					   len,
					   nm->name);
	else
		rr_name = g_strndup (nm->name, len);

	if (ctx->rr_name)
		g_free (ctx->rr_name);

	ctx->rr_name = rr_name;
	ctx->rr_name_continue = (nm->flags & BRASERO_NM_CONTINUE);

	return TRUE;
}

/**
 * All these entries are defined in rrip standards.
 * They are mainly used for directory/file relocation.
 */
static gboolean
brasero_susp_CL (BraseroSusp *susp,
		 BraseroSuspCtx *ctx)
{
	BraseroRockCL *cl;
	gint address;

	/* Child Link */
	cl = (BraseroRockCL *) susp;
	address = brasero_iso9660_get_733_val (cl->location);
	ctx->rr_children = g_slist_prepend (ctx->rr_children,
					    GINT_TO_POINTER (address));
	return TRUE;
}

static gboolean
brasero_susp_RE (BraseroSusp *susp,
		 BraseroSuspCtx *ctx)
{
	/* Nothing to see here really this is just an indication.
	 * Check consistency though BP3 = "4" and BP4 = "1" */
	if (susp->len != 4 || susp->version != 1)
		return FALSE;

	return TRUE;
}

static gboolean
brasero_susp_PL (BraseroSusp *susp,
		 BraseroSuspCtx *ctx)
{
	BraseroRockPL *pl;

	if (ctx->rr_parent)
		return FALSE;

	/* Parent Link */
	pl = (BraseroRockPL *) susp;
	ctx->rr_parent = brasero_iso9660_get_733_val (pl->location);
	return TRUE;
}

gboolean
brasero_susp_read (BraseroSuspCtx *ctx, gchar *buffer, gint max)
{
	BraseroSusp *susp;
	gint offset;

	memset (ctx, 0, sizeof (BraseroSuspCtx));

	if (max <= 0)
		return TRUE;

	if (!buffer)
		return FALSE;

	susp = (BraseroSusp *) buffer;
	if (susp->len > max)
		goto error;

	offset = 0;

	/* we are only interested in some of the Rock Ridge entries:
	 * name, relocation for directories */
	while (susp->len) {
		gboolean result;

		result = TRUE;
		if (!memcmp (susp->signature, "SP", 2))
			result = brasero_susp_SP (susp, ctx);
		if (!memcmp (susp->signature, "NM", 2))
			result = brasero_susp_NM (susp, ctx);
		else if (!memcmp (susp->signature, "CL", 2))
			result = brasero_susp_CL (susp, ctx);
		else if (!memcmp (susp->signature, "PL", 2))
			result = brasero_susp_PL (susp, ctx);

		/* This is to indicate that the entry has been relocated */
		else if (!memcmp (susp->signature, "RE", 2))
			result = brasero_susp_RE (susp, ctx);

		if (!result)
			goto error;

		offset += susp->len;
		/* there may be one byte for padding to an even number */
		if (offset == max || offset + 1 == max)
			break;

		if (offset > max)
			goto error;

		susp = (BraseroSusp *) (buffer + offset);

		/* some coherency checks */
		if (offset + susp->len > max)
			goto error;
	}

	return TRUE;

error:

	/* clean up context */
	brasero_susp_ctx_clean (ctx);
	return FALSE;
}
