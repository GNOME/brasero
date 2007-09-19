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

#include <gconf/gconf-client.h>

#include "brasero-player.h"
#include "brasero-player-bacon.h"
#include "brasero-utils.h"
#include "brasero-metadata.h"
#include "brasero-vfs.h"
#include "burn-debug.h"

G_DEFINE_TYPE (BraseroPlayer, brasero_player, GTK_TYPE_ALIGNMENT);

struct BraseroPlayerPrivate {
	GtkWidget *hbox;
	GtkWidget *vbox;

	GtkWidget *notebook;
	GtkWidget *bacon;
	GtkWidget *image_display;
	GtkWidget *controls;

	gint image_width;
	gint image_height;
	GdkPixbuf *pixbuf;

	GtkWidget *image;
	GtkWidget *image_zoom_in;
	GtkWidget *image_zoom_out;

	gint video_width;
	gint video_height;
	GtkWidget *video_zoom_in;
	GtkWidget *video_zoom_out;

	GtkWidget *button;
	GtkWidget *progress;

	GtkWidget *header;
	GtkWidget *size;
	guint update_scale_id;
	guint set_uri_id;

	BraseroPlayerBaconState state;

	BraseroVFS *vfs;
	BraseroVFSDataID meta_task;

	gchar *uri;
	gint64 start;
	gint64 length;
};

#define GCONF_IMAGE_SIZE_WIDTH	"/apps/brasero/display/image_width"
#define GCONF_IMAGE_SIZE_HEIGHT	"/apps/brasero/display/image_height"
#define GCONF_VIDEO_SIZE_WIDTH	"/apps/brasero/display/video_width"
#define GCONF_VIDEO_SIZE_HEIGHT	"/apps/brasero/display/video_height"

