/***************************************************************************
 *            brasero-sum-checks.h
 *
 *  lun sep 11 07:39:26 2006
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

#ifndef BRASERO_SUM_CHECKS_H
#define BRASERO_SUM_CHECKS_H

#include <glib.h>

#include "burn-basics.h"

G_BEGIN_DECLS

typedef struct _BraseroSumCheckCtx BraseroSumCheckCtx;

BraseroSumCheckCtx *
brasero_sum_check_new (void);

void
brasero_sum_check_free (BraseroSumCheckCtx*ctx);

BraseroBurnResult
brasero_sum_check (BraseroSumCheckCtx *ctx,
		   const gchar *path,
		   GSList **wrong_sums,
		   GError **error);

void
brasero_sum_check_cancel (BraseroSumCheckCtx *ctx);

void
brasero_sum_check_progress (BraseroSumCheckCtx *ctx,
			    gint *checked,
			    gint *total);

G_END_DECLS

#endif /* BRASERO_SUM_CHECKS_H */
