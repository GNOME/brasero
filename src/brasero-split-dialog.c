/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"
#include "brasero-metadata.h"

#include "brasero-units.h"

#include "brasero-track.h"
#include "brasero-track-stream.h"

#include "brasero-split-dialog.h"
#include "brasero-song-control.h"
#include "brasero-utils.h"

enum {
	START_COL,
	END_COL,
	LENGTH_COL,
	START_STR_COL,
	END_STR_COL,
	LENGTH_STR_COL,
	COLUMN_NUM,
};

typedef struct _BraseroSplitDialogPrivate BraseroSplitDialogPrivate;
struct _BraseroSplitDialogPrivate
{
	GtkWidget *cut;

	GtkListStore *model;

	GtkWidget *tree;
	GtkWidget *player;

	GtkWidget *notebook;
	GtkWidget *combo;

	GtkWidget *spin_parts;
	GtkWidget *spin_sec;

	GtkWidget *silence_label;

	GtkWidget *reset_button;
	GtkWidget *merge_button;
	GtkWidget *remove_button;

	gint64 start;
	gint64 end;

	BraseroMetadata *metadata;
};

#define BRASERO_SPLIT_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SPLIT_DIALOG, BraseroSplitDialogPrivate))

G_DEFINE_TYPE (BraseroSplitDialog, brasero_split_dialog, GTK_TYPE_DIALOG);

void
brasero_split_dialog_set_uri (BraseroSplitDialog *self,
			      const gchar *uri,
                              const gchar *title,
                              const gchar *artist)
{
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);
	brasero_song_control_set_uri (BRASERO_SONG_CONTROL (priv->player), uri);
	brasero_song_control_set_info (BRASERO_SONG_CONTROL (priv->player), title, artist);
}

void
brasero_split_dialog_set_boundaries (BraseroSplitDialog *self,
				     gint64 start,
				     gint64 end)
{
	BraseroSplitDialogPrivate *priv;
	guint64 length;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	if (BRASERO_DURATION_TO_BYTES (start) % 2352)
		start += BRASERO_BYTES_TO_DURATION (2352 - (BRASERO_DURATION_TO_BYTES (start) % 2352));

	if (BRASERO_DURATION_TO_BYTES (end) % 2352)
		end += BRASERO_BYTES_TO_DURATION (2352 - (BRASERO_DURATION_TO_BYTES (end) % 2352));

	if (end - start < BRASERO_MIN_STREAM_LENGTH)
		return;

	priv->start = start;
	priv->end = end;

	brasero_song_control_set_boundaries (BRASERO_SONG_CONTROL (priv->player),
	                                     priv->start,
	                                     priv->end);

	/* Don't allow splitting the track in sections longer than the track
	 * length in seconds */
	length = (gdouble) brasero_song_control_get_length  (BRASERO_SONG_CONTROL (priv->player)) / 1000000000;
	gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->spin_sec), 1.0, length);
}

GSList *
brasero_split_dialog_get_slices (BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	GSList *retval = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return NULL;

	do {
		BraseroAudioSlice *slice;

		slice = g_new0 (BraseroAudioSlice, 1);
		retval = g_slist_append (retval, slice);

		gtk_tree_model_get (model, &iter,
				    START_COL, &slice->start,
				    END_COL, &slice->end,
				    -1);

	} while (gtk_tree_model_iter_next (model, &iter));

	return retval;
}

