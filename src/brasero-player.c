/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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

typedef struct _BraseroPlayerPrivate BraseroPlayerPrivate;

struct _BraseroPlayerPrivate {
	GtkWidget *notebook;

	GtkWidget *bacon;

	GtkWidget *spinner;

	GtkWidget *play_image;

	gint image_width;
	gint image_height;
	GdkPixbuf *pixbuf;

	GtkWidget *image_display;

	GtkWidget *zoom_in;
	GtkWidget *zoom_out;

	gint video_width;
	gint video_height;

	BraseroPlayerBaconState state;

	BraseroIOJobBase *meta_task;

	gchar *uri;
	gint64 start;
	gint64 end;
	gint64 length;

	int is_video:1;
};

typedef enum {
	READY_SIGNAL,
	ERROR_SIGNAL,
	EOF_SIGNAL,
	LAST_SIGNAL
} BraseroPlayerSignalType;
static guint brasero_player_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (BraseroPlayer, brasero_player, GTK_TYPE_ALIGNMENT);

static void
brasero_player_no_multimedia_stream (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	gtk_spinner_stop (GTK_SPINNER (priv->spinner));
	gtk_widget_set_has_tooltip (GTK_WIDGET (player), FALSE);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 2);

	priv->length = 0;
	priv->start = 0;
	priv->end = 0;
}

static void
brasero_player_video_zoom_out (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	priv->video_width -= PLAYER_BACON_WIDTH / 3;
	priv->video_height -= PLAYER_BACON_HEIGHT / 3;

	if (priv->video_width <= PLAYER_BACON_WIDTH ||
	    priv->video_height <= PLAYER_BACON_HEIGHT) {
		priv->video_width = PLAYER_BACON_WIDTH;
		priv->video_height = PLAYER_BACON_HEIGHT;
	}

	gtk_widget_set_size_request (GTK_WIDGET (priv->bacon),
				     priv->video_width,
				     priv->video_height);
}

static void
brasero_player_video_zoom_in (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	priv->video_width += PLAYER_BACON_WIDTH / 3;
	priv->video_height += PLAYER_BACON_HEIGHT / 3;

	if (priv->video_width >= PLAYER_BACON_WIDTH * 3 ||
	    priv->video_height >= PLAYER_BACON_HEIGHT * 3) {
		priv->video_width = PLAYER_BACON_WIDTH * 3;
		priv->video_height = PLAYER_BACON_HEIGHT * 3;
	}

	gtk_widget_set_size_request (GTK_WIDGET (priv->bacon),
				     priv->video_width,
				     priv->video_height);
}

static gboolean
brasero_player_scale_image (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);
	gint height, width;
	GdkPixbuf *scaled;
	gdouble ratio;

	height = gdk_pixbuf_get_height (priv->pixbuf);
	width = gdk_pixbuf_get_width (priv->pixbuf);

	if (priv->image_height == height
	&&  priv->image_width == width) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image_display),
					   priv->pixbuf);
		return TRUE;
	}

	if (width / (gdouble) priv->image_width > height / (gdouble) priv->image_height)
		ratio = (gdouble) width / (gdouble) priv->image_width;
	else
		ratio = (gdouble) height / (gdouble) priv->image_height;

	scaled = gdk_pixbuf_scale_simple (priv->pixbuf,
					  width / ratio,
					  height / ratio,
					  GDK_INTERP_BILINEAR);

	if (!scaled)
		return FALSE;

	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image_display), scaled);
	g_object_unref (scaled);

	return TRUE;
}

static void
brasero_player_image_zoom_in (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	priv->image_width += PLAYER_BACON_WIDTH / 3;
	priv->image_height += PLAYER_BACON_HEIGHT / 3;

	if (priv->image_width >= PLAYER_BACON_WIDTH * 3 ||
	    priv->image_height >= PLAYER_BACON_HEIGHT * 3) {
		priv->image_width = PLAYER_BACON_WIDTH * 3;
		priv->image_height = PLAYER_BACON_HEIGHT * 3;
	}

	if (priv->pixbuf)
		brasero_player_scale_image (player);
}