typedef enum {
	READY_SIGNAL,
	EOF_SIGNAL,
	LAST_SIGNAL
} BraseroPlayerSignalType;
static guint brasero_player_signals [LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static void
brasero_player_destroy_controls (BraseroPlayer *player)
{
	if (!player->priv->controls)
		return;

	gtk_box_set_child_packing (GTK_BOX (player->priv->hbox),
				   player->priv->notebook->parent,
				   FALSE,
				   FALSE,
				   0,
				   GTK_PACK_START);

	gtk_widget_destroy (player->priv->controls);
	player->priv->controls = NULL;
	player->priv->progress = NULL;
	player->priv->header = NULL;
	player->priv->button = NULL;
	player->priv->image = NULL;
	player->priv->size = NULL;
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

	player->priv->length = -1;
	player->priv->start = 0;
}

static void
brasero_player_video_zoom_out (GtkButton *button,
			       BraseroPlayer *player)
{
	gint width, height;

	gtk_widget_set_sensitive (GTK_WIDGET (player->priv->video_zoom_in), TRUE);

	width = GTK_WIDGET (player->priv->bacon)->allocation.width;
	height = GTK_WIDGET (player->priv->bacon)->allocation.height;

	width -= PLAYER_BACON_WIDTH / 3;
	height -= PLAYER_BACON_HEIGHT / 3;

	if (width < (GTK_WIDGET (player)->allocation.width / 2)
	&&  player->priv->controls->parent == player->priv->vbox) {
		g_object_ref (player->priv->controls);
		gtk_container_remove (GTK_CONTAINER (player->priv->vbox),
				      player->priv->controls);

		gtk_box_pack_start (GTK_BOX (player->priv->hbox),
				    player->priv->controls,
				    TRUE,
				    TRUE,
				    0);
		g_object_unref (player->priv->controls);

		gtk_box_set_child_packing (GTK_BOX (player->priv->hbox),
					   player->priv->notebook->parent,
					   FALSE,
					   FALSE,
					   0,
					   GTK_PACK_START);
	}

	if (width <= PLAYER_BACON_WIDTH ||
	    height <= PLAYER_BACON_HEIGHT) {
		width = PLAYER_BACON_WIDTH;
		height = PLAYER_BACON_HEIGHT;
		gtk_widget_set_sensitive (GTK_WIDGET (player->priv->video_zoom_out), FALSE);
	}

	player->priv->video_width = width;
	player->priv->video_height = height;

	gtk_widget_set_size_request (GTK_WIDGET (player->priv->bacon),
				     width,
				     height);
}

static void
brasero_player_video_zoom_in (GtkButton *button,
			      BraseroPlayer *player)
{
	gint width, height;

	gtk_widget_set_sensitive (GTK_WIDGET (player->priv->video_zoom_out), TRUE);

	width = player->priv->bacon->allocation.width;
	height = player->priv->bacon->allocation.height;

	width += PLAYER_BACON_WIDTH / 3;
	height += PLAYER_BACON_HEIGHT / 3;

	if (width >= (GTK_WIDGET (player)->allocation.width / 2)
	&&  player->priv->controls->parent == player->priv->hbox) {
		g_object_ref (player->priv->controls);
		gtk_container_remove (GTK_CONTAINER (player->priv->hbox),
				      player->priv->controls);

		gtk_box_pack_start (GTK_BOX (player->priv->vbox),
				    player->priv->controls,
				    TRUE,
				    TRUE,
				    0);
		g_object_unref (player->priv->controls);

		gtk_box_set_child_packing (GTK_BOX (player->priv->hbox),
					   player->priv->notebook->parent,
					   TRUE,
					   TRUE,
					   0,
					   GTK_PACK_START);
	}

	if (width >= PLAYER_BACON_WIDTH * 3 ||
	    height >= PLAYER_BACON_HEIGHT * 3) {
		width = PLAYER_BACON_WIDTH * 3;
		height = PLAYER_BACON_HEIGHT * 3;
		gtk_widget_set_sensitive (GTK_WIDGET (player->priv->video_zoom_in), FALSE);
	}

	player->priv->video_width = width;
	player->priv->video_height = height;

	gtk_widget_set_size_request (GTK_WIDGET (player->priv->bacon),
				     width,
				     height);
}

static void
brasero_player_update_position (BraseroPlayer *player)
{
	gdouble value;
	GtkAdjustment *adjustment;
	gchar *pos_string, *len_string, *result;

	if (!player->priv->progress || !player->priv->size)
		return;

	adjustment = gtk_range_get_adjustment (GTK_RANGE (player->priv->progress));
	len_string = brasero_utils_get_time_string (player->priv->length, FALSE, FALSE);

	value = gtk_range_get_value (GTK_RANGE (player->priv->progress));
	pos_string = brasero_utils_get_time_string (value, FALSE, FALSE);

	result = g_strdup_printf (_("%s / %s"), pos_string, len_string);
	g_free (len_string);
	g_free (pos_string);

	gtk_label_set_text (GTK_LABEL (player->priv->size), result);
	g_free (result);
}

static void
brasero_player_range_value_changed (GtkRange *range,
				    BraseroPlayer *player)
{
	if (player->priv->state >= BACON_STATE_PAUSED && !player->priv->update_scale_id) {
		gdouble pos;

		/* user changed the value tell the player/pipeline */
		pos = gtk_range_get_value (GTK_RANGE (player->priv->progress));
		if(!brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon), (gint64) pos + player->priv->start))
			BRASERO_BURN_LOG ("position in stream cannot be set");
	}

	brasero_player_update_position (player);
}

static void
brasero_player_set_length (BraseroPlayer *player)
{
	if (player->priv->progress)
		gtk_range_set_range (GTK_RANGE (player->priv->progress),
				     0.0,
				     (gdouble) player->priv->length);

	brasero_player_update_position (player);
}