static gboolean
brasero_split_dialog_size_error (BraseroSplitDialog *self)
{
	GtkWidget *message;
	GtkResponseType answer;

	message = gtk_message_dialog_new (GTK_WINDOW (self),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					  _("Do you really want to split the track?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("If you split the track, the size of the new track will be shorter than 6 seconds and will be padded."));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (message),
			       _("_Split"),
			       GTK_RESPONSE_YES);

	answer = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (answer == GTK_RESPONSE_YES)
		return TRUE;

	return FALSE;
}

static gboolean
brasero_split_dialog_cut (BraseroSplitDialog *self,
			  gint64 pos,
			  gboolean warn)
{
	BraseroSplitDialogPrivate *priv;
	BraseroAudioSlice slice = {0,0};
	GtkTreeModel *model;
	GtkTreeIter child;
	GtkTreeIter iter;
	gchar *length_str;
	gchar *start_str;
	gchar *end_str;

	if (!pos)
		return FALSE;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	/* since pos is in nanosecond we have a small lattitude. Make sure that
	 * is up to the size of a sector */
	if (BRASERO_DURATION_TO_BYTES (pos) % 2352)
		pos += BRASERO_BYTES_TO_DURATION (2352 - (BRASERO_DURATION_TO_BYTES (pos) % 2352));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		gint64 end;

		/* nothing in the tree yet */
		if (priv->end <= 0)
			end = brasero_song_control_get_length (BRASERO_SONG_CONTROL (priv->player));
		else
			end = priv->end;

		/* check that pos > 300 sectors ( == 4 sec ) */
		if (warn
		&&  pos - priv->start < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		if (warn
		&&  end - (pos + 1) < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		length_str = brasero_units_get_time_string (pos - priv->start, TRUE, FALSE);
		start_str = brasero_units_get_time_string (priv->start, TRUE, FALSE);
		end_str = brasero_units_get_time_string (pos, TRUE, FALSE);

		gtk_list_store_append (priv->model, &iter);
		gtk_list_store_set (priv->model, &iter,
				    START_COL, (gint64) priv->start,
				    END_COL, (gint64) pos,
				    LENGTH_COL, (gint64) pos - priv->start,
				    START_STR_COL, start_str,
				    END_STR_COL, end_str,
				    LENGTH_STR_COL, length_str,
				    -1);
		g_free (length_str);
		g_free (start_str);
		g_free (end_str);

		pos ++;
		length_str = brasero_units_get_time_string (end - pos, TRUE, FALSE);
		start_str = brasero_units_get_time_string (pos, TRUE, FALSE);
		end_str = brasero_units_get_time_string (end, TRUE, FALSE);
		
		gtk_list_store_append (priv->model, &iter);
		gtk_list_store_set (priv->model, &iter,
				    START_COL, pos,
				    END_COL, end,
				    LENGTH_COL, (gint64) (end - pos),
				    START_STR_COL, start_str,
				    END_STR_COL, end_str,
				    LENGTH_STR_COL, length_str,
				    -1);
		g_free (length_str);
		g_free (start_str);
		g_free (end_str);
		return TRUE;
	}

	/* Try to find an already created slice encompassing the position */
	do {
		gint64 start;
		gint64 end;

		gtk_tree_model_get (model, &iter,
				    START_COL, &start,
				    END_COL, &end,
				    -1);

		/* NOTE: if pos == start or pos == end then nothing changes */
		if (pos <= start || pos >= end)
			continue;

		/* check the size of the new tracks */
		if (warn
		&& (pos - start) < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		if (warn
		&& (end - (pos + 1)) < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		/* Found one */
		slice.start = start;
		slice.end = end;
		break;

	} while (gtk_tree_model_iter_next (model, &iter));

	/* see if we found a slice, if not create a new one starting at pos
	 * until the end of the song */

	if (slice.start == 0 && slice.end == 0) {
		slice.start = pos;

		/* check if we need to stop this slice at the end of the song
		 * or at the start of the next slice. */
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gint64 start;
				gint64 end;

				gtk_tree_model_get (model, &iter,
						    START_COL, &start,
						    END_COL, &end,
						    -1);

				if (pos >= start)
					continue;

				/* Found one */
				slice.end = start - 1;
			} while (gtk_tree_model_iter_next (model, &iter));
		}

		if (!slice.end)
			slice.end = priv->end;

		/* check the size of the new slice */
		if (warn
		&& (slice.end - slice.start) < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;
	}
	else {
		/* we are in the middle of an existing slice */
		length_str = brasero_units_get_time_string (pos - slice.start, TRUE, FALSE);
		end_str = brasero_units_get_time_string (pos, TRUE, FALSE);

		gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
								&child,
								&iter);

		gtk_list_store_set (priv->model, &child,
				    END_COL, (gint64) pos,
				    LENGTH_COL, (gint64) (pos - slice.start),
				    END_STR_COL, end_str,
				    LENGTH_STR_COL, length_str,
				    -1);
		g_free (length_str);
		g_free (end_str);

		/* move the position by one */
		pos ++;
	}

	/* create a new one */
	gtk_list_store_append (priv->model, &child);

	length_str = brasero_units_get_time_string (slice.end - pos, TRUE, FALSE);
	start_str = brasero_units_get_time_string (pos, TRUE, FALSE);
	end_str = brasero_units_get_time_string (slice.end, TRUE, FALSE);

	gtk_list_store_set (priv->model, &child,
			    START_COL, pos,
			    END_COL, slice.end,
			    LENGTH_COL, (gint64) (slice.end - pos),
			    START_STR_COL, start_str,
			    END_STR_COL, end_str,
			    LENGTH_STR_COL, length_str,
			    -1);

	g_free (length_str);
	g_free (start_str);
	g_free (end_str);

	return TRUE;
}

static void
brasero_split_dialog_remove_range (BraseroSplitDialog *self,
				   gint64 start,
				   gint64 end,
				   gint64 length)
{
	BraseroSplitDialogPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *length_str;
	gchar *start_str;
	gchar *end_str;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	/* align on 2352 byte boundary */
	if (BRASERO_DURATION_TO_BYTES (start) % 2352)
		start += BRASERO_BYTES_TO_DURATION (2352 - (BRASERO_DURATION_TO_BYTES (start) % 2352));

	if (BRASERO_DURATION_TO_BYTES (end) % 2352) {
		end += BRASERO_BYTES_TO_DURATION (2352 - (BRASERO_DURATION_TO_BYTES (end) % 2352));
		if (end > length)
			end = length;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		/* nothing in the tree yet; so create two new segments:
		 * - 0 => start 
		 * - end => song end
		 * also make sure that the track is longer than 4 sec */
		if (start - priv->start < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self)) {
			/* that's not necessarily a good solution */
			start = BRASERO_MIN_STREAM_LENGTH;
			if (start > end)
				end = start;
		}

		if ((length - end) < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			end = length - BRASERO_MIN_STREAM_LENGTH;

		length_str = brasero_units_get_time_string (start - priv->start, TRUE, FALSE);
		start_str = brasero_units_get_time_string (priv->start, TRUE, FALSE);
		end_str = brasero_units_get_time_string (start, TRUE, FALSE);

		gtk_list_store_append (priv->model, &iter);
		gtk_list_store_set (priv->model, &iter,
				    START_COL, (gint64) priv->start,
				    END_COL, (gint64) start,
				    LENGTH_COL, (gint64) start - priv->start,
				    START_STR_COL, start_str,
				    END_STR_COL, end_str,
				    LENGTH_STR_COL, length_str,
				    -1);
		g_free (length_str);
		g_free (start_str);
		g_free (end_str);

		if (end == length)
			return;

		length_str = brasero_units_get_time_string (length - end, TRUE, FALSE);
		start_str = brasero_units_get_time_string (end, TRUE, FALSE);
		end_str = brasero_units_get_time_string (length, TRUE, FALSE);
		
		gtk_list_store_append (priv->model, &iter);
		gtk_list_store_set (priv->model, &iter,
				    START_COL, end,
				    END_COL, length,
				    LENGTH_COL, (gint64) (length - end),
				    START_STR_COL, start_str,
				    END_STR_COL, end_str,
				    LENGTH_STR_COL, length_str,
				    -1);
		g_free (length_str);
		g_free (start_str);
		g_free (end_str);
		return;
	}

	do {
		GtkTreeIter child;
		gint64 track_start;
		gint64 track_end;

		gtk_tree_model_get (model, &iter,
				    START_COL, &track_start,
				    END_COL, &track_end,
				    -1);

		gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
								&child,
								&iter);

		if (start == track_start) {
			if (start == end)
				return;

			if (end == track_end) {
				/* suppress it */
				gtk_list_store_remove (priv->model, &child);
				return;
			}

			if (end < track_end) {
				/* reduce the size but make sure the remaining 
				 * track is > 4 sec */
				if ((track_end - end) < BRASERO_MIN_STREAM_LENGTH
				&& !brasero_split_dialog_size_error (self))
					end = track_end - BRASERO_MIN_STREAM_LENGTH;

				start_str = brasero_units_get_time_string (end, TRUE, FALSE);
				length_str = brasero_units_get_time_string (track_end - end, TRUE, FALSE);
				gtk_list_store_set (priv->model, &child,
						    START_COL, end,
						    START_STR_COL, start_str,
						    LENGTH_COL, track_end - end,
						    LENGTH_STR_COL, length_str,
						    -1);
				g_free (length_str);
				g_free (start_str);
			}
			else if (!gtk_list_store_remove (priv->model, &child))
				return;
		}
		else if (start > track_start) {
			if (start > track_end)
				continue;

			/* reduce the size but make sure the remaining track is
			 * > 4 sec else change it */
			if ((start - track_start) < BRASERO_MIN_STREAM_LENGTH
			&& !brasero_split_dialog_size_error (self))
				start = track_start + BRASERO_MIN_STREAM_LENGTH;

			start_str = brasero_units_get_time_string (start, TRUE, FALSE);
			length_str = brasero_units_get_time_string (start - track_start, TRUE, FALSE);
			gtk_list_store_set (priv->model, &child,
					    END_COL, start,
					    END_STR_COL, start_str,
					    LENGTH_COL, start - track_start,
					    LENGTH_STR_COL, length_str,
					    -1);
			g_free (length_str);
			g_free (start_str);

			if (end == length)
				return;

			if (end == track_end)
				return;

			if (end > track_end)
				continue;

			/* create a new track with the remaining time.
			 * make sure the remaining track is > 4 sec */
			if ((track_end - end) < BRASERO_MIN_STREAM_LENGTH
			&& !brasero_split_dialog_size_error (self))
				end = track_end - BRASERO_MIN_STREAM_LENGTH;

			gtk_list_store_append (priv->model, &child);

			length_str = brasero_units_get_time_string (track_end - end, TRUE, FALSE);
			start_str = brasero_units_get_time_string (end, TRUE, FALSE);
			end_str = brasero_units_get_time_string (track_end, TRUE, FALSE);

			gtk_list_store_set (priv->model, &child,
					    START_COL, end,
					    END_COL, track_end,
					    LENGTH_COL, (gint64) (track_end - end),
					    START_STR_COL, start_str,
					    END_STR_COL, end_str,
					    LENGTH_STR_COL, length_str,
					    -1);
			g_free (length_str);
			g_free (start_str);
			g_free (end_str);
		}
		else if (end > track_end) {
			if (!gtk_list_store_remove (priv->model, &child))
				return;
		}
		else if (end == track_end) {
			gtk_list_store_remove (priv->model, &child);
			return;
		}
		else {
			if (end == length) {
				gtk_list_store_remove (priv->model, &child);
				return;
			}

			/* resize (make sure about the 4s) */
			if ((track_end - end) < BRASERO_MIN_STREAM_LENGTH
			&& !brasero_split_dialog_size_error (self))
				end = track_end - BRASERO_MIN_STREAM_LENGTH;

			start_str = brasero_units_get_time_string (end, TRUE, FALSE);
			length_str = brasero_units_get_time_string (track_end - end, TRUE, FALSE);
			gtk_list_store_set (priv->model, &child,
					    START_COL, end,
					    START_STR_COL, start_str,
					    LENGTH_COL, track_end - end,
					    LENGTH_STR_COL, length_str,
					    -1);
			g_free (length_str);
			g_free (start_str);
		}

		gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (model),
								&iter,
								&child);
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
brasero_split_dialog_no_silence_message (BraseroSplitDialog *self)
{
	brasero_utils_message_dialog (GTK_WIDGET (self),
				      _("The track wasn't split."),
				      _("No silence could be detected"),
				      GTK_MESSAGE_WARNING);
}

static void
brasero_split_dialog_metadata_finished_cb (BraseroMetadata *metadata,
					   GError *error,
					   BraseroSplitDialog *self)
{
	BraseroMetadataInfo info = { NULL, };
	BraseroSplitDialogPrivate *priv;
	gboolean added_silence;
	GSList *iter;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	gtk_widget_set_sensitive (priv->cut, TRUE);

	g_object_unref (priv->metadata);
	priv->metadata = NULL;

	if (error) {
		brasero_utils_message_dialog (GTK_WIDGET (self),
					      _("An error occurred while detecting silences."),
					      error->message,
					      GTK_MESSAGE_ERROR);
		return;
	}

	brasero_metadata_get_result (metadata, &info, NULL);
	if (!info.silences) {
		brasero_split_dialog_no_silence_message (self);
		return;
	}

	/* remove silences */
	added_silence = FALSE;
	for (iter = info.silences; iter; iter = iter->next) {
		BraseroMetadataSilence *silence;

		silence = iter->data;

		if (!silence)
			continue;

		if (silence->start >= priv->end)
			continue;

		if (silence->end <= priv->start)
			continue;

		if (silence->start < priv->start)
			silence->start = priv->start;

		if (silence->end > priv->end)
			silence->end = priv->end;

		if (!silence->start)
			brasero_split_dialog_cut (self, silence->end, TRUE);
		else if (silence->start != silence->end)
			brasero_split_dialog_remove_range (self,
							   silence->start,
							   silence->end,
							   priv->end - priv->start);
		else
			brasero_split_dialog_cut (self, silence->end, TRUE);

		added_silence = TRUE;
	}

	if (!added_silence)
		brasero_split_dialog_no_silence_message (self);

	brasero_metadata_info_clear (&info);
}

static gboolean
brasero_split_dialog_clear_confirm_dialog (BraseroSplitDialog *self,
					   const gchar *primary,
					   const gchar *cancel_button,
					   const gchar *ok_button)
{
	BraseroSplitDialogPrivate *priv;
	GtkResponseType answer;
	GtkTreeModel *model;
	GtkWidget *message;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_model_iter_n_children (model, NULL))
		return TRUE;

	message = gtk_message_dialog_new (GTK_WINDOW (self),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					  "%s",
					  primary);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("This will remove all previous results."));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       cancel_button,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (message),
			       ok_button,
			       GTK_RESPONSE_YES);

	answer = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (answer != GTK_RESPONSE_YES)
		return FALSE;

	return TRUE;
}

