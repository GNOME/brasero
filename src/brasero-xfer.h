/***************************************************************************
 *            burn-xfer.h
 *
 *  Sun Sep 10 09:08:59 2006
 *  Copyright  2006  philippe
 *  <philippe@Rouquier Philippe.localdomain>
 ****************************************************************************/

/*
 * Libbrasero-media is free software; you can redistribute it and/or modify
fy
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
#include <gio/gio.h>

#include "burn-basics.h"

#ifndef _BURN_XFER_H
#define _BURN_XFER_H

G_BEGIN_DECLS

typedef struct _BraseroXferCtx BraseroXferCtx;

BraseroXferCtx *
brasero_xfer_new (void);

void
brasero_xfer_free (BraseroXferCtx *ctx);

BraseroBurnResult
brasero_xfer (BraseroXferCtx *ctx,
	      const gchar *src,
	      const gchar *dest,
	      GError **error);

BraseroBurnResult
brasero_xfer_cancel (BraseroXferCtx *ctx);

BraseroBurnResult
brasero_xfer_get_progress (BraseroXferCtx *ctx,
			   gint64 *written,
			   gint64 *total);

G_END_DECLS

#endif /* _BURN_XFER_H */

 