static void
brasero_player_image_zoom_out (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);
	gint min_height, min_width;

	if (priv->pixbuf) {
		min_width = MIN (PLAYER_BACON_WIDTH, gdk_pixbuf_get_width (priv->pixbuf));
		min_height = MIN (PLAYER_BACON_HEIGHT, gdk_pixbuf_get_height (priv->pixbuf));
	}
	else {
		min_width = PLAYER_BACON_WIDTH;
		min_height = PLAYER_BACON_HEIGHT;
	}

	priv->image_width -= PLAYER_BACON_WIDTH / 3;
	priv->image_height -= PLAYER_BACON_HEIGHT / 3;

	/* the image itself */
	if (priv->image_width <= min_width ||
	    priv->image_height <= min_height) {
		priv->image_width = min_width;
		priv->image_height = min_height;
	}

	if (priv->pixbuf)
		brasero_player_scale_image (player);
}

static void
brasero_player_zoom_in_cb (GtkButton *button,
                           BraseroPlayer *player)
{
	brasero_player_image_zoom_in (player);
	brasero_player_video_zoom_in (player);
}

static void
brasero_player_zoom_out_cb (GtkButton *button,
                            BraseroPlayer *player)
{
	brasero_player_image_zoom_out (player);
	brasero_player_video_zoom_out (player);
}

static gboolean
brasero_bacon_scroll (BraseroPlayerBacon *bacon,
                      GdkEventScroll *event,
                      BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook))) {
	case 1:
	case 4:
		if (event->direction == GDK_SCROLL_UP)
			brasero_player_bacon_forward (bacon, GST_SECOND);
		else
			brasero_player_bacon_backward (bacon, GST_SECOND);
		break;

	case 0:
	case 2:
	case 3:
	default:
		if (event->direction == GDK_SCROLL_UP) {
			brasero_player_image_zoom_in (player);
			brasero_player_video_zoom_in (player);
		}
		else {
			brasero_player_image_zoom_out (player);
			brasero_player_video_zoom_out (player);
		}
		break;
	}

	return TRUE;
}

static gboolean
brasero_bacon_button_release (BraseroPlayerBacon *bacon,
                              GdkEventButton *event,
                              BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	if (event->button != 1)
		return FALSE;

	if (priv->state == BACON_STATE_READY) {
		/* This will probably never happen as we display a play button */
		gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_DIALOG);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
		brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (priv->bacon), priv->uri);
		brasero_player_bacon_play (BRASERO_PLAYER_BACON (priv->bacon));
	}
	else if (priv->state == BACON_STATE_PAUSED) {
		gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_DIALOG);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
		brasero_player_bacon_play (BRASERO_PLAYER_BACON (priv->bacon));
	}
	else if (priv->state == BACON_STATE_PLAYING) {
		gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_DIALOG);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 4);
		brasero_player_bacon_stop (BRASERO_PLAYER_BACON (priv->bacon));
	}

	return TRUE;
}

static gboolean
brasero_player_button_release (GtkWidget *widget,
                               GdkEventButton *event,
                               BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	if (event->button != 1)
		return FALSE;

	if (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) == 4) {
		if (priv->state == BACON_STATE_READY) {
			gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_DIALOG);
			if (priv->is_video)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);

			brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (priv->bacon), priv->uri);
			brasero_player_bacon_play (BRASERO_PLAYER_BACON (priv->bacon));
		}
		else if (priv->state == BACON_STATE_PAUSED) {
			gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_DIALOG);
			if (priv->is_video)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);

			brasero_player_bacon_play (BRASERO_PLAYER_BACON (priv->bacon));
		}
		else if (priv->state == BACON_STATE_PLAYING) {
			gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_DIALOG);
			brasero_player_bacon_stop (BRASERO_PLAYER_BACON (priv->bacon));
		}
	}

	return TRUE;
}