static void
brasero_split_dialog_cut_clicked_cb (GtkButton *button,
				     BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	guint page;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	page = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo));
	if (page == 0) {
		gint64 pos;

		/* this one is before since it doesn't wipe all slices */
		pos = brasero_song_control_get_pos (BRASERO_SONG_CONTROL (priv->player));
		brasero_split_dialog_cut (self, pos + priv->start, TRUE);
		return;
	}

	if (!brasero_split_dialog_clear_confirm_dialog (self,
							_("Do you really want to carry on with automatic splitting?"),
							_("_Don't split"),
							_("_Split")))
		return;

	if (page == 1) {
		gint64 sec;
		gint64 start;
		gint64 length;

		sec = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->spin_sec));

		sec *= 1000000000;
		if (sec < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return;

		length = priv->end - priv->start;

		gtk_list_store_clear (priv->model);
		for (start = sec; start < length; start += sec)
			brasero_split_dialog_cut (self, start, FALSE);

		return;
	}

	if (page == 2) {
		gint64 step;
		gint64 start;
		gint64 parts;
		gint64 length;

		parts = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->spin_parts));

		length = priv->end - priv->start;
		step = length / parts;

		if (step < BRASERO_MIN_STREAM_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return;

		gtk_list_store_clear (priv->model);

		parts --;
		for (start = step; start < length && parts; start += step, parts --)
			brasero_split_dialog_cut (self, start, FALSE);

		return;
	}

	gtk_list_store_clear (priv->model);

	priv->metadata = brasero_metadata_new ();
	g_signal_connect (priv->metadata,
			  "completed",
			  G_CALLBACK (brasero_split_dialog_metadata_finished_cb),
			  self);
	brasero_metadata_get_info_async (priv->metadata,
					 brasero_song_control_get_uri (BRASERO_SONG_CONTROL (priv->player)),
					 BRASERO_METADATA_FLAG_SILENCES);

	/* stop anything from playing and grey out things */
	gtk_widget_set_sensitive (priv->cut, FALSE);
}

