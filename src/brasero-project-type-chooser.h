/***************************************************************************
*            cd-type-chooser.h
*
*  ven mai 27 17:33:12 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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

#ifndef CD_TYPE_CHOOSER_H
#define CD_TYPE_CHOOSER_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkeventbox.h>

G_BEGIN_DECLS
#define BRASERO_TYPE_PROJECT_TYPE_CHOOSER         (brasero_project_type_chooser_get_type ())
#define BRASERO_PROJECT_TYPE_CHOOSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_PROJECT_TYPE_CHOOSER, BraseroProjectTypeChooser))
#define BRASERO_PROJECT_TYPE_CHOOSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k),BRASERO_TYPE_PROJECT_TYPE_CHOOSER, BraseroProjectTypeChooserClass))
#define BRASERO_IS_PROJECT_TYPE_CHOOSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_PROJECT_TYPE_CHOOSER))
#define BRASERO_IS_PROJECT_TYPE_CHOOSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_PROJECT_TYPE_CHOOSER))
#define BRASERO_PROJECT_TYPE_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_PROJECT_TYPE_CHOOSER, BraseroProjectTypeChooserClass))

typedef struct BraseroProjectTypeChooserPrivate BraseroProjectTypeChooserPrivate;

typedef struct {
	GtkEventBox parent;
	BraseroProjectTypeChooserPrivate *priv;
} BraseroProjectTypeChooser;

typedef enum {
	BRASERO_PROJECT_TYPE_INVALID,
	BRASERO_PROJECT_TYPE_COPY,
	BRASERO_PROJECT_TYPE_ISO,
	BRASERO_PROJECT_TYPE_AUDIO,
	BRASERO_PROJECT_TYPE_DATA,
} BraseroProjectType;

typedef struct {
	GtkEventBoxClass parent_class;

	void	(*recent_clicked)	(BraseroProjectTypeChooser *chooser,
					 const gchar *uri);
	void	(*chosen)		(BraseroProjectTypeChooser *chooser,
					 BraseroProjectType type);
} BraseroProjectTypeChooserClass;

GType brasero_project_type_chooser_get_type ();
GtkWidget *brasero_project_type_chooser_new ();

#endif				/* CD_TYPE_CHOOSER_H */