static gboolean
brasero_player_scroll (GtkWidget *widget,
                       GdkEventScroll *event,
                       BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook))) {
	case 1:
	case 4:
		if (event->direction == GDK_SCROLL_UP)
			brasero_player_bacon_forward (BRASERO_PLAYER_BACON (priv->bacon), GST_SECOND);
		else
			brasero_player_bacon_backward (BRASERO_PLAYER_BACON (priv->bacon), GST_SECOND);
		break;

	case 0:
	case 2:
	case 3:
	default:
		if (event->direction == GDK_SCROLL_UP) {
			brasero_player_image_zoom_in (player);
			brasero_player_video_zoom_in (player);
		}
		else {
			brasero_player_image_zoom_out (player);
			brasero_player_video_zoom_out (player);
		}
		break;
	}

	return TRUE;
}

static void
brasero_player_image (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);
	GError *error = NULL;
	gchar *path;

	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	/* image */
	/* FIXME: this does not allow to preview remote files */
	path = g_filename_from_uri (priv->uri, NULL, NULL);
	priv->pixbuf = gdk_pixbuf_new_from_file (path, &error);

	if (!priv->pixbuf) {
		if (error) {
			g_warning ("Couldn't load image %s\n", error->message);
			g_error_free (error);
		}

		brasero_player_no_multimedia_stream (player);
		g_free (path);
		return;
	}
	
	brasero_player_scale_image (player);

	gtk_widget_show (priv->notebook);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);
	g_signal_emit (player,
		       brasero_player_signals [READY_SIGNAL],
		       0);
}

static void
brasero_player_update_tooltip (BraseroPlayer *player,
                               GFileInfo *info)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);
	gchar *string;
	gchar *len_string;
	const gchar *title;
	const gchar *artist;

	/* Update the tooltip */
	len_string = brasero_units_get_time_string (g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN), TRUE, FALSE);
	title = g_file_info_get_attribute_string (info, BRASERO_IO_TITLE);
	artist = g_file_info_get_attribute_string (info, BRASERO_IO_ARTIST);
	if (artist) {
		gchar *artist_string;

		/* Translators: %s is the name of the artist */
		artist_string = g_strdup_printf (_("by %s"), artist);
		string = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>"
		                                  "\n<i><span size=\"smaller\">%s</span></i>"
		                                  "\n%s",
		                                  title,
		                                  artist_string,
		                                  len_string);
		g_free (artist_string);
	}
	else if (title)
		string = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>"
		                                  "\n%s",
		                                  title,
		                                  len_string);
	else {
		gchar *name;
		gchar *unescaped_uri;

		unescaped_uri = g_uri_unescape_string (priv->uri, NULL);
		name = g_path_get_basename (unescaped_uri);
		g_free (unescaped_uri);
		string = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>"
		                                  "\n%s",
		                                  name,
		                                  len_string);
		g_free (name);
	}

	g_free (len_string);

	gtk_widget_set_tooltip_markup (GTK_WIDGET (player), string);
	g_free (string);
}

