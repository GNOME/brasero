/***************************************************************************
 *            burn-xfer.h
 *
 *  Sun Sep 10 09:08:59 2006
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

#include <glib.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"

#ifndef _BURN_XFER_H
#define _BURN_XFER_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _BraseroXferCtx BraseroXferCtx;

BraseroXferCtx *
brasero_xfer_new (void);

void
brasero_xfer_free (BraseroXferCtx *ctx);

BraseroBurnResult
brasero_xfer (BraseroXferCtx *ctx,
	      GnomeVFSURI *uri,
	      GnomeVFSURI *dest,
	      GError **error);

BraseroBurnResult
brasero_xfer_cancel (BraseroXferCtx *ctx);

BraseroBurnResult
brasero_xfer_get_progress (BraseroXferCtx *ctx,
			   gint64 *written,
			   gint64 *total);

#ifdef __cplusplus
}
#endif

#endif /* _BURN_XFER_H */

 
