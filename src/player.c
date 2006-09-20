/***************************************************************************
*            player.c
*
*  lun mai 30 08:15:01 2005
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef BUILD_PREVIEW
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscale.h>
#include <gtk/gtkrange.h>
#include <gtk/gtkhscale.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkmessagedialog.h>

#include "player.h"
#include "metadata.h"
#include "player-bacon.h"
#include "utils.h"
#include "brasero-uri-container.h"

static void brasero_player_class_init (BraseroPlayerClass *klass);
static void brasero_player_init (BraseroPlayer *sp);
static void brasero_player_finalize (GObject *object);
static void brasero_player_destroy (GtkObject *obj);

static void brasero_player_button_clicked_cb (GtkButton *button,
					      BraseroPlayer *player);

static gboolean brasero_player_update_progress_cb (BraseroPlayer *player);

static char *brasero_player_scale_format_value (GtkScale *scale,
						gdouble value,
						BraseroPlayer *player);
static gboolean brasero_player_range_button_pressed_cb (GtkWidget *widget,
							GdkEvent *event,
							BraseroPlayer *
							player);
static gboolean brasero_player_range_button_released_cb (GtkWidget *
							 widget,
							 GdkEvent *event,
							 BraseroPlayer *
							 player);
static void brasero_player_state_changed_cb (BraseroPlayerBacon *bacon,
					      BraseroPlayerBaconState state,
					      BraseroPlayer *player);
static void
brasero_player_eof_cb (BraseroPlayerBacon *bacon, BraseroPlayer *player);

struct BraseroPlayerPrivate {
	GtkWidget *hbox;

	GtkWidget *frame;
	GtkWidget *notebook;
	GtkWidget *bacon;
	GtkWidget *image_display;
	GtkWidget *controls;

	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *progress;

	GtkWidget *header;
	GtkWidget *size;
	guint update_scale_id;
	guint set_uri_id;
	BraseroPlayerBaconState state;

	BraseroMetadata *metadata;
	char *uri;
};

static GObjectClass *parent_class = NULL;

GType
brasero_player_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroPlayerClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_player_class_init,
			NULL,
			NULL,
			sizeof (BraseroPlayer),
			0,
			(GInstanceInitFunc) brasero_player_init,
		};

		type = g_type_register_static (GTK_TYPE_ALIGNMENT,
					       "BraseroPlayer",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_player_class_init (BraseroPlayerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_player_finalize;
	gtk_object_class->destroy = brasero_player_destroy;
}

static void
brasero_player_init (BraseroPlayer *obj)
{
	obj->priv = g_new0 (BraseroPlayerPrivate, 1);

	obj->priv->frame = gtk_frame_new (_(" Preview "));
	gtk_container_add (GTK_CONTAINER (obj), obj->priv->frame);

	obj->priv->hbox = gtk_hbox_new (FALSE, 8);
	gtk_container_set_border_width (GTK_CONTAINER (obj->priv->hbox), 4);
	gtk_widget_show (obj->priv->hbox);
	gtk_container_add (GTK_CONTAINER (obj->priv->frame), obj->priv->hbox);

	obj->priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj->priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (obj->priv->hbox),
			    obj->priv->notebook,
			    FALSE,
			    FALSE,
			    0);

	obj->priv->image_display = gtk_image_new ();
	gtk_widget_show (obj->priv->image_display);
	gtk_misc_set_alignment (GTK_MISC (obj->priv->image_display), 1.0, 0.0);
	gtk_notebook_append_page (GTK_NOTEBOOK (obj->priv->notebook),
				  obj->priv->image_display,
				  NULL);

	obj->priv->bacon = brasero_player_bacon_new ();
	gtk_widget_show (obj->priv->bacon);
	g_signal_connect (obj->priv->bacon,
			  "state-change",
			  G_CALLBACK (brasero_player_state_changed_cb),
			  obj);
	g_signal_connect (obj->priv->bacon,
			  "eof",
			  G_CALLBACK (brasero_player_eof_cb),
			  obj);
	gtk_notebook_append_page (GTK_NOTEBOOK (obj->priv->notebook),
				  obj->priv->bacon,
				  NULL);
}

static void
brasero_player_destroy (GtkObject *obj)
{
	BraseroPlayer *player;

	player = BRASERO_PLAYER (obj);
	player->priv->image = NULL;

	if (player->priv->update_scale_id) {
		g_source_remove (player->priv->update_scale_id);
		player->priv->update_scale_id = 0;
	}

	if (player->priv->metadata) {
		brasero_metadata_cancel (player->priv->metadata);
		g_object_unref (player->priv->metadata);
		player->priv->metadata = NULL;
	}

	if (player->priv->uri) {
		g_free (player->priv->uri);
		player->priv->uri = NULL;
	}

	if (player->priv->set_uri_id) {
		g_source_remove (player->priv->set_uri_id);
		player->priv->set_uri_id = 0;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
brasero_player_finalize (GObject *object)
{
	BraseroPlayer *cobj;

	cobj = BRASERO_PLAYER (object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_player_new ()
{
	BraseroPlayer *obj;

	obj = BRASERO_PLAYER (g_object_new (BRASERO_TYPE_PLAYER, NULL));

	return GTK_WIDGET (obj);
}

static void
brasero_player_destroy_controls (BraseroPlayer *player)
{
	if (!player->priv->controls)
		return;

	gtk_widget_destroy (player->priv->controls);
	player->priv->controls = NULL;
	player->priv->progress = NULL;
	player->priv->header = NULL;
	player->priv->button = NULL;
	player->priv->image = NULL;
	player->priv->size = NULL;
}

static void
brasero_player_create_controls_stream (BraseroPlayer *player)
{
	GtkWidget *box = NULL;

	if (player->priv->controls)
		brasero_player_destroy_controls (player);

	player->priv->controls = gtk_vbox_new (FALSE, 4);
	player->priv->header = gtk_label_new (_("No file"));
	gtk_label_set_use_markup (GTK_LABEL (player->priv->header), TRUE);
	gtk_label_set_justify (GTK_LABEL (player->priv->header),
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (player->priv->header), 0, 1);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    player->priv->header,
			    TRUE,
			    TRUE,
			    0);

	/* second line : controls */
	box = gtk_hbox_new (FALSE, 12);

	player->priv->button = gtk_button_new ();
	player->priv->image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (player->priv->button), player->priv->image);
	gtk_box_pack_start (GTK_BOX (box), player->priv->button, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (player->priv->button), "clicked",
			  G_CALLBACK (brasero_player_button_clicked_cb),
			  player);

	player->priv->progress = gtk_hscale_new_with_range (0, 1, 500000000);
	gtk_scale_set_digits (GTK_SCALE (player->priv->progress), 0);
	gtk_scale_set_draw_value (GTK_SCALE (player->priv->progress), TRUE);
	gtk_scale_set_value_pos (GTK_SCALE (player->priv->progress), GTK_POS_RIGHT);
	gtk_range_set_update_policy (GTK_RANGE (player->priv->progress), GTK_UPDATE_CONTINUOUS);
	gtk_box_pack_start (GTK_BOX (box), player->priv->progress, TRUE, TRUE, 0);

	g_signal_connect (G_OBJECT (player->priv->progress),
			  "button_press_event",
			  G_CALLBACK (brasero_player_range_button_pressed_cb), player);
	g_signal_connect (G_OBJECT (player->priv->progress),
			  "button_release_event",
			  G_CALLBACK (brasero_player_range_button_released_cb), player);
	g_signal_connect (G_OBJECT (player->priv->progress),
			  "format_value",
			  G_CALLBACK (brasero_player_scale_format_value),
			  player);

	player->priv->size = gtk_label_new (_("out of 0:00"));
	gtk_box_pack_start (GTK_BOX (box),
			    player->priv->size,
			    FALSE,
			    FALSE,
			    0);

	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    box,
			    FALSE,
			    FALSE,
			    0);

	gtk_box_pack_start (GTK_BOX (player->priv->hbox),
			    player->priv->controls,
			    TRUE,
			    TRUE,
			    0);

	gtk_widget_show (player->priv->frame);
	gtk_widget_show_all (player->priv->controls);
	gtk_alignment_set_padding (GTK_ALIGNMENT (player), 12, 0, 0, 0);
}

