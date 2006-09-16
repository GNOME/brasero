/***************************************************************************
 *            burn-libisofs.h
 *
 *  lun aoû 21 14:34:32 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "burn-job.h"

#ifndef BURN_LIBISOFS_H
#define BURN_LIBISOFS_H

G_BEGIN_DECLS

#ifdef HAVE_LIBBURN

#define BRASERO_TYPE_LIBISOFS         (brasero_libisofs_get_type ())
#define BRASERO_LIBISOFS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LIBISOFS, BraseroLibisofs))
#define BRASERO_LIBISOFS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LIBISOFS, BraseroLibisofsClass))
#define BRASERO_IS_LIBISOFS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LIBISOFS))
#define BRASERO_IS_LIBISOFS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LIBISOFS))
#define BRASERO_LIBISOFS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LIBISOFS, BraseroLibisofsClass))

typedef struct _BraseroLibisofs BraseroLibisofs;
typedef struct _BraseroLibisofsPrivate BraseroLibisofsPrivate;
typedef struct _BraseroLibisofsClass BraseroLibisofsClass;

struct _BraseroLibisofs {
	BraseroJob parent;
	BraseroLibisofsPrivate *priv;
};

struct _BraseroLibisofsClass {
	BraseroJobClass parent_class;
};

GType brasero_libisofs_get_type ();
BraseroLibisofs *brasero_libisofs_new ();

#else

#define BRASERO_TYPE_LIBISOFS G_TYPE_NONE

#endif /* HAVE_LIBBURN */

G_END_DECLS

#endif /* BURN_LIBISOFS_H */
