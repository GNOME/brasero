/***************************************************************************
 *            burn-libburn-common.h
 *
 *  mer aoû 30 16:35:40 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef BURN_LIBBURN_COMMON_H
#define BURN_LIBBURN_COMMON_H

#include <glib.h>
#include <glib-object.h>

#include "burn-job.h"

#include <libburn/libburn.h>

G_BEGIN_DECLS

struct _BraseroLibburnCtx {
	struct burn_drive_info *drive_info;
	struct burn_drive *drive;
	struct burn_disc *disc;

	enum burn_drive_status status;

	/* used detect track hops */
	gint track_num;

	/* used to report current written sector */
	gint64 sectors;
	gint64 cur_sector;
	gint64 track_sectors;

	gint has_leadin;
};
typedef struct _BraseroLibburnCtx BraseroLibburnCtx;

BraseroLibburnCtx *
brasero_libburn_common_ctx_new (BraseroJob *job,
				GError **error);

void
brasero_libburn_common_ctx_free (BraseroLibburnCtx *ctx);

BraseroBurnResult
brasero_libburn_common_status (BraseroJob *job,
			       BraseroLibburnCtx *ctx);

G_END_DECLS

#endif /* BURN_LIBBURN_COMMON_H */