static void
brasero_player_set_length (BraseroPlayer *player, gint64 len)
{
	char *time_string;
	char *len_string;

	if (len == -1)
		return;

	time_string = brasero_utils_get_time_string (len, FALSE, TRUE);
	len_string = g_strdup_printf (_("out of %s"), time_string);
	g_free (time_string);
	gtk_label_set_text (GTK_LABEL (player->priv->size), len_string);
	g_free (len_string);

	gtk_range_set_range (GTK_RANGE (player->priv->progress), 0.0, (double) len);
}

static void
brasero_player_create_controls_image (BraseroPlayer *player)
{
	if (player->priv->controls)
		brasero_player_destroy_controls (player);

	player->priv->controls = gtk_vbox_new (FALSE, 4);

	gtk_box_pack_end (GTK_BOX (player->priv->hbox),
			  player->priv->controls,
			  TRUE,
			  TRUE,
			  0);

	player->priv->header = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (player->priv->header), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    player->priv->header,
			    FALSE,
			    FALSE,
			    0);

	player->priv->size = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (player->priv->size), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    player->priv->size,
			    FALSE,
			    FALSE,
			    0);

	gtk_widget_show_all (player->priv->controls);
	gtk_alignment_set_padding (GTK_ALIGNMENT (player), 12, 0, 0, 0);
}

