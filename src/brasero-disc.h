/***************************************************************************
 *            brasero-disc.h
 *
 *  dim nov 27 14:58:13 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

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

#ifndef DISC_H
#define DISC_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-project-parse.h"
#include "burn-basics.h"
#include "burn-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DISC         (brasero_disc_get_type ())
#define BRASERO_DISC(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_DISC, BraseroDisc))
#define BRASERO_IS_DISC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_DISC))
#define BRASERO_DISC_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), BRASERO_TYPE_DISC, BraseroDiscIface))

#define BRASERO_DISC_ACTION "DiscAction"


typedef enum {
	BRASERO_DISC_OK = 0,
	BRASERO_DISC_NOT_IN_TREE,
	BRASERO_DISC_NOT_READY,
	BRASERO_DISC_LOADING,
	BRASERO_DISC_BROKEN_SYMLINK,
	BRASERO_DISC_CANCELLED,
	BRASERO_DISC_ERROR_SIZE,
	BRASERO_DISC_ERROR_EMPTY_SELECTION,
	BRASERO_DISC_ERROR_FILE_NOT_FOUND,
	BRASERO_DISC_ERROR_UNREADABLE,
	BRASERO_DISC_ERROR_ALREADY_IN_TREE,
	BRASERO_DISC_ERROR_JOLIET,
	BRASERO_DISC_ERROR_FILE_TYPE,
	BRASERO_DISC_ERROR_THREAD,
	BRASERO_DISC_ERROR_UNKNOWN
} BraseroDiscResult;

typedef struct _BraseroDisc        BraseroDisc;
typedef struct _BraseroDiscIface   BraseroDiscIface;

struct _BraseroDiscIface {
	GTypeInterface g_iface;

	/* signals */
	void	(*selection_changed)			(BraseroDisc *disc);
	void	(*contents_changed)			(BraseroDisc *disc,
							 gint nb_files);
	void	(*size_changed)				(BraseroDisc *disc,
							 gint64 size);
	void	(*flags_changed)			(BraseroDisc *disc,
							 BraseroBurnFlag flags);

	/* virtual functions */
	BraseroDiscResult	(*get_status)		(BraseroDisc *disc,
							 gint *remaining,
							 gchar **current_task);

	BraseroDiscResult	(*load_track)		(BraseroDisc *disc,
							 BraseroDiscTrack *track);
	BraseroDiscResult	(*get_track)		(BraseroDisc *disc,
							 BraseroDiscTrack *track);

	BraseroDiscResult	(*set_session_param)	(BraseroDisc *disc,
							 BraseroBurnSession *session);
	BraseroDiscResult	(*set_session_contents)	(BraseroDisc *disc,
							 BraseroBurnSession *session);

	BraseroDiscResult	(*add_uri)		(BraseroDisc *disc,
							 const gchar *uri);

	gboolean		(*get_selected_uri)	(BraseroDisc *disc,
							 gchar **uri);
	gboolean		(*get_boundaries)	(BraseroDisc *disc,
							 gint64 *start,
							 gint64 *end);

	void			(*delete_selected)	(BraseroDisc *disc);
	void			(*clear)		(BraseroDisc *disc);
	void			(*reset)		(BraseroDisc *disc);

	guint			(*add_ui)		(BraseroDisc *disc,
							 GtkUIManager *manager,
							 GtkWidget *message);
};

GType brasero_disc_get_type ();

guint
brasero_disc_add_ui (BraseroDisc *disc,
		     GtkUIManager *manager,
		     GtkWidget *message);

BraseroDiscResult
brasero_disc_add_uri (BraseroDisc *disc, const gchar *escaped_uri);

gboolean
brasero_disc_get_selected_uri (BraseroDisc *disc, gchar **uri);

gboolean
brasero_disc_get_boundaries (BraseroDisc *disc,
			     gint64 *start,
			     gint64 *end);

void
brasero_disc_delete_selected (BraseroDisc *disc);
void
brasero_disc_clear (BraseroDisc *disc);
void
brasero_disc_reset (BraseroDisc *disc);

BraseroDiscResult
brasero_disc_get_status (BraseroDisc *disc,
			 gint *remaining,
			 gchar **current_task);

BraseroDiscResult
brasero_disc_get_track (BraseroDisc *disc,
			BraseroDiscTrack *track);
BraseroDiscResult
brasero_disc_load_track (BraseroDisc *disc,
			 BraseroDiscTrack *track);

BraseroDiscResult
brasero_disc_set_session_param (BraseroDisc *disc,
				BraseroBurnSession *session);
BraseroDiscResult
brasero_disc_set_session_contents (BraseroDisc *disc,
				   BraseroBurnSession *session);

void
brasero_disc_size_changed (BraseroDisc *disc,
			   gint64 size);
void
brasero_disc_flags_changed (BraseroDisc *disc,
			    BraseroBurnFlag flags);
void
brasero_disc_contents_changed (BraseroDisc *disc,
			       gint nb_files);
void
brasero_disc_selection_changed (BraseroDisc *disc);


GtkWidget *
brasero_disc_get_use_info_notebook (void);

#endif /* DISC_H */
