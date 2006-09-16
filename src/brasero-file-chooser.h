/***************************************************************************
 *            brasero-file-chooser.h
 *
 *  lun mai 29 08:53:18 2006
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

#ifndef BRASERO_FILE_CHOOSER_H
#define BRASERO_FILE_CHOOSER_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkalignment.h>
#include <gtk/gtkfilechooserwidget.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_FILE_CHOOSER         (brasero_file_chooser_get_type ())
#define BRASERO_FILE_CHOOSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_FILE_CHOOSER, BraseroFileChooser))
#define BRASERO_FILE_CHOOSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_FILE_CHOOSER, BraseroFileChooserClass))
#define BRASERO_IS_FILE_CHOOSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_FILE_CHOOSER))
#define BRASERO_IS_FILE_CHOOSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_FILE_CHOOSER))
#define BRASERO_FILE_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_FILE_CHOOSER, BraseroFileChooserClass))

typedef struct BraseroFileChooserPrivate BraseroFileChooserPrivate;

typedef struct {
	GtkAlignment parent;
	BraseroFileChooserPrivate *priv;
} BraseroFileChooser;

typedef struct {
	GtkAlignmentClass parent_class;
} BraseroFileChooserClass;

GType brasero_file_chooser_get_type ();
GtkWidget *brasero_file_chooser_new ();

#endif /* BRASERO_FILE_CHOOSER_H */