static void
brasero_split_dialog_merge_clicked_cb (GtkButton *button,
				       BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		guint64 end;
		guint64 start;
		gchar *end_str;
		gchar *start_str;
		GtkTreeIter next;
		gchar *length_str;
		GtkTreeIter child;

		if (!gtk_tree_selection_iter_is_selected (selection, &iter))
			continue;

		next = iter;
		if (!gtk_tree_model_iter_next (model, &next))
			continue;

		if (!gtk_tree_selection_iter_is_selected (selection, &next))
			continue;

		gtk_tree_model_get (model, &iter,
				    START_COL, &start,
				    -1);

		gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
								&child,
								&iter);

		do {
			GtkTreeIter next_child;

			gtk_tree_model_get (model, &next,
					    END_COL, &end,
					   -1);

			gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
									&next_child,
									&next);

			if (!gtk_list_store_remove (priv->model, &next_child))
				break;

			gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (model),
									&next,
									&next_child);

		} while (gtk_tree_selection_iter_is_selected (selection, &next));

		length_str = brasero_units_get_time_string (end - start, TRUE, FALSE);
		start_str = brasero_units_get_time_string (start, TRUE, FALSE);
		end_str = brasero_units_get_time_string (end, TRUE, FALSE);

		gtk_list_store_set (priv->model, &child,
				    START_COL, (gint64) start,
				    END_COL, (gint64) end,
				    LENGTH_COL, (gint64) end - start,
				    START_STR_COL, start_str,
				    END_STR_COL, end_str,
				    LENGTH_STR_COL, length_str,
				    -1);
		g_free (length_str);
		g_free (start_str);
		g_free (end_str);

		gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (model),
								&iter,
								&child);

	} while (gtk_tree_model_iter_next (model, &iter));

	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->model), NULL) == 1)
		gtk_list_store_clear (priv->model);
}

