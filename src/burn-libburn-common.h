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

#ifdef HAVE_LIBBURN

#include <libburn/libburn.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_LIBBURN_COMMON         (brasero_libburn_common_get_type ())
#define BRASERO_LIBBURN_COMMON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LIBBURN_COMMON, BraseroLibburnCommon))
#define BRASERO_LIBBURN_COMMON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LIBBURN_COMMON, BraseroLibburnCommonClass))
#define BRASERO_IS_LIBBURN_COMMON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LIBBURN_COMMON))
#define BRASERO_IS_LIBBURN_COMMON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LIBBURN_COMMON))
#define BRASERO_LIBBURN_COMMON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LIBBURN_COMMON, BraseroLibburnCommonClass))

typedef struct _BraseroLibburnCommon BraseroLibburnCommon;
typedef struct _BraseroLibburnCommonPrivate BraseroLibburnCommonPrivate;
typedef struct _BraseroLibburnCommonClass BraseroLibburnCommonClass;

struct _BraseroLibburnCommon {
	BraseroJob parent;
	BraseroLibburnCommonPrivate *priv;
};

struct _BraseroLibburnCommonClass {
	BraseroJobClass parent_class;
};

GType brasero_libburn_common_get_type ();
BraseroLibburnCommon *brasero_libburn_common_new ();

BraseroBurnResult
brasero_libburn_common_set_drive (BraseroLibburnCommon *self,
				  NautilusBurnDrive *drive,
				  GError **error);
BraseroBurnResult
brasero_libburn_common_get_drive (BraseroLibburnCommon *self,
				  struct burn_drive **drive);

BraseroBurnResult
brasero_libburn_common_set_disc (BraseroLibburnCommon *self,
				 struct burn_disc *disc);
BraseroBurnResult
brasero_libburn_common_get_disc (BraseroLibburnCommon *self,
				 struct burn_disc **disc);

G_END_DECLS

#endif /* HAVE_LIBBURN */

#endif /* BURN_LIBBURN_COMMON_H */
