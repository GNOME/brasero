/***************************************************************************
 *            burn-susp.h
 *
 *  Sun Nov 26 19:20:31 2006
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

#include <glib.h>

#ifndef _BURN_SUSP_H
#define _BURN_SUSP_H

G_BEGIN_DECLS

struct _BraseroSuspCtx {
	gchar *rr_name;

	gboolean has_SP;

	GSList *rr_children;
	gint rr_parent;

	guchar skip;

	gboolean rr_name_continue;
};
typedef struct _BraseroSuspCtx BraseroSuspCtx;

void
brasero_susp_ctx_clean (BraseroSuspCtx *ctx);

gboolean
brasero_susp_read (BraseroSuspCtx *ctx, gchar *buffer, gint max);

G_END_DECLS

#endif /* _BURN_SUSP_H */

 