static void
brasero_player_no_multimedia_stream (BraseroPlayer *player)
{
	if (player->priv->update_scale_id) {
		g_source_remove (player->priv->update_scale_id);
		player->priv->update_scale_id = 0;
	}

	gtk_alignment_set_padding (GTK_ALIGNMENT (player), 0, 0, 0, 0);

	gtk_widget_hide (player->priv->notebook);
	gtk_widget_hide (player->priv->frame);
	brasero_player_destroy_controls (player);
}

static void
brasero_player_image (BraseroPlayer *player)
{
	GdkPixbuf *scaled = NULL;
	GError *error = NULL;
	GdkPixbuf *pixbuf;
	gint width, height;
	gchar *string;
	gchar *path;
	gchar *name;

	/* image */
	/* FIXME: this does not allow to preview remote files */
	path = gnome_vfs_get_local_path_from_uri (player->priv->uri);
	pixbuf = gdk_pixbuf_new_from_file (path, &error);

	if (!pixbuf) {
		brasero_player_no_multimedia_stream (player);
		if (error) {
			g_warning ("Couldn't load image %s\n", error->message);
			g_error_free (error);
		}

		g_free (path);
		return;
	}

	height = gdk_pixbuf_get_height (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);

	/* the image itself */
	if (width > PLAYER_BACON_WIDTH || height > PLAYER_BACON_HEIGHT) {
		gdouble ratio;

		if (width / (gdouble) PLAYER_BACON_WIDTH > height / (gdouble) PLAYER_BACON_HEIGHT)
			ratio = (gdouble) width / (gdouble) PLAYER_BACON_WIDTH;
		else
			ratio = (gdouble) height / (gdouble) PLAYER_BACON_HEIGHT;

		scaled = gdk_pixbuf_scale_simple (pixbuf,
						  width / ratio,
						  height / ratio,
						  GDK_INTERP_BILINEAR);
		g_object_unref (pixbuf);
	}
	else
		scaled = pixbuf;

	if (!scaled) {
		brasero_player_no_multimedia_stream (player);
		g_free (path);
		return;
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (player->priv->image_display), scaled);
	g_object_unref (scaled);

	/* display information about the image */
	brasero_player_create_controls_image (player);

    	BRASERO_GET_BASENAME_FOR_DISPLAY (path, name);
	g_free (path);

	string = g_strdup_printf (_("<span weight=\"bold\">Name:</span>\t %s"), name);
	g_free (name);

	gtk_label_set_markup (GTK_LABEL (player->priv->header), string);
	g_free (string);

	string = g_strdup_printf (_("<span weight=\"bold\">Size:</span>\t<i><span size=\"smaller\"> %i x %i pixels</span></i>"), width, height);
	gtk_label_set_markup (GTK_LABEL (player->priv->size), string);
	g_free (string);

	gtk_widget_hide (player->priv->bacon);
	gtk_widget_show (player->priv->image_display);
	gtk_widget_show (player->priv->frame);
	gtk_widget_show (player->priv->notebook);
	gtk_alignment_set_padding (GTK_ALIGNMENT (player), 12, 0, 0, 0);
}

static void
brasero_player_update_info_real (BraseroPlayer *player,
				 const char *artist,
				 const char *title,
				 gint64 len)
{
	char *header;

	brasero_player_set_length (player, len);
	if (artist && title) {
		header = g_markup_printf_escaped (_("<span weight=\"bold\">%s</span>\nby <span size=\"smaller\"><i>%s</i></span>"),
						  title,
						  artist);
		gtk_label_set_ellipsize (GTK_LABEL (player->priv->header),
					 PANGO_ELLIPSIZE_END);

	}
	else if (title) {
		header = g_markup_printf_escaped (_("<span weight=\"bold\">%s</span>"),
						  title);
		gtk_label_set_ellipsize (GTK_LABEL (player->priv->header),
					 PANGO_ELLIPSIZE_END);
	}
	else {
		gchar *name;

	    	BRASERO_GET_BASENAME_FOR_DISPLAY (player->priv->uri, name);
		header = g_markup_printf_escaped (_("<span weight=\"bold\">%s</span>"),
						  name);
		g_free (name);
		gtk_label_set_ellipsize (GTK_LABEL (player->priv->header),
					 PANGO_ELLIPSIZE_END);
	}

	gtk_label_set_markup (GTK_LABEL (player->priv->header), header);
	g_free (header);
}

