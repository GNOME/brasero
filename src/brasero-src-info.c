/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero-git-trunk
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero-git-trunk is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero-git-trunk is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "burn-medium.h"

#include "brasero-utils.h"
#include "brasero-src-info.h"

typedef struct _BraseroSrcInfoPrivate BraseroSrcInfoPrivate;
struct _BraseroSrcInfoPrivate
{
	GtkWidget *image;
//	GtkWidget *status;
	GtkWidget *capacity;
	BraseroMedium *medium;
};

#define BRASERO_SRC_INFO_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SRC_INFO, BraseroSrcInfoPrivate))


G_DEFINE_TYPE (BraseroSrcInfo, brasero_src_info, GTK_TYPE_HBOX);

static void
brasero_src_info_update (BraseroSrcInfo *self)
{
	gchar *string;
	gint64 sectors = 0;
	BraseroMedia media;
	BraseroSrcInfoPrivate *priv;

	priv = BRASERO_SRC_INFO_PRIVATE (self);

	if (priv->medium)
		media = brasero_medium_get_status (priv->medium);
	else
		media = BRASERO_MEDIUM_NONE;

	gtk_label_set_text (GTK_LABEL (priv->capacity), "");
//	gtk_label_set_text (GTK_LABEL (priv->status), "");

	/* type */
	if (media == BRASERO_MEDIUM_NONE) {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "drive-optical",
					      GTK_ICON_SIZE_DIALOG);

		gtk_label_set_text (GTK_LABEL (priv->capacity), _("no medium"));
//		gtk_label_set_text (GTK_LABEL (priv->status), _("no medium"));
		return;
	}

	if (media == BRASERO_MEDIUM_UNSUPPORTED) {
		gtk_label_set_text (GTK_LABEL (priv->capacity), _("no supported medium"));
//		gtk_label_set_text (GTK_LABEL (priv->status), _("no supported medium"));
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "media-optical",
					      GTK_ICON_SIZE_DIALOG);
		return;
	}

	if (media == BRASERO_MEDIUM_BUSY) {
		gtk_label_set_text (GTK_LABEL (priv->capacity), _("medium busy"));
//		gtk_label_set_text (GTK_LABEL (priv->status), _("medium busy"));
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "media-optical",
					      GTK_ICON_SIZE_DIALOG);
		return;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      brasero_medium_get_icon (priv->medium),
				      GTK_ICON_SIZE_DIALOG);

	if (media & BRASERO_MEDIUM_BLANK) {
		gtk_label_set_text (GTK_LABEL (priv->capacity), _("0"));
//		gtk_label_set_text (GTK_LABEL (priv->status), _("blank medium"));
		return;
	}

	brasero_medium_get_data_size (priv->medium, NULL, &sectors);
	string = brasero_utils_get_sectors_string (sectors,
						   !(media & BRASERO_MEDIUM_HAS_DATA),
						   TRUE,
						   TRUE);

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) {
		gtk_label_set_text (GTK_LABEL (priv->capacity), string);
//		gtk_label_set_text (GTK_LABEL (priv->status), _("audio and data"));
	}
	else if (media & BRASERO_MEDIUM_HAS_AUDIO) {
		gtk_label_set_text (GTK_LABEL (priv->capacity), string);
//		gtk_label_set_text (GTK_LABEL (priv->status), _("audio only"));
	}
	else if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_HAS_DATA|BRASERO_MEDIUM_PROTECTED)) {
		gtk_label_set_text (GTK_LABEL (priv->capacity), string);
//		gtk_label_set_text (GTK_LABEL (priv->status), _("protected data"));
	}
	else if (media & BRASERO_MEDIUM_HAS_DATA) {
		gtk_label_set_text (GTK_LABEL (priv->capacity), string);
//		gtk_label_set_text (GTK_LABEL (priv->status), _("data only"));
	}

	g_free (string);
}

void
brasero_src_info_set_medium (BraseroSrcInfo *self,
			     BraseroMedium *medium)
{
	BraseroSrcInfoPrivate *priv;

	priv = BRASERO_SRC_INFO_PRIVATE (self);

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	if (medium) {
		priv->medium = medium;
		g_object_ref (medium);
	}

	brasero_src_info_update (self);	
}

static void
brasero_src_info_init (BraseroSrcInfo *object)
{
	BraseroSrcInfoPrivate *priv;
	GtkWidget *table;
	GtkWidget *label;

	priv = BRASERO_SRC_INFO_PRIVATE (object);

	gtk_box_set_spacing (GTK_BOX (object), 12);

	priv->image = gtk_image_new ();
	gtk_box_pack_end (GTK_BOX (object), priv->image, FALSE, FALSE, 0);
//	gtk_widget_show (priv->image);

	table = gtk_table_new (2, 1, FALSE);
	gtk_widget_show (table);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);

	label = gtk_label_new (_("<b>Size:</b>"));
	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);

	priv->capacity = gtk_label_new ("");
	gtk_widget_show (priv->capacity);
	gtk_misc_set_alignment (GTK_MISC (priv->capacity), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), priv->capacity, 1, 2, 0, 1,
			  GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);

/*	label = gtk_label_new (_("<b>Status:</b>"));
	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);

	priv->status = gtk_label_new ("");
	gtk_widget_show (priv->status);
	gtk_misc_set_alignment (GTK_MISC (priv->status), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), priv->status, 1, 2, 1, 2,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
*/
	gtk_box_pack_start (GTK_BOX (object), table, FALSE, TRUE, 0);
}

static void
brasero_src_info_finalize (GObject *object)
{
	BraseroSrcInfoPrivate *priv;

	priv = BRASERO_SRC_INFO_PRIVATE (object);

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	G_OBJECT_CLASS (brasero_src_info_parent_class)->finalize (object);
}

static void
brasero_src_info_class_init (BraseroSrcInfoClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSrcInfoPrivate));

	object_class->finalize = brasero_src_info_finalize;
}

GtkWidget *
brasero_src_info_new (void)
{
	return g_object_new (BRASERO_TYPE_SRC_INFO, NULL);
}