static gboolean
brasero_player_update_progress_cb (BraseroPlayer *player)
{
	gint64 pos;

	if (brasero_player_bacon_get_pos (BRASERO_PLAYER_BACON (player->priv->bacon), &pos) == TRUE) {
		gtk_range_set_value (GTK_RANGE (player->priv->progress), (gdouble) pos - player->priv->start);

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
	/* stop the automatic update of progress bar position */
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
	/* restart the automatic update of progress bar */
	if (player->priv->state == BACON_STATE_PLAYING && !player->priv->update_scale_id)
		player->priv->update_scale_id = g_timeout_add (500,
							       (GSourceFunc) brasero_player_update_progress_cb,
							       player);

	return FALSE;
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

static void
brasero_player_volume_changed_cb (GtkScaleButton *button,
				  gdouble volume,
				  BraseroPlayer *player)
{
	brasero_player_bacon_set_volume (BRASERO_PLAYER_BACON (player->priv->bacon),
					 volume);
}

static void
brasero_player_create_controls_stream (BraseroPlayer *player,
				       gboolean video)
{
	GtkWidget *box = NULL;
	GtkWidget *header_box;
	GtkWidget *alignment;
	GtkWidget *volume;

	if (player->priv->controls)
		brasero_player_destroy_controls (player);

	player->priv->controls = gtk_vbox_new (FALSE, 4);

	/* first line title */
	header_box = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    header_box,
			    FALSE,
			    FALSE,
			    0);
	
	player->priv->header = gtk_label_new (_("No file"));
	gtk_label_set_use_markup (GTK_LABEL (player->priv->header), TRUE);
	gtk_label_set_justify (GTK_LABEL (player->priv->header), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (player->priv->header), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (header_box),
			    player->priv->header,
			    TRUE,
			    TRUE,
			    0);

	player->priv->size = gtk_label_new (NULL);
	gtk_label_set_justify (GTK_LABEL (player->priv->size), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (player->priv->size), 1.0, 0.0);
	gtk_box_pack_start (GTK_BOX (header_box),
			    player->priv->size,
			    FALSE,
			    FALSE,
			    0);

	/* second line : play, progress, volume button */
	box = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    box,
			    FALSE,
			    FALSE,
			    0);

	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
	player->priv->button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (alignment), player->priv->button);
	gtk_box_pack_start (GTK_BOX (box),
			    alignment,
			    FALSE,
			    FALSE,
			    0);

	player->priv->image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (player->priv->button), player->priv->image);
	g_signal_connect (G_OBJECT (player->priv->button), "clicked",
			  G_CALLBACK (brasero_player_button_clicked_cb),
			  player);

	player->priv->progress = gtk_hscale_new_with_range (0, 1, 500000000);
	gtk_scale_set_digits (GTK_SCALE (player->priv->progress), 0);
	gtk_scale_set_draw_value (GTK_SCALE (player->priv->progress), FALSE);
	gtk_range_set_update_policy (GTK_RANGE (player->priv->progress), GTK_UPDATE_CONTINUOUS);
	gtk_box_pack_start (GTK_BOX (box),
			  player->priv->progress,
			  TRUE,
			  TRUE,
			  0);

	g_signal_connect (G_OBJECT (player->priv->progress),
			  "button-press-event",
			  G_CALLBACK (brasero_player_range_button_pressed_cb), player);
	g_signal_connect (G_OBJECT (player->priv->progress),
			  "button-release-event",
			  G_CALLBACK (brasero_player_range_button_released_cb), player);
	g_signal_connect (G_OBJECT (player->priv->progress),
			  "value-changed",
			  G_CALLBACK (brasero_player_range_value_changed),
			  player);

	volume = gtk_volume_button_new ();
	gtk_widget_show (volume);
	gtk_box_pack_start (GTK_BOX (box),
			    volume,
			    FALSE,
			    FALSE,
			    0);

	if (player->priv->bacon)
		gtk_scale_button_set_value (GTK_SCALE_BUTTON (volume),
					    brasero_player_bacon_get_volume (BRASERO_PLAYER_BACON (player->priv->bacon)));

	g_signal_connect (volume,
			  "value-changed",
			  G_CALLBACK (brasero_player_volume_changed_cb),
			  player);

	/* zoom in/out, only if video */
	if (video) {
		GtkWidget *image;
		GtkWidget *zoom;
		GtkWidget *hbox;

		box = gtk_hbox_new (FALSE, 12);
		gtk_box_pack_start (GTK_BOX (player->priv->controls),
				    box,
				    FALSE,
				    FALSE,
				    0);

		hbox = gtk_hbox_new (FALSE, 0);
		alignment = gtk_alignment_new (1.0, 0.0, 0.0, 0.0);
		player->priv->button = gtk_button_new ();
		gtk_container_add (GTK_CONTAINER (alignment), hbox);
		gtk_box_pack_start (GTK_BOX (box),
				    alignment,
				    TRUE,
				    TRUE,
				    0);

		image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_BUTTON);
		zoom = gtk_button_new ();
		gtk_button_set_image (GTK_BUTTON (zoom), image);
		gtk_button_set_relief (GTK_BUTTON (zoom), GTK_RELIEF_NONE);
		gtk_button_set_focus_on_click (GTK_BUTTON (zoom), FALSE);
		g_signal_connect (zoom,
				  "clicked",
				  G_CALLBACK (brasero_player_video_zoom_out),
				  player);
		gtk_box_pack_start (GTK_BOX (hbox),
				    zoom,
				    FALSE,
				    FALSE,
				    0);
		player->priv->video_zoom_out = zoom;

		image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_BUTTON);
		zoom = gtk_button_new ();
		gtk_button_set_image (GTK_BUTTON (zoom), image);
		gtk_button_set_relief (GTK_BUTTON (zoom), GTK_RELIEF_NONE);
		gtk_button_set_focus_on_click (GTK_BUTTON (zoom), FALSE);
		g_signal_connect (zoom,
				  "clicked",
				  G_CALLBACK (brasero_player_video_zoom_in),
				  player);
		gtk_box_pack_start (GTK_BOX (hbox),
				    zoom,
				    FALSE,
				    FALSE,
				    0);
		player->priv->video_zoom_in = zoom;

		if (player->priv->video_height <= PLAYER_BACON_HEIGHT
		||  player->priv->video_width  <= PLAYER_BACON_WIDTH)
			gtk_widget_set_sensitive (player->priv->video_zoom_out, FALSE);
		else
			gtk_widget_set_sensitive (player->priv->video_zoom_out, TRUE);

		if (player->priv->video_height >= PLAYER_BACON_HEIGHT * 3
		||  player->priv->video_width  >= PLAYER_BACON_WIDTH * 3)
			gtk_widget_set_sensitive (player->priv->video_zoom_in, FALSE);
		else
			gtk_widget_set_sensitive (player->priv->video_zoom_in, TRUE);

		if (player->priv->video_width >= (GTK_WIDGET (player)->allocation.width / 2)) {
			gtk_box_pack_start (GTK_BOX (player->priv->vbox),
					    player->priv->controls,
					    TRUE,
					    TRUE,
					    0);
			gtk_box_set_child_packing (GTK_BOX (player->priv->hbox),
						   player->priv->notebook->parent,
						   TRUE,
						   TRUE,
						   0,
						   GTK_PACK_START);
		}
		else if (player->priv->video_width < (GTK_WIDGET (player)->allocation.width / 2))
			gtk_box_pack_start (GTK_BOX (player->priv->hbox),
					    player->priv->controls,
					    TRUE,
					    TRUE,
					    0);
	}
	else
		gtk_box_pack_end (GTK_BOX (player->priv->hbox),
				  player->priv->controls,
				  TRUE,
				  TRUE,
				  0);

	gtk_widget_show_all (player->priv->controls);
	gtk_alignment_set_padding (GTK_ALIGNMENT (player), 12, 0, 0, 0);
}

