/***************************************************************************
 *            data-disc.h
 *
 *  dim nov 27 15:34:04 2005
 *  Copyright  2005  Rouquier Philippe
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

#ifndef DATA_DISC_H
#define DATA_DISC_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_DATA_DISC         (brasero_data_disc_get_type ())
#define BRASERO_DATA_DISC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_DATA_DISC, BraseroDataDisc))
#define BRASERO_DATA_DISC_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_DATA_DISC, BraseroDataDiscClass))
#define BRASERO_IS_DATA_DISC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_DATA_DISC))
#define BRASERO_IS_DATA_DISC_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_DATA_DISC))
#define BRASERO_DATA_DISC_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_DATA_DISC, BraseroDataDiscClass))

typedef struct BraseroDataDiscPrivate BraseroDataDiscPrivate;

typedef struct {
	GtkVBox parent;
	BraseroDataDiscPrivate *priv;
} BraseroDataDisc;

typedef struct {
	GtkVBoxClass parent_class;
} BraseroDataDiscClass;

GType brasero_data_disc_get_type();
GtkWidget *brasero_data_disc_new();

#endif /* DATA_DISC_H */
