/***************************************************************************
 *            burn-sum.h
 *
 *  ven aoû  4 19:46:34 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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

#ifndef BURN_SUM_H
#define BURN_SUM_H

#include <glib.h>
#include <glib-object.h>

#include "burn-job.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_BURN_SUM         (brasero_burn_sum_get_type ())
#define BRASERO_BURN_SUM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURN_SUM, BraseroBurnSum))
#define BRASERO_BURN_SUM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURN_SUM, BraseroBurnSumClass))
#define BRASERO_IS_BURN_SUM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURN_SUM))
#define BRASERO_IS_BURN_SUM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURN_SUM))
#define BRASERO_BURN_SUM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURN_SUM, BraseroBurnSumClass))

#define BRASERO_CHECKSUM_FILE	".checksum.md5"

typedef struct _BraseroBurnSum BraseroBurnSum;
typedef struct _BraseroBurnSumPrivate BraseroBurnSumPrivate;
typedef struct _BraseroBurnSumClass BraseroBurnSumClass;

struct _BraseroBurnSum {
	BraseroJob parent;
	BraseroBurnSumPrivate *priv;
};

struct _BraseroBurnSumClass {
	BraseroJobClass parent_class;
};

GType brasero_burn_sum_get_type ();
BraseroBurnSum *brasero_burn_sum_new ();

G_END_DECLS

#endif /* BURN_SUM_H */