static gboolean
brasero_player_scale_image (BraseroPlayer *player)
{
	gint height, width;
	GdkPixbuf *scaled;
	gdouble ratio;

	height = gdk_pixbuf_get_height (player->priv->pixbuf);
	width = gdk_pixbuf_get_width (player->priv->pixbuf);

	if (player->priv->image_height == height
	&&  player->priv->image_width == width) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (player->priv->image_display),
					   player->priv->pixbuf);
		return TRUE;
	}

	if (width / (gdouble) player->priv->image_width > height / (gdouble) player->priv->image_height)
		ratio = (gdouble) width / (gdouble) player->priv->image_width;
	else
		ratio = (gdouble) height / (gdouble) player->priv->image_height;

	scaled = gdk_pixbuf_scale_simple (player->priv->pixbuf,
					  width / ratio,
					  height / ratio,
					  GDK_INTERP_BILINEAR);

	if (!scaled)
		return FALSE;

	gtk_image_set_from_pixbuf (GTK_IMAGE (player->priv->image_display), scaled);
	g_object_unref (scaled);

	return TRUE;
}

static void
brasero_player_image_zoom_in (GtkButton *button,
			      BraseroPlayer *player)
{
	gtk_widget_set_sensitive (player->priv->image_zoom_out, TRUE);

	player->priv->image_width += PLAYER_BACON_WIDTH / 3;
	player->priv->image_height += PLAYER_BACON_HEIGHT / 3;

	if (player->priv->image_width >= PLAYER_BACON_WIDTH * 3 ||
	    player->priv->image_height >= PLAYER_BACON_HEIGHT * 3) {
		gtk_widget_set_sensitive (player->priv->image_zoom_in, FALSE);
		player->priv->image_width = PLAYER_BACON_WIDTH * 3;
		player->priv->image_height = PLAYER_BACON_HEIGHT * 3;
	}

	brasero_player_scale_image (player);
}