static void
brasero_split_dialog_remove_clicked_cb (GtkButton *button,
				        BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	GList *references = NULL;
	GtkTreeModel *model;
	GList *selected;
	GList *iter;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	selected = gtk_tree_selection_get_selected_rows (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)), &model);

	/* since we are going to modify the tree take references */
	for (iter = selected; iter; iter = iter->next) {
		GtkTreePath *treepath;
		GtkTreeRowReference *reference;

		treepath = iter->data;
		reference = gtk_tree_row_reference_new (model, treepath);
		gtk_tree_path_free (treepath);

		references = g_list_prepend (references, reference);
	}
	g_list_free (selected);

	for (iter = references; iter; iter = iter->next) {
		GtkTreeRowReference *reference;
		GtkTreePath *treepath;
		GtkTreeIter child;
		GtkTreeIter row;

		reference = iter->data;

		treepath = gtk_tree_row_reference_get_path (reference);
		gtk_tree_row_reference_free (reference);
		if (!treepath)
			continue;

		if (!gtk_tree_model_get_iter (model, &row, treepath)) {
			gtk_tree_path_free (treepath);
			continue;
		}

		gtk_tree_path_free (treepath);

		gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
								&child,
								&row);

		gtk_list_store_remove (priv->model, &child);
	}
	g_list_free (references);
}

