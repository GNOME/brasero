/***************************************************************************
*            player.c
*
*  lun mai 30 08:15:01 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"
#include "brasero-metadata.h"
#include "brasero-io.h"

#include "brasero-units.h"

#include "brasero-setting.h"

#include "brasero-player.h"
#include "brasero-player-bacon.h"
#include "brasero-utils.h"


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

	BraseroPlayerBaconState state;

	BraseroIOJobBase *meta_task;

	gchar *uri;
	gint64 start;
	gint64 end;
	gint64 length;
};

#define GCONF_IMAGE_SIZE_WIDTH	"/apps/brasero/display/image_width"
#define GCONF_IMAGE_SIZE_HEIGHT	"/apps/brasero/display/image_height"
#define GCONF_VIDEO_SIZE_WIDTH	"/apps/brasero/display/video_width"
#define GCONF_VIDEO_SIZE_HEIGHT	"/apps/brasero/display/video_height"

typedef enum {
	READY_SIGNAL,
	ERROR_SIGNAL,
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
	player->priv->video_zoom_in = NULL;
	player->priv->video_zoom_out = NULL;
	player->priv->image_zoom_in = NULL;
	player->priv->image_zoom_out = NULL;
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

	player->priv->length = 0;
	player->priv->start = 0;
	player->priv->end = 0;
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
	len_string = brasero_units_get_time_string (player->priv->end - player->priv->start, FALSE, FALSE);

	value = gtk_range_get_value (GTK_RANGE (player->priv->progress));
	pos_string = brasero_units_get_time_string (value, FALSE, FALSE);

	/**
	 * Translators: this is the position being played in the stream. The 
	 * first %s is the position and the second %s is the whole length of
	 * the stream. I chose to make that translatable in case some languages
	 * don't allow the "/" */
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
		brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (player->priv->bacon), (gint64) pos + player->priv->start);
	}

	brasero_player_update_position (player);
}

static void
brasero_player_set_length (BraseroPlayer *player)
{
	if (player->priv->progress && player->priv->end - player->priv->start != 0)
		gtk_range_set_range (GTK_RANGE (player->priv->progress),
				     0.0,
				     (gdouble) player->priv->end - player->priv->start);

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
	if (GTK_WIDGET (player)->allocation.width > GTK_WIDGET (player)->allocation.height) {
		gtk_label_set_justify (GTK_LABEL (player->priv->size), GTK_JUSTIFY_RIGHT);
		gtk_misc_set_alignment (GTK_MISC (player->priv->size), 1.0, 0.0);

		gtk_box_pack_start (GTK_BOX (header_box),
				    player->priv->size,
				    FALSE,
				    FALSE,
				    0);
	}
	else {
		gtk_label_set_justify (GTK_LABEL (player->priv->size), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment (GTK_MISC (player->priv->size), 0.0, 0.0);

		gtk_box_pack_start (GTK_BOX (player->priv->controls),
				    player->priv->size,
				    FALSE,
				    FALSE,
				    0);
	}
	
	/* second line : play, progress, volume button */
	box = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (player->priv->controls),
			    box,
			    FALSE,
			    FALSE,
			    0);

	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
	player->priv->button = gtk_button_new ();
	gtk_widget_set_tooltip_text (player->priv->button, _("Start and stop playing"));
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
	gtk_widget_set_size_request (player->priv->progress, 80, -1);
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

	if (player->priv->image_display)
		gtk_widget_set_sensitive (player->priv->image_display, TRUE);

	player->priv->controls = gtk_vbox_new (FALSE, 4);

	if (GTK_WIDGET (player)->allocation.width > GTK_WIDGET (player)->allocation.height)
		gtk_box_pack_end (GTK_BOX (player->priv->hbox),
				  player->priv->controls,
				  TRUE,
				  TRUE,
				  0);
	else
		gtk_box_pack_end (GTK_BOX (player->priv->vbox),
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
	path = g_filename_from_uri (player->priv->uri, NULL, NULL);
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

	string = g_strdup_printf ("<span weight=\"bold\">%s</span>\t %s",
				  _("Name:"),
				  name);
	g_free (name);

	gtk_label_set_markup (GTK_LABEL (player->priv->header), string);
	g_free (string);

	string = g_strdup_printf (_("<span weight=\"bold\">Size:</span>\t<i><span size=\"smaller\"> %i \303\227 %i pixels</span></i>"), width, height);
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
		header = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>\n", title);
		gtk_label_set_ellipsize (GTK_LABEL (player->priv->header),
					 PANGO_ELLIPSIZE_END);
	}
	else {
		gchar *name;

	    	BRASERO_GET_BASENAME_FOR_DISPLAY (player->priv->uri, name);
		header = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>\n", name);
		g_free (name);
		gtk_label_set_ellipsize (GTK_LABEL (player->priv->header),
					 PANGO_ELLIPSIZE_END);
	}

	gtk_label_set_markup (GTK_LABEL (player->priv->header), header);
	g_free (header);
}