static void
brasero_player_image_zoom_out (GtkButton *button,
			       BraseroPlayer *player)
{
	gint min_height, min_width;

	gtk_widget_set_sensitive (player->priv->image_zoom_in, TRUE);

	min_width = MIN (PLAYER_BACON_WIDTH, gdk_pixbuf_get_width (player->priv->pixbuf));
	min_height = MIN (PLAYER_BACON_HEIGHT, gdk_pixbuf_get_height (player->priv->pixbuf));

	player->priv->image_width -= PLAYER_BACON_WIDTH / 3;
	player->priv->image_height -= PLAYER_BACON_HEIGHT / 3;

	/* the image itself */
	if (player->priv->image_width <= min_width ||
	    player->priv->image_height <= min_height) {
		gtk_widget_set_sensitive (player->priv->image_zoom_out, FALSE);
		player->priv->image_width = min_width;
		player->priv->image_height = min_height;
	}

	brasero_player_scale_image (player);
}

static void
brasero_player_create_controls_image (BraseroPlayer *player)
{
	GtkWidget *box, *zoom;
	GtkWidget *image;

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

	box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    box,
			    FALSE,
			    FALSE,
			    0);

	image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_BUTTON);
	zoom = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (zoom), image);
	gtk_button_set_relief (GTK_BUTTON (zoom), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (zoom), FALSE);
	g_signal_connect (zoom,
			  "clicked",
			  G_CALLBACK (brasero_player_image_zoom_out),
			  player);
	gtk_box_pack_start (GTK_BOX (box),
			    zoom,
			    FALSE,
			    FALSE,
			    0);
	player->priv->image_zoom_out = zoom;

	image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_BUTTON);
	zoom = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (zoom), image);
	gtk_button_set_relief (GTK_BUTTON (zoom), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (zoom), FALSE);
	g_signal_connect (zoom,
			  "clicked",
			  G_CALLBACK (brasero_player_image_zoom_in),
			  player);
	gtk_box_pack_start (GTK_BOX (box),
			    zoom,
			    FALSE,
			    FALSE,
			    0);
	player->priv->image_zoom_in = zoom;

	gtk_widget_show_all (player->priv->controls);
	gtk_alignment_set_padding (GTK_ALIGNMENT (player), 12, 0, 0, 0);
}