static void
brasero_split_dialog_reset_clicked_cb (GtkButton *button,
				       BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);
	if (!brasero_split_dialog_clear_confirm_dialog (self,
							_("Do you really want to empty the slices preview?"),
							GTK_STOCK_CANCEL,
							_("Re_move All")))
		return;

	gtk_list_store_clear (priv->model);
}

static void
brasero_split_dialog_combo_changed_cb (GtkComboBox *combo,
				       BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	guint page;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);
	page = gtk_combo_box_get_active (combo);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), page);
}

static void
brasero_split_dialog_selection_changed_cb (GtkTreeSelection *selection,
					   BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	GList *selected;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	selected = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (selected) {
		g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selected);

		gtk_widget_set_sensitive (priv->merge_button, TRUE);
		gtk_widget_set_sensitive (priv->remove_button, TRUE);
	}
	else {
		gtk_widget_set_sensitive (priv->merge_button, FALSE);
		gtk_widget_set_sensitive (priv->remove_button, FALSE);
	}
}

static void
brasero_split_dialog_row_inserted_cb (GtkTreeModel *model,
				      GtkTreePath *path,
				      GtkTreeIter *iter,
				      BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);
	gtk_widget_set_sensitive (priv->reset_button, TRUE);
}

static void
brasero_split_dialog_row_deleted_cb (GtkTreeModel *model,
				     GtkTreePath *path,
				     BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	GtkTreeIter iter;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);
	if (!gtk_tree_model_get_iter_first (model, &iter))
		gtk_widget_set_sensitive (priv->reset_button, FALSE);
}