static void
brasero_player_metadata_completed (GObject *obj,
				   GError *error,
				   const gchar *uri,
				   GFileInfo *info,
				   gpointer null_data)
{
	BraseroPlayer *player = BRASERO_PLAYER (obj);
	const gchar *mime;

	if (player->priv->pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (player->priv->image_display), NULL);
		g_object_unref (player->priv->pixbuf);
		player->priv->pixbuf = NULL;
	}

	if (player->priv->controls)
		brasero_player_destroy_controls (player);

	if (error) {
		brasero_player_no_multimedia_stream (player);
		g_signal_emit (player,
			       brasero_player_signals [ERROR_SIGNAL],
			       0);
		return;
	}

	if (g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN) <= 0) {
		brasero_player_no_multimedia_stream (player);
		g_signal_emit (player,
			       brasero_player_signals [ERROR_SIGNAL],
			       0);
		return;
	}

	mime = g_file_info_get_content_type (info);

	/* based on the mime type, we try to determine the type of file */
	if (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)) {
		/* video */
		brasero_player_create_controls_stream (player, TRUE);
		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0.0);

		if (g_file_info_get_attribute_boolean (info, BRASERO_IO_IS_SEEKABLE))
			gtk_widget_set_sensitive (player->priv->progress, TRUE);
		else
			gtk_widget_set_sensitive (player->priv->progress, FALSE);

		gtk_widget_show (player->priv->bacon);
		gtk_widget_hide (player->priv->image_display);
		gtk_widget_show (player->priv->notebook);
	}
	else if (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_AUDIO)) {
		/* audio */
		brasero_player_create_controls_stream (player, FALSE);
		gtk_widget_hide (player->priv->notebook);
		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0.0);

		if (g_file_info_get_attribute_boolean (info, BRASERO_IO_IS_SEEKABLE))
			gtk_widget_set_sensitive (player->priv->progress, TRUE);
		else
			gtk_widget_set_sensitive (player->priv->progress, FALSE);
	}
	else if (mime && !strncmp ("image/", mime, 6)) {
		/* Only do that if the image is < 20 M otherwise that's crap
		 * FIXME: maybe a sort of error message here? or use thumbnail? */
		if (g_file_info_get_size (info) > 100000000) {
			brasero_player_no_multimedia_stream (player);
			g_signal_emit (player,
				       brasero_player_signals [ERROR_SIGNAL],
				       0);
		}
		else
			brasero_player_image (player);

		return;
	}
	else {
		brasero_player_no_multimedia_stream (player);
		g_signal_emit (player,
			       brasero_player_signals [ERROR_SIGNAL],
			       0);
	       return;
	}

	if (player->priv->end <= 0)
		player->priv->end = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);

	player->priv->length = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);

	/* only reached for audio/video */
	brasero_player_update_info_real (player,
					 g_file_info_get_attribute_string (info, BRASERO_IO_ARTIST),
					 g_file_info_get_attribute_string (info, BRASERO_IO_TITLE));

	player->priv->state = BACON_STATE_READY;
	g_signal_emit (player,
		       brasero_player_signals [READY_SIGNAL],
		       0);
}