static void
brasero_player_image (BraseroPlayer *player)
{
	GError *error = NULL;
	gint width, height;
	gchar *string;
	gchar *path;
	gchar *name;

	if (player->priv->pixbuf) {
		g_object_unref (player->priv->pixbuf);
		player->priv->pixbuf = NULL;
	}

	/* image */
	/* FIXME: this does not allow to preview remote files */
	path = gnome_vfs_get_local_path_from_uri (player->priv->uri);
	player->priv->pixbuf = gdk_pixbuf_new_from_file (path, &error);

	if (!player->priv->pixbuf) {
		if (error) {
			g_warning ("Couldn't load image %s\n", error->message);
			g_error_free (error);
		}

		brasero_player_no_multimedia_stream (player);

		g_free (path);
		return;
	}

	height = gdk_pixbuf_get_height (player->priv->pixbuf);
	width = gdk_pixbuf_get_width (player->priv->pixbuf);

	brasero_player_scale_image (player);

	/* display information about the image */
	brasero_player_create_controls_image (player);

	if (player->priv->image_height <= MIN (PLAYER_BACON_HEIGHT, gdk_pixbuf_get_height (player->priv->pixbuf))
	||  player->priv->image_width  <= MIN (PLAYER_BACON_WIDTH, gdk_pixbuf_get_width (player->priv->pixbuf)))
		gtk_widget_set_sensitive (player->priv->image_zoom_out, FALSE);
	else
		gtk_widget_set_sensitive (player->priv->image_zoom_out, TRUE);

	if (player->priv->image_height >= PLAYER_BACON_HEIGHT * 3
	||  player->priv->image_width  >= PLAYER_BACON_WIDTH * 3)
		gtk_widget_set_sensitive (player->priv->image_zoom_in, FALSE);
	else
		gtk_widget_set_sensitive (player->priv->image_zoom_in, TRUE);

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
	gtk_widget_show (player->priv->notebook);
	gtk_alignment_set_padding (GTK_ALIGNMENT (player), 12, 0, 0, 0);

	g_signal_emit (player,
		       brasero_player_signals [READY_SIGNAL],
		       0);
}

static void
brasero_player_update_info_real (BraseroPlayer *player,
				 const gchar *artist,
				 const gchar *title)
{
	gchar *header;

	brasero_player_set_length (player);
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
brasero_player_metadata_completed (BraseroVFS *vfs,
				   GObject *obj,
				   GnomeVFSResult result,
				   const gchar *uri,
				   GnomeVFSFileInfo *info,
				   BraseroMetadataInfo *metadata,
				   gpointer null_data)
{
	BraseroPlayer *player = BRASERO_PLAYER (obj);

	if (player->priv->pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (player->priv->image_display), NULL);
		g_object_unref (player->priv->pixbuf);
		player->priv->pixbuf = NULL;
	}

	if (player->priv->controls)
		brasero_player_destroy_controls (player);

	if (result != GNOME_VFS_OK) {
		brasero_player_no_multimedia_stream (player);
		return;
	}

	/* based on the mime type, we try to determine the type of file */
	if (metadata && metadata->has_video) {
		/* video */
		brasero_player_create_controls_stream (player, TRUE);
		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0.0);

		if (metadata->is_seekable)
			gtk_widget_set_sensitive (player->priv->progress, TRUE);
		else
			gtk_widget_set_sensitive (player->priv->progress, FALSE);

		gtk_widget_show (player->priv->bacon);
		gtk_widget_hide (player->priv->image_display);
		gtk_widget_show (player->priv->notebook);
	}
	else if (metadata && metadata->has_audio) {
		/* audio */
		brasero_player_create_controls_stream (player, FALSE);
		gtk_widget_hide (player->priv->notebook);
		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0.0);

		if (metadata->is_seekable)
			gtk_widget_set_sensitive (player->priv->progress, TRUE);
		else
			gtk_widget_set_sensitive (player->priv->progress, FALSE);
	}
	else if (info && info->mime_type && !strncmp ("image/", info->mime_type, 6)) {
		brasero_player_image (player);
		return;
	}
	else {
		brasero_player_no_multimedia_stream (player);
		return;
	}

	if (player->priv->length <= 0)
		player->priv->length = metadata->len;

	/* only reached for audio/video */
	brasero_player_update_info_real (player,
					 metadata->artist,
					 metadata->title);

	player->priv->state = BACON_STATE_READY;

	g_signal_emit (player,
		       brasero_player_signals [READY_SIGNAL],
		       0);
}

