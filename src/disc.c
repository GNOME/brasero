/***************************************************************************
 *            disc.c
 *
 *  dim nov 27 14:58:13 2005
 *  Copyright  2005  Rouquier Philippe
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

#include <glib.h>
#include <glib-object.h>

#include "brasero-marshal.h"
#include "disc.h"
 
static void brasero_disc_base_init (gpointer g_class);

typedef enum {
	SELECTION_CHANGED_SIGNAL,
	CONTENTS_CHANGED_SIGNAL,
	SIZE_CHANGED_SIGNAL,
	LAST_SIGNAL
} BraseroDiscSignalType;

static guint brasero_disc_signals [LAST_SIGNAL] = { 0 };

GType
brasero_disc_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDiscIface),
			brasero_disc_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};

		type = g_type_register_static(G_TYPE_INTERFACE, 
					      "BraseroDisc",
					      &our_info,
					      0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
brasero_disc_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	brasero_disc_signals [CONTENTS_CHANGED_SIGNAL] =
	    g_signal_new ("contents_changed",
			  BRASERO_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDiscIface, contents_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);
	brasero_disc_signals [SELECTION_CHANGED_SIGNAL] =
	    g_signal_new ("selection_changed",
			  BRASERO_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDiscIface, selection_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	brasero_disc_signals [SIZE_CHANGED_SIGNAL] =
	    g_signal_new ("size_changed",
			  BRASERO_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDiscIface, size_changed),
			  NULL,
			  NULL,
			  brasero_marshal_VOID__INT64,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT64);
	initialized = TRUE;
}

BraseroDiscResult
brasero_disc_add_uri (BraseroDisc *disc,
		      const gchar *uri)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (uri != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->add_uri)
		return (* iface->add_uri) (disc, uri);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

void
brasero_disc_delete_selected (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_if_fail (BRASERO_IS_DISC (disc));
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->delete_selected)
		(* iface->delete_selected) (disc);
}

void
brasero_disc_clear (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_if_fail (BRASERO_IS_DISC (disc));
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->clear)
		(* iface->clear) (disc);
}

void
brasero_disc_reset (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_if_fail (BRASERO_IS_DISC (disc));
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->reset)
		(* iface->reset) (disc);
}

BraseroDiscResult
brasero_disc_get_status (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_status)
		return (* iface->get_status) (disc);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

BraseroDiscResult
brasero_disc_get_track (BraseroDisc *disc,
			BraseroDiscTrack *track)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (track != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_track)
		return (* iface->get_track) (disc, track);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

BraseroDiscResult
brasero_disc_get_track_source (BraseroDisc *disc,
			       BraseroTrackSource **track,
			       BraseroImageFormat format)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (track != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_track_source)
		return (* iface->get_track_source) (disc, track, format);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

BraseroDiscResult
brasero_disc_load_track (BraseroDisc *disc,
			 BraseroDiscTrack *track)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (track != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->load_track)
		return (* iface->load_track) (disc, track);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

char *
brasero_disc_get_selected_uri (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), NULL);
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_selected_uri)
		return (* iface->get_selected_uri) (disc);

	return NULL;
}

void
brasero_disc_selection_changed (BraseroDisc *disc)
{
	g_return_if_fail (BRASERO_IS_DISC (disc));
	g_signal_emit (disc,
		       brasero_disc_signals [SELECTION_CHANGED_SIGNAL],
		       0);
}

void
brasero_disc_contents_changed (BraseroDisc *disc, int nb_files)
{
	g_return_if_fail (BRASERO_IS_DISC (disc));
	g_signal_emit (disc,
		       brasero_disc_signals [CONTENTS_CHANGED_SIGNAL],
		       0,
		       nb_files);
}

void
brasero_disc_size_changed (BraseroDisc *disc,
			   gint64 size)
{
	g_return_if_fail (BRASERO_IS_DISC (disc));

	g_signal_emit (disc,
		       brasero_disc_signals [SIZE_CHANGED_SIGNAL],
		       0,
		       size);
}

/************************************ ******************************************/
static void
brasero_track_clear_song (gpointer data)
{
	BraseroDiscSong *song;

	song = data;
	g_free (song->uri);
	g_free (song);
}

void
brasero_track_clear (BraseroDiscTrack *track)
{
	if (!track)
		return;

	if (track->type == BRASERO_DISC_TRACK_AUDIO) {
		g_slist_foreach (track->contents.tracks, (GFunc) brasero_track_clear_song, NULL);
		g_slist_free (track->contents.tracks);
	}
	else if (track->type == BRASERO_DISC_TRACK_DATA) {
		g_slist_foreach (track->contents.data.grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (track->contents.data.grafts);
		g_slist_foreach (track->contents.data.restored, (GFunc) g_free, NULL);
		g_slist_free (track->contents.data.restored);
		g_slist_foreach (track->contents.data.unreadable, (GFunc) g_free, NULL);
		g_slist_free (track->contents.data.unreadable);
		g_free (track->contents.data.label);
	}
	else if (track->type == BRASERO_DISC_TRACK_SOURCE)
		brasero_track_source_free (track->contents.src);
}

void
brasero_track_free (BraseroDiscTrack *track)
{
	brasero_track_clear (track);
	g_free (track);
}
