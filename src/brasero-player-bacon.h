/***************************************************************************
 *            player-bacon.h
 *
 *  ven déc 30 11:29:33 2005
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

#ifndef PLAYER_BACON_H
#define PLAYER_BACON_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_PLAYER_BACON         (brasero_player_bacon_get_type ())
#define BRASERO_PLAYER_BACON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_PLAYER_BACON, BraseroPlayerBacon))
#define BRASERO_PLAYER_BACON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_PLAYER_BACON, BraseroPlayerBaconClass))
#define BRASERO_IS_PLAYER_BACON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_PLAYER_BACON))
#define BRASERO_IS_PLAYER_BACON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_PLAYER_BACON))
#define BRASERO_PLAYER_BACON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_PLAYER_BACON, BraseroPlayerBaconClass))

#define	PLAYER_BACON_WIDTH 120
#define	PLAYER_BACON_HEIGHT 90

typedef struct BraseroPlayerBaconPrivate BraseroPlayerBaconPrivate;

typedef enum {
	BACON_STATE_ERROR,
	BACON_STATE_READY,
	BACON_STATE_PAUSED,
	BACON_STATE_PLAYING
} BraseroPlayerBaconState;

typedef struct {
	GtkWidget parent;
	BraseroPlayerBaconPrivate *priv;
} BraseroPlayerBacon;

typedef struct {
	GtkWidgetClass parent_class;

	void	(*state_changed)	(BraseroPlayerBacon *bacon,
					 BraseroPlayerBaconState state);

	void	(*eof)			(BraseroPlayerBacon *bacon);

} BraseroPlayerBaconClass;

GType brasero_player_bacon_get_type (void);
GtkWidget *brasero_player_bacon_new (void);

void brasero_player_bacon_set_uri (BraseroPlayerBacon *bacon, const gchar *uri);
void brasero_player_bacon_set_volume (BraseroPlayerBacon *bacon, gdouble volume);
gboolean brasero_player_bacon_set_boundaries (BraseroPlayerBacon *bacon, gint64 start, gint64 end);
gboolean brasero_player_bacon_play (BraseroPlayerBacon *bacon);
gboolean brasero_player_bacon_stop (BraseroPlayerBacon *bacon);
gboolean brasero_player_bacon_set_pos (BraseroPlayerBacon *bacon, gdouble pos);
gboolean brasero_player_bacon_get_pos (BraseroPlayerBacon *bacon, gint64 *pos);
gdouble  brasero_player_bacon_get_volume (BraseroPlayerBacon *bacon);
gboolean brasero_player_bacon_forward (BraseroPlayerBacon *bacon, gint64 value);
gboolean brasero_player_bacon_backward (BraseroPlayerBacon *bacon, gint64 value);
G_END_DECLS

#endif /* PLAYER_BACON_H */