static gboolean
brasero_player_set_uri_timeout (BraseroPlayer *player)
{
	GList *uris;

	if (!player->priv->vfs)
		player->priv->vfs = brasero_vfs_get_default ();

	if (!player->priv->meta_task)
		player->priv->meta_task = brasero_vfs_register_data_type (player->priv->vfs,
									  G_OBJECT (player),
									  G_CALLBACK (brasero_player_metadata_completed),
									  NULL);

	uris = g_list_prepend (NULL, player->priv->uri);
	brasero_vfs_get_metadata (player->priv->vfs,
				  uris,
				  GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
				  BRASERO_METADATA_FLAG_NONE,
				  FALSE,
				  player->priv->meta_task,
				  NULL);
	g_list_free (uris);

	player->priv->set_uri_id = 0;

	return FALSE;
}

const gchar *
brasero_player_get_uri (BraseroPlayer *player)
{
	return player->priv->uri;
}

void
brasero_player_set_uri (BraseroPlayer *player,
			const gchar *uri)
{
	gchar *uri_unescaped;
	GtkWidget *label;
	gchar *song_uri;

	/* avoid reloading everything if it's the same uri */
	if (uri && player->priv->uri
	&& !strcmp (uri, player->priv->uri)) {
		/* if it's not loaded yet just return */
		if (!player->priv->controls)
			return;

		/* just stop the pipeline and reset to 0 */
		if (player->priv->state == BACON_STATE_PLAYING)
			gtk_button_clicked (GTK_BUTTON (player->priv->button));

		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0);
		brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon),
					      player->priv->start);

		g_signal_emit (player,
			       brasero_player_signals [READY_SIGNAL],
			       0);
		return;
	}

	if (player->priv->uri)
		g_free (player->priv->uri);

	player->priv->uri = g_strdup (uri);
	player->priv->length = 0;

	if (player->priv->vfs)
		brasero_vfs_cancel (player->priv->vfs, player);

	if (player->priv->set_uri_id) {
		g_source_remove (player->priv->set_uri_id);
		player->priv->set_uri_id = 0;
	}

	brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (player->priv->bacon), NULL);
	brasero_player_no_multimedia_stream (player);
	brasero_player_destroy_controls (player);
	if (!uri)
		return;

	player->priv->controls = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (player->priv->controls);
	gtk_box_pack_end (GTK_BOX (player->priv->vbox),
			  player->priv->controls,
			  TRUE,
			  TRUE,
			  0);

	/* first line title */
	label = gtk_label_new (_("<span weight=\"bold\">loading ...</span>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    label,
			    TRUE,
			    TRUE,
			    0);

	uri_unescaped = gnome_vfs_unescape_string_for_display (uri);
	song_uri = g_strdup_printf ("<span size=\"smaller\"><i>%s</i></span>", uri_unescaped);
	g_free (uri_unescaped);
	label = gtk_label_new (song_uri);
	g_free (song_uri);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    label,
			    TRUE,
			    TRUE,
			    0);

	/* we add a timeout to wait a little since it could be the arrow keys
	 * which are pressed and in this case we can't keep on setting uris */
	player->priv->set_uri_id = g_timeout_add (400,
						  (GSourceFunc) brasero_player_set_uri_timeout,
						  player);
}

void
brasero_player_set_boundaries (BraseroPlayer *player, 
			       gint64 start,
			       gint64 end)
{
	if (player->priv->bacon)
		brasero_player_bacon_set_boundaries (BRASERO_PLAYER_BACON (player->priv->bacon),
						     start,
						     end);

	player->priv->length = end - start;
	player->priv->start = start;
	brasero_player_set_length (player);
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
	brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon), player->priv->start);
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
			gdouble pos;

			pos = gtk_range_get_value (GTK_RANGE (player->priv->progress));
			brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon), pos + player->priv->start);
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

gint64
brasero_player_get_length (BraseroPlayer *self)
{
	if (!self->priv->bacon)
		return -1;

	return self->priv->length;
}

gint64
brasero_player_get_pos (BraseroPlayer *self)
{
	gdouble pos;

	if (!self->priv->bacon)
		return -1;

	pos = gtk_range_get_value (GTK_RANGE (self->priv->progress));

	return pos;
}

