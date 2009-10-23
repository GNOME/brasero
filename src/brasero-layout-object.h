/***************************************************************************
 *            brasero-layout-object.h
 *
 *  dim oct 15 17:15:58 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef BRASERO_LAYOUT_OBJECT_H
#define BRASERO_LAYOUT_OBJECT_H

#include <glib.h>
#include <glib-object.h>

#include "brasero-layout.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_LAYOUT_OBJECT         (brasero_layout_object_get_type ())
#define BRASERO_LAYOUT_OBJECT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LAYOUT_OBJECT, BraseroLayoutObject))
#define BRASERO_IS_LAYOUT_OBJECT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LAYOUT_OBJECT))
#define BRASERO_LAYOUT_OBJECT_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), BRASERO_TYPE_LAYOUT_OBJECT, BraseroLayoutObjectIFace))

typedef struct _BraseroLayoutObject BraseroLayoutObject;
typedef struct _BraseroLayoutIFace BraseroLayoutObjectIFace;

struct _BraseroLayoutIFace {
	GTypeInterface g_iface;

	void	(*get_proportion)	(BraseroLayoutObject *self,
					 gint *header,
					 gint *center,
					 gint *footer);
	void	(*set_context)		(BraseroLayoutObject *self,
					 BraseroLayoutType type);
};

GType brasero_layout_object_get_type (void);

void brasero_layout_object_get_proportion (BraseroLayoutObject *self,
					   gint *header,
					   gint *center,
					   gint *footer);

void brasero_layout_object_set_context (BraseroLayoutObject *self,
					BraseroLayoutType type);

G_END_DECLS

#endif /* BRASERO_LAYOUT_OBJECT_H */