static void
brasero_player_metadata_completed (GObject *obj,
				   GError *error,
				   const gchar *uri,
				   GFileInfo *info,
				   gpointer null_data)
{
	BraseroPlayer *player = BRASERO_PLAYER (obj);
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);
	const gchar *mime;

	gtk_spinner_stop (GTK_SPINNER (priv->spinner));

	if (priv->pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image_display), NULL);
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	if (error) {
		brasero_player_no_multimedia_stream (player);
		g_signal_emit (player,
			       brasero_player_signals [ERROR_SIGNAL],
			       0);
		return;
	}

	mime = g_file_info_get_content_type (info);

	/* based on the mime type, we try to determine the type of file */
	if (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)) {
		if (g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN) <= 0) {
			brasero_player_no_multimedia_stream (player);
			g_signal_emit (player,
				       brasero_player_signals [ERROR_SIGNAL],
				       0);
			return;
		}

		/* video: display play button first */
		priv->is_video = TRUE;
		gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_DIALOG);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 4);

		brasero_player_update_tooltip (player, info);
	}
	else if (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_AUDIO)) {
		if (g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN) <= 0) {
			brasero_player_no_multimedia_stream (player);
			g_signal_emit (player,
				       brasero_player_signals [ERROR_SIGNAL],
				       0);
			return;
		}

		/* Audio */
		priv->is_video = FALSE;
		gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_DIALOG);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 4);

		brasero_player_update_tooltip (player, info);
	}
	else if (mime && !strncmp ("image/", mime, 6)) {
		gchar *size_string;
		gchar *string;
		gchar *path;
		gchar *name;
		gint height;
		gint width;

		/* Only do that if the image is < 20 M otherwise that's crap
		 * FIXME: maybe a sort of error message here? or use thumbnail? */
		if (g_file_info_get_size (info) > 100000000LL) {
			brasero_player_no_multimedia_stream (player);
			g_signal_emit (player,
				       brasero_player_signals [ERROR_SIGNAL],
				       0);
			return;
		}

		brasero_player_image (player);

		path = g_filename_from_uri (priv->uri, NULL, NULL);
		BRASERO_GET_BASENAME_FOR_DISPLAY (path, name);
		g_free (path);

		height = gdk_pixbuf_get_height (priv->pixbuf);
		width = gdk_pixbuf_get_width (priv->pixbuf);
		size_string = g_strdup_printf (_("%i \303\227 %i pixels"), width, height);

		string = g_strdup_printf ("<span weight=\"bold\">%s</span>\n"
		                          "<i><span size=\"smaller\">%s</span></i>",
		                          name,
		                          size_string);
		g_free (name);
		g_free (size_string);

		gtk_widget_set_tooltip_markup (GTK_WIDGET (player), string);
		g_free (string);
		return;
	}
	else {
		brasero_player_no_multimedia_stream (player);
		g_signal_emit (player,
			       brasero_player_signals [ERROR_SIGNAL],
			       0);
	       return;
	}

	if (priv->end <= 0)
		priv->end = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);

	priv->length = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);

	priv->state = BACON_STATE_READY;
	g_signal_emit (player,
		       brasero_player_signals [READY_SIGNAL],
		       0);
}

static void
brasero_player_retrieve_metadata (BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	if (!priv->meta_task)
		priv->meta_task = brasero_io_register (G_OBJECT (player),
						       brasero_player_metadata_completed,
						       NULL,
						       NULL);

	brasero_io_get_file_info (priv->uri,
				  priv->meta_task,
				  BRASERO_IO_INFO_METADATA|
				  BRASERO_IO_INFO_MIME,
				  NULL);
}

void
brasero_player_set_boundaries (BraseroPlayer *player, 
			       gint64 start,
			       gint64 end)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	if (start <= 0)
		priv->start = 0;
	else
		priv->start = start;

	if (end <= 0)
		priv->end = priv->length;
	else
		priv->end = end;

	if (priv->bacon)
		brasero_player_bacon_set_boundaries (BRASERO_PLAYER_BACON (priv->bacon),
						     priv->start,
						     priv->end);
}

void
brasero_player_set_uri (BraseroPlayer *player,
			const gchar *uri)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	/* avoid reloading everything if it's the same uri */
	if (!g_strcmp0 (uri, priv->uri)) {
		/* if it's not loaded yet just return */
		/* the existence of progress is the surest way to know
		 * if that uri was successfully loaded */
		if (uri)
			g_signal_emit (player,
				       brasero_player_signals [READY_SIGNAL],
				       0);
		else
			g_signal_emit (player,
				       brasero_player_signals [ERROR_SIGNAL],
				       0);
		return;
	}

	if (priv->uri)
		g_free (priv->uri);

	priv->uri = g_strdup (uri);
	priv->length = 0;
	priv->start = 0;
	priv->end = 0;

	if (priv->meta_task)
		brasero_io_cancel_by_base (priv->meta_task);

	/* That stops the pipeline from playing */
	brasero_player_bacon_set_uri (BRASERO_PLAYER_BACON (priv->bacon), NULL);
	if (!uri) {
		brasero_player_no_multimedia_stream (player);
		return;
	}

	gtk_widget_set_has_tooltip (GTK_WIDGET (player), FALSE);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 3);
	gtk_spinner_start (GTK_SPINNER (priv->spinner));

	brasero_player_retrieve_metadata (player);
}