static void
brasero_player_destroy (GtkObject *obj)
{
	BraseroPlayer *player;
	GConfClient *client;

	player = BRASERO_PLAYER (obj);

	client = gconf_client_get_default ();

	gconf_client_set_int (client,
			      GCONF_IMAGE_SIZE_WIDTH,
			      player->priv->image_width,
			      NULL);

	gconf_client_set_int (client,
			      GCONF_IMAGE_SIZE_HEIGHT,
			      player->priv->image_height,
			      NULL);

	gconf_client_set_int (client,
			      GCONF_VIDEO_SIZE_WIDTH,
			      player->priv->video_width,
			      NULL);

	gconf_client_set_int (client,
			      GCONF_VIDEO_SIZE_HEIGHT,
			      player->priv->video_height,
			      NULL);

	g_object_unref (client);

	player->priv->image = NULL;

	if (player->priv->pixbuf) {
		g_object_unref (player->priv->pixbuf);
		player->priv->pixbuf = NULL;
	}

	if (player->priv->update_scale_id) {
		g_source_remove (player->priv->update_scale_id);
		player->priv->update_scale_id = 0;
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

static void
brasero_player_class_init (BraseroPlayerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_player_finalize;
	gtk_object_class->destroy = brasero_player_destroy;

	brasero_player_signals [READY_SIGNAL] = 
			g_signal_new ("ready",
				      G_TYPE_FROM_CLASS (klass),
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (BraseroPlayerClass, ready),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
}

static void
brasero_player_init (BraseroPlayer *obj)
{
	GtkWidget *alignment;
	GConfClient *client;

	obj->priv = g_new0 (BraseroPlayerPrivate, 1);

	obj->priv->vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (obj->priv->vbox);
	gtk_container_set_border_width (GTK_CONTAINER (obj->priv->vbox), 0);
	gtk_container_add (GTK_CONTAINER (obj), obj->priv->vbox);

	obj->priv->hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (obj->priv->hbox);
	gtk_box_pack_start (GTK_BOX (obj->priv->vbox),
			    obj->priv->hbox,
			    TRUE,
			    TRUE,
			    0);

	alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (obj->priv->hbox),
			    alignment,
			    FALSE,
			    FALSE,
			    0);

	obj->priv->notebook = gtk_notebook_new ();
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->notebook);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj->priv->notebook), FALSE);

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

	client = gconf_client_get_default ();
	obj->priv->image_width = gconf_client_get_int (client,
						       GCONF_IMAGE_SIZE_WIDTH,
						       NULL);

	if (obj->priv->image_width > PLAYER_BACON_WIDTH * 3
	||  obj->priv->image_width < PLAYER_BACON_WIDTH)
		obj->priv->image_width = PLAYER_BACON_WIDTH;

	obj->priv->image_height = gconf_client_get_int (client,
						        GCONF_IMAGE_SIZE_HEIGHT,
						        NULL);

	if (obj->priv->image_height > PLAYER_BACON_HEIGHT * 3
	||  obj->priv->image_height < PLAYER_BACON_HEIGHT)
		obj->priv->image_height = PLAYER_BACON_HEIGHT;

	obj->priv->video_width = gconf_client_get_int (client,
						       GCONF_VIDEO_SIZE_WIDTH,
						       NULL);

	if (obj->priv->video_width > PLAYER_BACON_WIDTH * 3
	||  obj->priv->video_width < PLAYER_BACON_WIDTH)
		obj->priv->video_width = PLAYER_BACON_WIDTH;

	obj->priv->video_height = gconf_client_get_int (client,
							GCONF_VIDEO_SIZE_HEIGHT,
							NULL);

	if (obj->priv->video_height > PLAYER_BACON_HEIGHT * 3
	||  obj->priv->video_height < PLAYER_BACON_HEIGHT)
		obj->priv->video_height = PLAYER_BACON_HEIGHT;

	gtk_widget_set_size_request (obj->priv->bacon,
				     obj->priv->video_width,
				     obj->priv->video_height);
	g_object_unref (client);
}

GtkWidget *
brasero_player_new ()
{
	BraseroPlayer *obj;

	obj = BRASERO_PLAYER (g_object_new (BRASERO_TYPE_PLAYER, NULL));

	return GTK_WIDGET (obj);
}
