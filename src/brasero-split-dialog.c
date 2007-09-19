/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
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

#include <gtk/gtkdialog.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkradiobutton.h>

#include "brasero-split-dialog.h"
#include "brasero-player.h"
#include "brasero-utils.h"
#include "brasero-metadata.h"
#include "burn-track.h"

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
	GtkWidget *auto_cut;

	GtkWidget *tree;
	GtkWidget *remove;
	GtkWidget *player;

	GtkWidget *radio_parts;
	GtkWidget *radio_sec;

	GtkWidget *spin_parts;
	GtkWidget *spin_sec;

	gint64 start;
	gint64 end;

	BraseroMetadata *metadata;
};

#define BRASERO_SPLIT_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SPLIT_DIALOG, BraseroSplitDialogPrivate))

G_DEFINE_TYPE (BraseroSplitDialog, brasero_split_dialog, GTK_TYPE_DIALOG);

void
brasero_split_dialog_set_uri (BraseroSplitDialog *self,
			      const gchar *uri)
{
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);
	brasero_player_set_uri (BRASERO_PLAYER (priv->player), uri);
}

void
brasero_split_dialog_set_boundaries (BraseroSplitDialog *self,
				     gint64 start,
				     gint64 end)
{
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	if (BRASERO_DURATION_TO_BYTES (start) % 2352)
		start += BRASERO_BYTES_TO_DURATION (2352 - (BRASERO_DURATION_TO_BYTES (start) % 2352));

	if (BRASERO_DURATION_TO_BYTES (end) % 2352)
		end += BRASERO_BYTES_TO_DURATION (2352 - (BRASERO_DURATION_TO_BYTES (end) % 2352));

	if (end - start < BRASERO_MIN_AUDIO_TRACK_LENGTH)
		return;

	priv->start = start;
	priv->end = end;

	brasero_player_set_boundaries (BRASERO_PLAYER (priv->player),
				       priv->start,
				       priv->end);
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
					  GTK_BUTTONS_YES_NO,
					  _("The size of the new track is shorted than 6 seconds and will be padded."));
	gtk_window_set_title (GTK_WINDOW (message), _("size error"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Do you want to split it nevertheless?"));

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
	GtkTreeModel *model;
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
			end = brasero_player_get_length (BRASERO_PLAYER (priv->player));
		else
			end = priv->end;

		/* check that pos > 300 sectors ( == 4 sec ) */
		if (warn
		&&  pos - priv->start < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		if (warn
		&&  end - (pos + 1) < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		length_str = brasero_utils_get_time_string (pos - priv->start, TRUE, FALSE);
		start_str = brasero_utils_get_time_string (priv->start, TRUE, FALSE);
		end_str = brasero_utils_get_time_string (pos, TRUE, FALSE);

		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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
		length_str = brasero_utils_get_time_string (end - pos, TRUE, FALSE);
		start_str = brasero_utils_get_time_string (pos, TRUE, FALSE);
		end_str = brasero_utils_get_time_string (end, TRUE, FALSE);
		
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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
		&& (pos - start) < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		if (warn
		&& (end - (pos + 1)) < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return FALSE;

		/* we are in the middle of an existing slice */
		length_str = brasero_utils_get_time_string (pos - start, TRUE, FALSE);
		end_str = brasero_utils_get_time_string (pos, TRUE, FALSE);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    END_COL, (gint64)  pos,
				    LENGTH_COL, (gint64) (pos - start),
				    END_STR_COL, end_str,
				    LENGTH_STR_COL, length_str,
				    -1);
		g_free (length_str);
		g_free (end_str);

		gtk_list_store_append (GTK_LIST_STORE (model), &iter);

		pos ++;
		length_str = brasero_utils_get_time_string (end - pos, TRUE, FALSE);
		start_str = brasero_utils_get_time_string (pos, TRUE, FALSE);
		end_str = brasero_utils_get_time_string (end, TRUE, FALSE);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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
		break;

	} while (gtk_tree_model_iter_next (model, &iter));

	return TRUE;
}

static void
brasero_split_dialog_cut_clicked_cb (GtkButton *button,
				     BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	gint64 pos;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	pos = brasero_player_get_pos (BRASERO_PLAYER (priv->player));
	brasero_split_dialog_cut (self, pos + priv->start, TRUE);
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
		if (start - priv->start < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self)) {
			/* that's not necessarily a good solution */
			start = BRASERO_MIN_AUDIO_TRACK_LENGTH;
			if (start > end)
				end = start;
		}

		if ((length - end) < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self))
			end = length - BRASERO_MIN_AUDIO_TRACK_LENGTH;

		length_str = brasero_utils_get_time_string (start - priv->start, TRUE, FALSE);
		start_str = brasero_utils_get_time_string (priv->start, TRUE, FALSE);
		end_str = brasero_utils_get_time_string (start, TRUE, FALSE);

		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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

		length_str = brasero_utils_get_time_string (length - end, TRUE, FALSE);
		start_str = brasero_utils_get_time_string (end, TRUE, FALSE);
		end_str = brasero_utils_get_time_string (length, TRUE, FALSE);
		
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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
		gint64 track_start;
		gint64 track_end;

		gtk_tree_model_get (model, &iter,
				    START_COL, &track_start,
				    END_COL, &track_end,
				    -1);

		if (start == track_start) {
			if (start == end)
				return;

			if (end == track_end) {
				/* suppress it */
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				return;
			}

			if (end < track_end) {
				/* reduce the size but make sure the remaining 
				 * track is > 4 sec */
				if ((track_end - end) < BRASERO_MIN_AUDIO_TRACK_LENGTH
				&& !brasero_split_dialog_size_error (self))
					end = track_end - BRASERO_MIN_AUDIO_TRACK_LENGTH;

				start_str = brasero_utils_get_time_string (end, TRUE, FALSE);
				length_str = brasero_utils_get_time_string (track_end - end, TRUE, FALSE);
				gtk_list_store_set (GTK_LIST_STORE (model), &iter,
						    START_COL, end,
						    START_STR_COL, start_str,
						    LENGTH_COL, track_end - end,
						    LENGTH_STR_COL, length_str,
						    -1);
				g_free (length_str);
				g_free (start_str);
			}
			else if (!gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
				return;
		}
		else if (start > track_start) {
			if (start > track_end)
				continue;

			/* reduce the size but make sure the remaining track is
			 * > 4 sec else change it */
			if ((start - track_start) < BRASERO_MIN_AUDIO_TRACK_LENGTH
			&& !brasero_split_dialog_size_error (self))
				start = track_start + BRASERO_MIN_AUDIO_TRACK_LENGTH;

			start_str = brasero_utils_get_time_string (start, TRUE, FALSE);
			length_str = brasero_utils_get_time_string (start - track_start, TRUE, FALSE);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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
			if ((track_end - end) < BRASERO_MIN_AUDIO_TRACK_LENGTH
			&& !brasero_split_dialog_size_error (self))
				end = track_end - BRASERO_MIN_AUDIO_TRACK_LENGTH;

			gtk_list_store_append (GTK_LIST_STORE (model), &iter);

			length_str = brasero_utils_get_time_string (track_end - end, TRUE, FALSE);
			start_str = brasero_utils_get_time_string (end, TRUE, FALSE);
			end_str = brasero_utils_get_time_string (track_end, TRUE, FALSE);

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
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
			if (!gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
				return;
		}
		else if (end == track_end) {
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			return;
		}
		else {
			if (end == length) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				return;
			}

			/* resize (make sure about the 4s) */
			if ((track_end - end) < BRASERO_MIN_AUDIO_TRACK_LENGTH
			&& !brasero_split_dialog_size_error (self))
				end = track_end - BRASERO_MIN_AUDIO_TRACK_LENGTH;

			start_str = brasero_utils_get_time_string (end, TRUE, FALSE);
			length_str = brasero_utils_get_time_string (track_end - end, TRUE, FALSE);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    START_COL, end,
					    START_STR_COL, start_str,
					    LENGTH_COL, track_end - end,
					    LENGTH_STR_COL, length_str,
					    -1);
			g_free (length_str);
			g_free (start_str);
		}

	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
brasero_split_dialog_no_silence_message (BraseroSplitDialog *self)
{
	GtkWidget *message;

	/* no silences found */
	message = gtk_message_dialog_new (GTK_WINDOW (self),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_CLOSE,
					  _("The track wasn't split:"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("no silence could be retrieved."));

	gtk_window_set_title (GTK_WINDOW (message), _("no silence"));
	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

static void
brasero_split_dialog_metadata_finished_cb (BraseroMetadata *metadata,
					   GError *error,
					   BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	BraseroMetadataInfo info;
	gboolean added_silence;
	GSList *iter;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	gtk_widget_set_sensitive (priv->cut, TRUE);
	gtk_widget_set_sensitive (priv->auto_cut, TRUE);

	g_object_unref (priv->metadata);
	priv->metadata = NULL;

	if (error) {
		GtkWidget *message;

		/* error while retrieve silences */
		message = gtk_message_dialog_new (GTK_WINDOW (self),
						  GTK_DIALOG_DESTROY_WITH_PARENT|
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_ERROR,
						  GTK_BUTTONS_CLOSE,
						  _("An error occured while retrieving silences:"));
		gtk_window_set_title (GTK_WINDOW (message), _("Error"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  error->message);

		gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);
		return;
	}

	brasero_metadata_set_info (metadata, &info);
	if (!info.silences) {
		brasero_split_dialog_no_silence_message (self);
		return;
	}

	/* remove silences */
	added_silence = FALSE;
	for (iter = info.silences; iter; iter = iter->next) {
		BraseroMetadataSilence *silence;

		silence = iter->data;

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

static void
brasero_split_dialog_autocut_clicked_cb (GtkButton *button,
					 BraseroSplitDialog *self)
{
	BraseroSplitDialogPrivate *priv;
	GtkTreeModel *model;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (self);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (gtk_tree_model_iter_n_children (model, NULL)) {
		GtkWidget *message;
		GtkResponseType answer;

		message = gtk_message_dialog_new (GTK_WINDOW (self),
						  GTK_DIALOG_DESTROY_WITH_PARENT|
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_QUESTION,
						  GTK_BUTTONS_YES_NO,
						  _("This will remove all previous results."));
		gtk_window_set_title (GTK_WINDOW (message), _("automatic splitting"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  _("Do you want to carry on with automatic splitting nevertheless?"));

		answer = gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);

		if (answer != GTK_RESPONSE_YES)
			return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->radio_sec))) {
		gint64 sec;
		gint64 start;
		gint64 length;

		sec = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->spin_sec));

		sec *= 1000000000;
		if (sec < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return;

		length = priv->end - priv->start;

		gtk_list_store_clear (GTK_LIST_STORE (model));
		for (start = sec; start < length; start += sec)
			brasero_split_dialog_cut (self, start, FALSE);

		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->radio_parts))) {
		gint64 step;
		gint64 start;
		gint64 parts;
		gint64 length;

		parts = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->spin_parts));

		length = priv->end - priv->start;
		step = length / parts;

		if (step < BRASERO_MIN_AUDIO_TRACK_LENGTH
		&& !brasero_split_dialog_size_error (self))
			return;

		gtk_list_store_clear (GTK_LIST_STORE (model));
		for (start = step; start < length; start += step)
			brasero_split_dialog_cut (self, start, FALSE);
		return;
	}

	gtk_list_store_clear (GTK_LIST_STORE (model));

	priv->metadata = brasero_metadata_new ();
	g_signal_connect (priv->metadata,
			  "completed",
			  G_CALLBACK (brasero_split_dialog_metadata_finished_cb),
			  self);
	brasero_metadata_get_info_async (priv->metadata,
					 brasero_player_get_uri (BRASERO_PLAYER (priv->player)),
					 BRASERO_METADATA_FLAG_SILENCES);

	/* stop anything from playing and grey out things */
	gtk_widget_set_sensitive (priv->cut, FALSE);
	gtk_widget_set_sensitive (priv->auto_cut, FALSE);
}