static void
brasero_player_eof_cb (BraseroPlayerBacon *bacon, BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	brasero_player_bacon_stop (BRASERO_PLAYER_BACON (priv->bacon));
	brasero_player_bacon_set_pos (BRASERO_PLAYER_BACON (priv->bacon), priv->start);
	priv->state = BACON_STATE_PAUSED;
	gtk_image_set_from_stock (GTK_IMAGE (priv->play_image), GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_DIALOG);
}

static void
brasero_player_state_changed_cb (BraseroPlayerBacon *bacon,
				 BraseroPlayerBaconState state,
				 BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	if (priv->state == state)
		return;

	switch (state) {
	case BACON_STATE_ERROR:
		brasero_player_no_multimedia_stream (player);
		break;

	default:
		break;
	}

	priv->state = state;
}

static void
brasero_player_destroy (GtkWidget *obj)
{
	BraseroPlayer *player = BRASERO_PLAYER (obj);
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_WIDTH,
	                           GINT_TO_POINTER (priv->image_width));
	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_HEIGHT,
	                           GINT_TO_POINTER (priv->image_height));
	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_WIDTH,
	                           GINT_TO_POINTER (priv->video_width));
	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_HEIGHT,
	                           GINT_TO_POINTER (priv->video_height));

	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	if (priv->meta_task){
		brasero_io_cancel_by_base (priv->meta_task);
		brasero_io_job_base_free (priv->meta_task);
		priv->meta_task = 0;
	}

	if (GTK_WIDGET_CLASS (brasero_player_parent_class)->destroy)
		GTK_WIDGET_CLASS (brasero_player_parent_class)->destroy (obj);
}

static void
brasero_player_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_player_parent_class)->finalize (object);
}

static void
brasero_player_class_init (BraseroPlayerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = brasero_player_finalize;

	widget_class->destroy = brasero_player_destroy;

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
brasero_player_volume_changed_cb (GtkScaleButton *button,
				  gdouble volume,
				  BraseroPlayer *player)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (player);

	brasero_player_bacon_set_volume (BRASERO_PLAYER_BACON (priv->bacon), volume);
}

