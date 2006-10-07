/***************************************************************************
 *            brasero-image-type-chooser.c
 *
 *  mar oct  3 18:40:02 2006
 *  Copyright  2006  Philippe Rouquier
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
#include <glib/gi18n-lib.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtkliststore.h>

#include "burn-basics.h"
#include "burn-caps.h"
#include "brasero-image-type-chooser.h"
 
static void brasero_image_type_chooser_class_init (BraseroImageTypeChooserClass *klass);
static void brasero_image_type_chooser_init (BraseroImageTypeChooser *sp);
static void brasero_image_type_chooser_finalize (GObject *object);

struct _BraseroImageTypeChooserPrivate {
	GtkWidget *combo;

	BraseroBurnCaps *caps;
	BraseroImageFormat *formats;
};

static GtkHBoxClass *parent_class = NULL;

GType
brasero_image_type_chooser_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroImageTypeChooserClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_image_type_chooser_class_init,
			NULL,
			NULL,
			sizeof (BraseroImageTypeChooser),
			0,
			(GInstanceInitFunc)brasero_image_type_chooser_init,
		};

		type = g_type_register_static (GTK_TYPE_HBOX, 
					       "BraseroImageTypeChooser",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_image_type_chooser_class_init(BraseroImageTypeChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_image_type_chooser_finalize;
}

static void
brasero_image_type_chooser_init (BraseroImageTypeChooser *obj)
{
	GtkWidget *label;

	obj->priv = g_new0 (BraseroImageTypeChooserPrivate, 1);

	label = gtk_label_new (_("Image type:\t"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (obj), label, FALSE, FALSE, 0);

	obj->priv->combo = gtk_combo_box_new_text ();
	gtk_widget_show (obj->priv->combo);
	gtk_box_pack_end (GTK_BOX (obj), obj->priv->combo, TRUE, TRUE, 0);

	obj->priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_image_type_chooser_finalize (GObject *object)
{
	BraseroImageTypeChooser *cobj;

	cobj = BRASERO_IMAGE_TYPE_CHOOSER (object);

	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	if (cobj->priv->formats) {
		g_free (cobj->priv->formats);
		cobj->priv->formats = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_image_type_chooser_new ()
{
	BraseroImageTypeChooser *obj;

	obj = BRASERO_IMAGE_TYPE_CHOOSER (g_object_new (BRASERO_TYPE_IMAGE_TYPE_CHOOSER, NULL));
	
	return GTK_WIDGET (obj);
}

void
brasero_image_type_chooser_set_source (BraseroImageTypeChooser *self,
				       NautilusBurnDrive *drive,
				       BraseroTrackSourceType type,
				       BraseroImageFormat format)
{
	BraseroImageFormat *formats;
	BraseroImageFormat *iter;
	GtkTreeModel *model;

	/* clean */
	if (self->priv->formats) {
		g_free (self->priv->formats);
		self->priv->formats = NULL;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->priv->combo));
	if (GTK_IS_LIST_STORE (model))
		gtk_list_store_clear (GTK_LIST_STORE (model));
	else if (GTK_IS_TREE_STORE (model))
		gtk_tree_store_clear (GTK_TREE_STORE (model));

	/* now we get the targets available and display them */
	gtk_combo_box_append_text (GTK_COMBO_BOX (self->priv->combo), _("Let brasero choose (safest)"));
	brasero_burn_caps_get_imager_available_formats (self->priv->caps,
							drive,
							type,
							&formats);

	for (iter = formats; iter [0] != BRASERO_IMAGE_FORMAT_NONE; iter ++) {
		if (iter [0] & BRASERO_IMAGE_FORMAT_JOLIET)
			gtk_combo_box_append_text (GTK_COMBO_BOX (self->priv->combo), _("*.iso (joliet) image"));
		if (iter [0] & BRASERO_IMAGE_FORMAT_ISO)
			gtk_combo_box_append_text (GTK_COMBO_BOX (self->priv->combo), _("*.iso image"));
		else if (iter [0] & BRASERO_IMAGE_FORMAT_CLONE)
			gtk_combo_box_append_text (GTK_COMBO_BOX (self->priv->combo), _("*.raw image"));
		else if (iter [0] & BRASERO_IMAGE_FORMAT_CUE)
			gtk_combo_box_append_text (GTK_COMBO_BOX (self->priv->combo), _("*.cue image"));
		else if (iter [0] & BRASERO_IMAGE_FORMAT_CDRDAO)
			gtk_combo_box_append_text (GTK_COMBO_BOX (self->priv->combo), _("*.toc image (cdrdao)"));
	}

	if (format != BRASERO_IMAGE_FORMAT_ANY
	&&  format != BRASERO_IMAGE_FORMAT_NONE) {
		gint i;

		/* we find the number of the target if it is still available */
		for (i = 0; formats [i] != BRASERO_IMAGE_FORMAT_NONE; i++) {
			if (formats [i] == format) {
				gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combo), i);
				break;
			}
		}
	}
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combo), 0);

	/* just to make sure we see if there is a line which is active. It can 
	 * happens that the last time it was a CD and the user chose RAW. If it
	 * is now a DVD it can't be raw any more */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self->priv->combo)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->combo), 0);

	self->priv->formats = formats;
}

void
brasero_image_type_chooser_get_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat *format)
{
	gint type;

	type = gtk_combo_box_get_active (GTK_COMBO_BOX (self->priv->combo));
	if (type == 0) 
		*format = BRASERO_IMAGE_FORMAT_ANY;
	else
		*format = self->priv->formats [type - 1];
}