static void
brasero_split_dialog_init (BraseroSplitDialog *object)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *radio;
	GtkWidget *scroll;
	GtkWidget *button;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	BraseroSplitDialogPrivate *priv;

	priv = BRASERO_SPLIT_DIALOG_PRIVATE (object);

	gtk_window_set_title (GTK_WINDOW (object), _("Track splitting dialog"));
	gtk_window_set_default_size (GTK_WINDOW (object), 550, 400);

	gtk_dialog_set_has_separator (GTK_DIALOG (object), FALSE);
	gtk_dialog_add_button (GTK_DIALOG (object), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (object), GTK_STOCK_OK, GTK_RESPONSE_OK);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (vbox);

	label = gtk_label_new (_("Move the slider of the player and press \"Cut\" when you want to add a splitting point."));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);

	priv->player = brasero_player_new ();
	gtk_widget_show (priv->player);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);

	button = gtk_button_new_from_stock (GTK_STOCK_CUT);
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_split_dialog_cut_clicked_cb),
			  object);
	priv->cut = button;

	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (_("<b>Manual splitting</b>"),
							   hbox,
							   priv->player,
							   label,
							   NULL),
			    FALSE,
			    FALSE,
			    0);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);

	radio = gtk_radio_button_new_with_label (NULL, _("for every silence (automatic search)"));
	gtk_widget_show (radio);
	gtk_box_pack_start (GTK_BOX (vbox2), radio, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	/* Translators: this goes with the next (= "seconds") */
	radio = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio),
							     _("every \"x\" seconds"));
	gtk_widget_show (radio);
	gtk_box_pack_start (GTK_BOX (hbox), radio, FALSE, FALSE, 0);
	priv->radio_sec = radio;

	priv->spin_sec = gtk_spin_button_new_with_range (1.0, 1000.0, 1.0);
	gtk_widget_show (priv->spin_sec);
	gtk_box_pack_start (GTK_BOX (hbox), priv->spin_sec, FALSE, FALSE, 0);

	/* Translators: this goes with the previous (= "Split track every") */
	label = gtk_label_new (_("seconds"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	radio = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio), _("in \"x\" parts"));
	gtk_widget_show (radio);
	gtk_box_pack_start (GTK_BOX (hbox), radio, FALSE, FALSE, 0);
	priv->radio_parts = radio;

	priv->spin_parts = gtk_spin_button_new_with_range (2.0, 1000.0, 1.0);
	gtk_widget_show (priv->spin_parts);
	gtk_box_pack_start (GTK_BOX (hbox), priv->spin_parts, FALSE, FALSE, 0);

	label = gtk_label_new (_("parts"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	
	button = gtk_button_new_from_stock (GTK_STOCK_CUT);
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_split_dialog_autocut_clicked_cb),
			  object);
	priv->auto_cut = button;

	label = gtk_label_new (_("Split track automatically:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (_("<b>Automatic splitting</b>"),
							   vbox2,
							   label,
							   NULL),
			    FALSE,
			    FALSE,
			    0);

	model = gtk_list_store_new (COLUMN_NUM,
				    G_TYPE_INT64,
				    G_TYPE_INT64,
				    G_TYPE_INT64,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll);

	priv->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);

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

	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (_("<b>Slices</b>"),
							   scroll,
							   NULL),
			    TRUE,
			    TRUE,
			    0);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    vbox,
			    TRUE,
			    TRUE,
			    0);
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