static void
brasero_split_dialog_init (BraseroSplitDialog *object)
{
	gchar *title;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *hbox2;
	GtkWidget *label;
	GtkWidget *scroll;
	GtkWidget *button;
	GtkTreeModel *model;
	GtkSizeGroup *size_group;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (object);

	gtk_window_set_title (GTK_WINDOW (object), _("Split Track"));
	gtk_window_set_default_size (GTK_WINDOW (object), 500, 600);

	gtk_dialog_add_button (GTK_DIALOG (object), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (object), GTK_STOCK_OK, GTK_RESPONSE_OK);

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (object));
	gtk_box_set_spacing (GTK_BOX (vbox), 0);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	/* Slicing method */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);

	priv->combo = gtk_combo_box_text_new ();

	label = gtk_label_new_with_mnemonic (_("M_ethod:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->combo);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	gtk_widget_set_tooltip_text (priv->combo, _("Method to be used to split the track"));
	gtk_widget_show (priv->combo);
	gtk_box_pack_start (GTK_BOX (hbox), priv->combo, TRUE, TRUE, 0);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Split track manually"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Split track in parts with a fixed length"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Split track in a fixed number of parts"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Split track for each silence"));
	g_signal_connect (priv->combo,
			  "changed",
			  G_CALLBACK (brasero_split_dialog_combo_changed_cb),
			  object);

	button = brasero_utils_make_button (_("_Slice"),
					    NULL,
					    "transform-crop-and-resize",
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);
	gtk_size_group_add_widget (size_group, button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_split_dialog_cut_clicked_cb),
			  object);
	gtk_widget_set_tooltip_text (button, _("Add a splitting point"));
	priv->cut = button;

	priv->notebook = gtk_notebook_new ();
	gtk_widget_show (priv->notebook);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);

	priv->player = brasero_song_control_new ();
	gtk_widget_show (priv->player);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->player, NULL);

	hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_valign (hbox2, GTK_ALIGN_CENTER);
	gtk_widget_show (hbox2);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), hbox2, NULL);

	/* Translators: this goes with the next (= "seconds") */
	label = gtk_label_new (_("Split this track every"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

	priv->spin_sec = gtk_spin_button_new_with_range (1.0, 1000.0, 1.0);
	gtk_widget_show (priv->spin_sec);
	gtk_box_pack_start (GTK_BOX (hbox2), priv->spin_sec, FALSE, FALSE, 0);

	/* Translators: this goes with the previous (= "Split track every") */
	label = gtk_label_new (_("seconds"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

	hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_valign (hbox2, GTK_ALIGN_CENTER);
	gtk_widget_show (hbox2);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), hbox2, NULL);

	/* Translators: this goes with the next (= "parts") */
	label = gtk_label_new (_("Split this track in"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

	priv->spin_parts = gtk_spin_button_new_with_range (2.0, 1000.0, 1.0);
	gtk_widget_show (priv->spin_parts);
	gtk_box_pack_start (GTK_BOX (hbox2), priv->spin_parts, FALSE, FALSE, 0);

	/* Translators: this goes with the previous (= "Split this track in") */
	label = gtk_label_new (_("parts"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

	priv->silence_label = gtk_label_new (NULL);
	gtk_widget_show (priv->silence_label);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->silence_label, NULL);

	title = g_strdup_printf ("<b>%s</b>", _("Slicing Method"));
	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (title,
							   priv->notebook,
							   hbox,
							   NULL),
			    FALSE,
			    FALSE,
			    0);
	g_free (title);

	/* slices preview */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);

	priv->model = gtk_list_store_new (COLUMN_NUM,
					  G_TYPE_INT64,
					  G_TYPE_INT64,
					  G_TYPE_INT64,
					  G_TYPE_STRING,
					  G_TYPE_STRING,
					  G_TYPE_STRING);

	g_signal_connect (priv->model,
			  "row-inserted",
			  G_CALLBACK (brasero_split_dialog_row_inserted_cb),
			  object);
	g_signal_connect (priv->model,
			  "row-deleted",
			  G_CALLBACK (brasero_split_dialog_row_deleted_cb),
			  object);

	model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (priv->model));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      START_COL,
					      GTK_SORT_ASCENDING);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll);
	gtk_box_pack_start (GTK_BOX (hbox), scroll, TRUE, TRUE, 0);

	priv->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->tree), TRUE);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
				     GTK_SELECTION_MULTIPLE);

	gtk_widget_show (priv->tree);
	gtk_container_add (GTK_CONTAINER (scroll), priv->tree);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Start"),
							   renderer,
							   "text", START_STR_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("End"),
							   renderer,
							   "text", END_STR_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Length"),
							   renderer,
							   "text", LENGTH_STR_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
			  "changed",
			  G_CALLBACK (brasero_split_dialog_selection_changed_cb),
			  object);

	/* buttons */
	vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, TRUE, 0);

	button = brasero_utils_make_button (_("Mer_ge"),
					    NULL,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);
	gtk_size_group_add_widget (size_group, button);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_split_dialog_merge_clicked_cb),
			  object);
	gtk_widget_set_tooltip_text (button, _("Merge a selected slice with the next selected one"));
	priv->merge_button = button;

	button = brasero_utils_make_button (_("_Remove"),
					    GTK_STOCK_REMOVE,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);
	gtk_size_group_add_widget (size_group, button);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_split_dialog_remove_clicked_cb),
			  object);
	gtk_widget_set_tooltip_text (button, _("Remove the selected slices"));
	priv->remove_button = button;

	button = brasero_utils_make_button (_("Re_move All"),
					    NULL,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);
	gtk_size_group_add_widget (size_group, button);
	gtk_box_pack_start (GTK_BOX (vbox2), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_split_dialog_reset_clicked_cb),
			  object);
	gtk_widget_set_tooltip_text (button, _("Clear the slices preview"));
	priv->reset_button = button;

	gtk_widget_set_sensitive (priv->reset_button, FALSE);
	gtk_widget_set_sensitive (priv->merge_button, FALSE);
	gtk_widget_set_sensitive (priv->remove_button, FALSE);

	vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (vbox2);

	label = gtk_label_new_with_mnemonic (_("_List of slices that are to be created:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->tree);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

	title = g_strdup_printf ("<b>%s</b>", _("Slices Preview"));
	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (title,
							   vbox2,
							   NULL),
			    TRUE,
			    TRUE,
			    0);
	g_free (title);

	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), 0);
	g_object_unref (size_group);
}

static void
brasero_split_dialog_finalize (GObject *object)
{
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (object);
	if (priv->metadata) {
		brasero_metadata_cancel (priv->metadata);
		g_object_unref (priv->metadata);
		priv->metadata = NULL;
	}

	G_OBJECT_CLASS (brasero_split_dialog_parent_class)->finalize (object);
}

static void
brasero_split_dialog_class_init (BraseroSplitDialogClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSplitDialogPrivate));

	object_class->finalize = brasero_split_dialog_finalize;
}

GtkWidget *
brasero_split_dialog_new (void)
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_SPLIT_DIALOG, NULL));
}
