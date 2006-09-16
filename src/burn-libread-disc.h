/***************************************************************************
 *            burn-libread-disc.h
 *
 *  ven aoû 25 22:15:11 2006
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

#ifndef BURN_LIBREAD_DISC_H
#define BURN_LIBREAD_DISC_H

#include <glib.h>
#include <glib-object.h>

#include "burn-libburn-common.h"

#ifdef HAVE_LIBBURN

G_BEGIN_DECLS

#define BRASERO_TYPE_LIBREAD_DISC         (brasero_libread_disc_get_type ())
#define BRASERO_LIBREAD_DISC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LIBREAD_DISC, BraseroLibreadDisc))
#define BRASERO_LIBREAD_DISC_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LIBREAD_DISC, BraseroLibreadDiscClass))
#define BRASERO_IS_LIBREAD_DISC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LIBREAD_DISC))
#define BRASERO_IS_LIBREAD_DISC_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LIBREAD_DISC))
#define BRASERO_LIBREAD_DISC_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LIBREAD_DISC, BraseroLibreadDiscClass))

typedef struct _BraseroLibreadDisc BraseroLibreadDisc;
typedef struct _BraseroLibreadDiscPrivate BraseroLibreadDiscPrivate;
typedef struct _BraseroLibreadDiscClass BraseroLibreadDiscClass;

struct _BraseroLibreadDisc {
	BraseroLibburnCommon parent;
	BraseroLibreadDiscPrivate *priv;
};

struct _BraseroLibreadDiscClass {
	BraseroLibburnCommonClass parent_class;
};

GType brasero_libread_disc_get_type ();
BraseroLibreadDisc *brasero_libread_disc_new ();

G_END_DECLS

#else

#define BRASERO_TYPE_LIBREAD_DISC G_TYPE_NONE

#endif /* HAVE_LIBBURN*/

#endif /* BURN_LIBREAD_DISC_H */