static void
brasero_player_retrieve_metadata (BraseroPlayer *player)
{
	if (!player->priv->meta_task)
		player->priv->meta_task = brasero_io_register (G_OBJECT (player),
							       brasero_player_metadata_completed,
							       NULL,
							       NULL);

	brasero_io_get_file_info (player->priv->uri,
				  player->priv->meta_task,
				  BRASERO_IO_INFO_METADATA|
				  BRASERO_IO_INFO_MIME,
				  NULL);
}

const gchar *
brasero_player_get_uri (BraseroPlayer *player)
{
	return player->priv->uri;
}

void
brasero_player_set_boundaries (BraseroPlayer *player, 
			       gint64 start,
			       gint64 end)
{
	if (start <= 0)
		player->priv->start = 0;
	else
		player->priv->start = start;

	if (end <= 0)
		player->priv->end = player->priv->length;
	else
		player->priv->end = end;

	if (player->priv->progress) {
		brasero_player_set_length (player);
		gtk_range_set_value (GTK_RANGE (player->priv->progress), 0);
	}

	if (player->priv->bacon)
		brasero_player_bacon_set_boundaries (BRASERO_PLAYER_BACON (player->priv->bacon),
						     player->priv->start,
						     player->priv->end);
}

void
brasero_player_set_uri (BraseroPlayer *player,
			const gchar *uri)
{
	gchar *name;

	/* avoid reloading everything if it's the same uri */
	if (uri && player->priv->uri
	&& !strcmp (uri, player->priv->uri)) {
		/* if it's not loaded yet just return */
		if (!player->priv->controls)
			return;

		if (player->priv->progress) {
			brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (player->priv->bacon), uri);
			brasero_player_set_boundaries (player, -1, -1);

			/* the existence of progress is the surest way to know
			 * if that uri was successfully loaded */
			g_signal_emit (player,
				       brasero_player_signals [READY_SIGNAL],
				       0);
		}

		return;
	}

	if (player->priv->uri)
		g_free (player->priv->uri);

	player->priv->uri = g_strdup (uri);
	player->priv->length = 0;
	player->priv->start = 0;
	player->priv->end = 0;

	if (player->priv->meta_task)
		brasero_io_cancel_by_base (player->priv->meta_task);

	/* That stops the pipeline from playing */
	brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (player->priv->bacon), NULL);

	if (!uri) {
		brasero_player_no_multimedia_stream (player);
		brasero_player_destroy_controls (player);
		return;
	}

	if (player->priv->controls) {
		if (player->priv->header) {
			gchar *song_uri;

			BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
			song_uri = g_markup_printf_escaped (_("<span weight=\"bold\">Loading information</span>\nabout <span size=\"smaller\"><i>%s</i></span>"),
							  name);
			g_free (name);

			gtk_label_set_markup (GTK_LABEL (player->priv->header), song_uri);
			g_free (song_uri);
		}

		/* grey out the rest of the control while it's loading */
		if (player->priv->progress) {
			gtk_widget_set_sensitive (player->priv->progress, FALSE);
			gtk_range_set_value (GTK_RANGE (player->priv->progress), 0);
		}

		if (player->priv->size)
			gtk_label_set_text (GTK_LABEL (player->priv->size), NULL);

		if (player->priv->button)
			gtk_widget_set_sensitive (player->priv->button, FALSE);

		if (player->priv->image) {
			gtk_image_set_from_stock (GTK_IMAGE (player->priv->image),
						  GTK_STOCK_MEDIA_PLAY,
						  GTK_ICON_SIZE_BUTTON);
		}

		if (player->priv->image_zoom_in)
			gtk_widget_set_sensitive (player->priv->image_zoom_in, FALSE);

		if (player->priv->image_zoom_out)
			gtk_widget_set_sensitive (player->priv->image_zoom_out, FALSE);

		if (player->priv->video_zoom_in)
			gtk_widget_set_sensitive (player->priv->video_zoom_in, FALSE);

		if (player->priv->video_zoom_out)
			gtk_widget_set_sensitive (player->priv->video_zoom_out, FALSE);

		if (player->priv->image_display)
			gtk_widget_set_sensitive (player->priv->image_display, FALSE);
	}

	brasero_player_retrieve_metadata (player);
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

	return self->priv->end - self->priv->start;
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
brasero_player_size_allocate (GtkWidget *widget,
			      GtkAllocation *allocation)
{
	BraseroPlayer *player;
	GtkWidget *parent;

	player = BRASERO_PLAYER (widget);
	if (!player->priv->controls) {
		GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
		return;
	}

	if (!player->priv->pixbuf) {
		GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
		return;
	}

	parent = gtk_widget_get_parent (player->priv->controls);

	g_object_ref (player->priv->controls);

	if (allocation->width > allocation->height) {
		if (parent != player->priv->hbox) {
			gtk_container_remove (GTK_CONTAINER (player->priv->vbox), player->priv->controls);
			gtk_box_pack_end (GTK_BOX (player->priv->hbox),
					  player->priv->controls,
					  TRUE,
					  TRUE,
					  0);
		}
	}
	else {
		if (parent != player->priv->vbox) {
			gtk_container_remove (GTK_CONTAINER (player->priv->hbox), player->priv->controls);
			gtk_box_pack_end (GTK_BOX (player->priv->vbox),
					  player->priv->controls,
					  TRUE,
					  TRUE,
					  0);
		}
	}

	g_object_unref (player->priv->controls);
	gtk_widget_show (player->priv->controls);

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
}