static void
brasero_player_metadata_completed (BraseroMetadata *metadata,
				   const GError *error,
				   BraseroPlayer *player)
{
	if (error) {
		/* see if it's not an image */
		if (metadata->type
		&&  !strncmp ("image/", metadata->type, 6))
			brasero_player_image (player);
		else
			brasero_player_no_multimedia_stream (player);

		player->priv->metadata = NULL;
		g_object_unref (metadata);
		return;
	}

	/* based on the mime type, we try to determine the type of file */
	if (metadata->has_video) {
		/* video */
		brasero_player_create_controls_stream (player);
		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0.0);

		if (metadata->is_seekable)
			gtk_widget_set_sensitive (player->priv->progress, TRUE);
		else
			gtk_widget_set_sensitive (player->priv->progress, FALSE);

		gtk_widget_show (player->priv->bacon);
		gtk_widget_hide (player->priv->image_display);
		gtk_widget_show (player->priv->notebook);
	}
	else if (metadata->has_audio) {
		/* audio */
		brasero_player_create_controls_stream (player);
		gtk_widget_hide (player->priv->notebook);
		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0.0);

		if (metadata->is_seekable)
			gtk_widget_set_sensitive (player->priv->progress, TRUE);
		else
			gtk_widget_set_sensitive (player->priv->progress, FALSE);
	}
	else if (!strncmp ("image/", metadata->type, 6)) {
		brasero_player_image (player);
		return;
	}
	else {
		brasero_player_destroy_controls (player);
		return;
	}

	/* only reached for audio/video */
	brasero_player_update_info_real (player,
					 metadata->artist,
					 metadata->title,
					 metadata->len);

	player->priv->state = BACON_STATE_READY;

	player->priv->metadata = NULL;
	g_object_unref (metadata);
}

static gboolean
brasero_player_set_uri_timeout (BraseroPlayer *player)
{
	BraseroMetadata *metadata;

	metadata = brasero_metadata_new (player->priv->uri);
	if (!metadata)
		return FALSE;

	player->priv->metadata = metadata;
	g_signal_connect (player->priv->metadata,
			  "completed",
			  G_CALLBACK (brasero_player_metadata_completed),
			  player);

	brasero_metadata_get_async (player->priv->metadata, TRUE);
	player->priv->set_uri_id = 0;

	return FALSE;
}

void
brasero_player_set_uri (BraseroPlayer *player, const char *uri)
{
	/* avoid reloading everything if it's the same uri */
	if (uri && player->priv->uri
	&&  !strcmp (uri, player->priv->uri))
		return;

	if (player->priv->uri)
		g_free (player->priv->uri);

	player->priv->uri = g_strdup (uri);

	if (player->priv->metadata) {
		brasero_metadata_cancel (player->priv->metadata);
		g_object_unref (player->priv->metadata);
		player->priv->metadata = NULL;
	}

	if (player->priv->set_uri_id) {
		g_source_remove (player->priv->set_uri_id);
		player->priv->set_uri_id = 0;
	}

	brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (player->priv->bacon),
				      NULL);
	if (!uri) {
		brasero_player_no_multimedia_stream (player);
		return;
	}

	/* we add a timeout to wait a little since it could be the arrow keys
	 * which are pressed and in this case we can't keep on setting uris */
	player->priv->set_uri_id = g_timeout_add (400,
						  (GSourceFunc) brasero_player_set_uri_timeout,
						  player);
}

static void
brasero_player_button_clicked_cb (GtkButton *button,
				  BraseroPlayer *player)
{
	if (player->priv->state == BACON_STATE_READY) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (player->priv->notebook), 2);
		brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (player->priv->bacon), player->priv->uri);
		brasero_player_bacon_play (BRASERO_PLAYER_BACON (player->priv->bacon));
	}
	else if (player->priv->state == BACON_STATE_PAUSED)
		brasero_player_bacon_play (BRASERO_PLAYER_BACON (player->priv->bacon));
	else if (player->priv->state == BACON_STATE_PLAYING)
		brasero_player_bacon_stop (BRASERO_PLAYER_BACON (player->priv->bacon));
}

static gboolean
brasero_player_update_progress_cb (BraseroPlayer *player)
{
	gint64 pos;

	if (brasero_player_bacon_get_pos (BRASERO_PLAYER_BACON (player->priv->bacon), &pos) == TRUE) {
		gtk_range_set_value (GTK_RANGE (player->priv->progress), (double) pos);

		/* This is done on purpose with videos it wouldn't redraw automatically 
		 * I don't know why */
		gtk_widget_queue_draw (GTK_WIDGET (player->priv->progress));
	}

	return TRUE;
}

