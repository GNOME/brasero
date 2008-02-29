/***************************************************************************
 *            brasero-project-size.h
 *
 *  jeu jui 27 11:54:52 2006
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

#ifndef BRASERO_PROJECT_SIZE_H
#define BRASERO_PROJECT_SIZE_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkcontainer.h>

#include "burn-medium.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_PROJECT_SIZE         (brasero_project_size_get_type ())
#define BRASERO_PROJECT_SIZE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_PROJECT_SIZE, BraseroProjectSize))
#define BRASERO_PROJECT_SIZE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_PROJECT_SIZE, BraseroProjectSizeClass))
#define BRASERO_IS_PROJECT_SIZE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_PROJECT_SIZE))
#define BRASERO_IS_PROJECT_SIZE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_PROJECT_SIZE))
#define BRASERO_PROJECT_SIZE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_PROJECT_SIZE, BraseroProjectSizeClass))

typedef struct _BraseroProjectSize BraseroProjectSize;
typedef struct _BraseroProjectSizePrivate BraseroProjectSizePrivate;
typedef struct _BraseroProjectSizeClass BraseroProjectSizeClass;

struct _BraseroProjectSize {
	GtkContainer parent;
	BraseroProjectSizePrivate *priv;
};

struct _BraseroProjectSizeClass {
	GtkContainerClass parent_class;

	void	(*disc_changed)		(BraseroProjectSize *size);
};

GType brasero_project_size_get_type ();
GtkWidget *brasero_project_size_new ();

gint
brasero_project_get_ruler_height (BraseroProjectSize *self);

void
brasero_project_size_set_sectors (BraseroProjectSize *self,
				  gint64 sectors);

void
brasero_project_size_set_multisession (BraseroProjectSize *self,
				       gboolean multi);

void
brasero_project_size_set_context (BraseroProjectSize *self,
				  gboolean is_audio);

gboolean
brasero_project_size_check_status (BraseroProjectSize *self,
				   gboolean *overburn);

BraseroMedium *
brasero_project_size_get_active_medium (BraseroProjectSize *self);

G_END_DECLS

#endif /* BRASERO_PROJECT_SIZE_H */