static void
brasero_player_destroy (GtkObject *obj)
{
	BraseroPlayer *player;

	player = BRASERO_PLAYER (obj);

	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_WIDTH,
	                           GINT_TO_POINTER (player->priv->image_width));
	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_HEIGHT,
	                           GINT_TO_POINTER (player->priv->image_height));
	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_WIDTH,
	                           GINT_TO_POINTER (player->priv->video_width));
	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_WIDTH,
	                           GINT_TO_POINTER (player->priv->video_width));

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

	if (player->priv->meta_task){
		brasero_io_cancel_by_base (player->priv->meta_task);
		g_free (player->priv->meta_task);
		player->priv->meta_task = 0;
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
	GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_player_finalize;

	gtk_object_class->destroy = brasero_player_destroy;

	gtk_widget_class->size_allocate = brasero_player_size_allocate;

	brasero_player_signals [ERROR_SIGNAL] = 
			g_signal_new ("error",
				      G_TYPE_FROM_CLASS (klass),
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (BraseroPlayerClass, error),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
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
	gpointer value;

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
	gtk_container_set_border_width (GTK_CONTAINER (obj->priv->notebook), 6);
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

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_WIDTH,
	                           &value);
	obj->priv->image_width = GPOINTER_TO_INT (value);

	if (obj->priv->image_width > PLAYER_BACON_WIDTH * 3
	||  obj->priv->image_width < PLAYER_BACON_WIDTH)
		obj->priv->image_width = PLAYER_BACON_WIDTH;

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_HEIGHT,
	                           &value);
	obj->priv->image_height = GPOINTER_TO_INT (value);

	if (obj->priv->image_height > PLAYER_BACON_HEIGHT * 3
	||  obj->priv->image_height < PLAYER_BACON_HEIGHT)
		obj->priv->image_height = PLAYER_BACON_HEIGHT;

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_WIDTH,
	                           &value);
	obj->priv->video_width = GPOINTER_TO_INT (value);

	if (obj->priv->video_width > PLAYER_BACON_WIDTH * 3
	||  obj->priv->video_width < PLAYER_BACON_WIDTH)
		obj->priv->video_width = PLAYER_BACON_WIDTH;

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_HEIGHT,
	                           &value);
	obj->priv->video_height = GPOINTER_TO_INT (value);

	if (obj->priv->video_height > PLAYER_BACON_HEIGHT * 3
	||  obj->priv->video_height < PLAYER_BACON_HEIGHT)
		obj->priv->video_height = PLAYER_BACON_HEIGHT;

	gtk_widget_set_size_request (obj->priv->bacon,
				     obj->priv->video_width,
				     obj->priv->video_height);
}

GtkWidget *
brasero_player_new ()
{
	BraseroPlayer *obj;

	obj = BRASERO_PLAYER (g_object_new (BRASERO_TYPE_PLAYER, NULL));

	return GTK_WIDGET (obj);
}