static void
brasero_player_init (BraseroPlayer *obj)
{
	BraseroPlayerPrivate *priv = brasero_player_get_instance_private (obj);
	GtkWidget *volume;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *event;
	GtkWidget *zoom;
	GtkWidget *hbox;
	GtkWidget *vbox;
	gpointer value;
	gchar *string;

	gtk_alignment_set (GTK_ALIGNMENT (obj), 0.5, 0.5, 0.0, 0.0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (obj), vbox);
	
	/* The notebook and all views */
	event = gtk_event_box_new ();
	gtk_box_pack_start (GTK_BOX (vbox), event, FALSE, FALSE, 0);
	gtk_event_box_set_above_child (GTK_EVENT_BOX (event), TRUE);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event), FALSE);
	g_signal_connect (event,
			  "button-release-event",
			  G_CALLBACK (brasero_player_button_release),
			  obj);
	g_signal_connect (event,
			  "scroll-event",
			  G_CALLBACK (brasero_player_scroll),
			  obj);
	gtk_widget_show (event);

	priv->notebook = gtk_notebook_new ();
	gtk_widget_show (priv->notebook);
	gtk_container_add (GTK_CONTAINER (event), priv->notebook);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);

	/* Images */
	priv->image_display = gtk_image_new ();
	gtk_widget_show (priv->image_display);
	gtk_misc_set_alignment (GTK_MISC (priv->image_display), 0.5, 0.5);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->image_display,
				  NULL);

	/* Video */
	priv->bacon = brasero_player_bacon_new ();
	gtk_widget_show (priv->bacon);
	g_signal_connect (priv->bacon,
			  "button-release-event",
			  G_CALLBACK (brasero_bacon_button_release),
			  obj);
	g_signal_connect (priv->bacon,
			  "scroll-event",
			  G_CALLBACK (brasero_bacon_scroll),
			  obj);
	g_signal_connect (priv->bacon,
			  "state-change",
			  G_CALLBACK (brasero_player_state_changed_cb),
			  obj);
	g_signal_connect (priv->bacon,
			  "eof",
			  G_CALLBACK (brasero_player_eof_cb),
			  obj);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
				  priv->bacon,
				  NULL);

	/* No Preview view */
	string = g_strdup_printf ("<span color='grey'><big><b>%s</b></big></span>", _("No preview"));
	label = gtk_label_new (string);
	g_free (string);

	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
	                          label,
	                          NULL);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 2);

	/* Loading view */
	priv->spinner = gtk_spinner_new ();
	gtk_widget_show (priv->spinner);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
	                          priv->spinner,
	                          NULL);

	/* Music */
	image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_DIALOG);
	gtk_widget_show (image);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
	                          image,
	                          NULL);
	priv->play_image = image;

	/* Set the saved sizes */
	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_WIDTH,
	                           &value);
	priv->image_width = GPOINTER_TO_INT (value);

	if (priv->image_width > PLAYER_BACON_WIDTH * 3
	||  priv->image_width < PLAYER_BACON_WIDTH)
		priv->image_width = PLAYER_BACON_WIDTH;

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_IMAGE_SIZE_HEIGHT,
	                           &value);
	priv->image_height = GPOINTER_TO_INT (value);

	if (priv->image_height > PLAYER_BACON_HEIGHT * 3
	||  priv->image_height < PLAYER_BACON_HEIGHT)
		priv->image_height = PLAYER_BACON_HEIGHT;

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_WIDTH,
	                           &value);
	priv->video_width = GPOINTER_TO_INT (value);

	if (priv->video_width > PLAYER_BACON_WIDTH * 3
	||  priv->video_width < PLAYER_BACON_WIDTH)
		priv->video_width = PLAYER_BACON_WIDTH;

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_VIDEO_SIZE_HEIGHT,
	                           &value);
	priv->video_height = GPOINTER_TO_INT (value);

	if (priv->video_height > PLAYER_BACON_HEIGHT * 3
	||  priv->video_height < PLAYER_BACON_HEIGHT)
		priv->video_height = PLAYER_BACON_HEIGHT;

	gtk_widget_set_size_request (priv->bacon,
				     priv->video_width,
				     priv->video_height);

	/* A few controls */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_BUTTON);
	zoom = gtk_button_new ();
	gtk_widget_show (zoom);
	gtk_widget_show (image);
	gtk_button_set_image (GTK_BUTTON (zoom), image);
	gtk_button_set_relief (GTK_BUTTON (zoom), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (zoom), FALSE);
	g_signal_connect (zoom,
			  "clicked",
			  G_CALLBACK (brasero_player_zoom_out_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (hbox),
	                    zoom,
	                    FALSE,
	                    FALSE,
	                    0);
	priv->zoom_out = zoom;

	image = gtk_image_new_from_stock (GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_BUTTON);
	zoom = gtk_button_new ();
	gtk_widget_show (zoom);
	gtk_widget_show (image);
	gtk_button_set_image (GTK_BUTTON (zoom), image);
	gtk_button_set_relief (GTK_BUTTON (zoom), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (zoom), FALSE);
	g_signal_connect (zoom,
			  "clicked",
			  G_CALLBACK (brasero_player_zoom_in_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (hbox),
	                    zoom,
	                    FALSE,
	                    FALSE,
	                    0);
	priv->zoom_in = zoom;

	volume = gtk_volume_button_new ();
	gtk_widget_show (volume);
	gtk_box_pack_end (GTK_BOX (hbox),
			  volume,
			  FALSE,
			  FALSE,
			  0);

	gtk_scale_button_set_value (GTK_SCALE_BUTTON (volume),
				    brasero_player_bacon_get_volume (BRASERO_PLAYER_BACON (priv->bacon)));

	g_signal_connect (volume,
			  "value-changed",
			  G_CALLBACK (brasero_player_volume_changed_cb),
			  obj);
}

GtkWidget *
brasero_player_new ()
{
	BraseroPlayer *obj;

	obj = BRASERO_PLAYER (g_object_new (BRASERO_TYPE_PLAYER, NULL));

	return GTK_WIDGET (obj);

}