static gboolean
brasero_player_range_button_pressed_cb (GtkWidget *widget,
					GdkEvent *event,
					BraseroPlayer *player)
{
	if (player->priv->update_scale_id) {
		g_source_remove (player->priv->update_scale_id);
		player->priv->update_scale_id = 0;
	}

	return FALSE;
}

static gboolean
brasero_player_range_button_released_cb (GtkWidget *widget,
					 GdkEvent *event,
					 BraseroPlayer *player)
{
	if (player->priv->state >= BACON_STATE_PAUSED) {
		double pos;

		pos = gtk_range_get_value (GTK_RANGE (player->priv->progress));
		if (brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon), pos) == FALSE) {
			gint64 oldpos = 0.0;
			gint64 length = 0.0;

			brasero_player_bacon_get_pos (BRASERO_PLAYER_BACON (player->priv->bacon), &oldpos);
			gtk_range_set_value (GTK_RANGE (player->priv->progress), (double) oldpos);

			brasero_player_bacon_get_length (BRASERO_PLAYER_BACON (player->priv->bacon), &length);
			brasero_player_set_length (player, (gdouble) length);
		}
	}

	if (player->priv->state == BACON_STATE_PLAYING)
		player->priv->update_scale_id = g_timeout_add (500,
							       (GSourceFunc) brasero_player_update_progress_cb,
							       player);
	return FALSE;
}

static void
brasero_player_eof_cb (BraseroPlayerBacon *bacon, BraseroPlayer *player)
{
	gtk_image_set_from_stock (GTK_IMAGE (player->priv->image),
				  GTK_STOCK_MEDIA_PLAY,
				  GTK_ICON_SIZE_BUTTON);
	
	if (player->priv->update_scale_id) {
		g_source_remove (player->priv->update_scale_id);
		player->priv->update_scale_id = 0;
	}

	gtk_range_set_value (GTK_RANGE (player->priv->progress), 0.0);
	brasero_player_bacon_stop (BRASERO_PLAYER_BACON (player->priv->bacon));
	brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon), 0.0);
	player->priv->state = BACON_STATE_PAUSED;
}

static void
brasero_player_state_changed_cb (BraseroPlayerBacon *bacon,
				 BraseroPlayerBaconState state,
				 BraseroPlayer *player)
{
	if (player->priv->state == state)
		return;

	switch (state) {
	case BACON_STATE_ERROR:
		brasero_player_no_multimedia_stream (player);
		break;

	case BACON_STATE_PAUSED:
		gtk_image_set_from_stock (GTK_IMAGE (player->priv->image),
					  GTK_STOCK_MEDIA_PLAY,
					  GTK_ICON_SIZE_BUTTON);
	
		if (player->priv->update_scale_id) {
			g_source_remove (player->priv->update_scale_id);
			player->priv->update_scale_id = 0;
		}
		break;

	case BACON_STATE_PLAYING:
		if (player->priv->state == BACON_STATE_READY) {
			double pos;
			gint64 length;

			brasero_player_bacon_get_length (BRASERO_PLAYER_BACON (player->priv->bacon), &length);
			brasero_player_set_length (player, (gdouble) length);

			pos = gtk_range_get_value (GTK_RANGE (player->priv->progress));
			brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon), pos);
		}

		gtk_image_set_from_stock (GTK_IMAGE (player->priv->image),
					  GTK_STOCK_MEDIA_PAUSE,
					  GTK_ICON_SIZE_BUTTON);

		if (!player->priv->update_scale_id)
			player->priv->update_scale_id = g_timeout_add (500,
								       (GSourceFunc) brasero_player_update_progress_cb,
								       player);
		break;

	default:
		break;
	}

	player->priv->state = state;
}

static char *
brasero_player_scale_format_value (GtkScale *scale,
				   gdouble value,
				   BraseroPlayer *player)
{
	return brasero_utils_get_time_string (value, FALSE, TRUE);
}

static void
brasero_player_source_selection_changed_cb (BraseroURIContainer *source,
					    BraseroPlayer *player)
{
	char *uri;

	uri = brasero_uri_container_get_selected_uri (source);
	brasero_player_set_uri (player, uri);
	g_free (uri);
}

void
brasero_player_add_source (BraseroPlayer *player, BraseroURIContainer *source)
{
	g_signal_connect (source,
			  "uri-selected",
			  G_CALLBACK (brasero_player_source_selection_changed_cb),
			  player);
}

#endif
