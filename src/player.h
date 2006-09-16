/***************************************************************************
*            player.h
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

#ifndef PLAYER_H
#define PLAYER_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkalignment.h>

#include "brasero-uri-container.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_PLAYER         (brasero_player_get_type ())
#define BRASERO_PLAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_PLAYER, BraseroPlayer))
#define BRASERO_PLAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_PLAYER, BraseroPlayerClass))
#define BRASERO_IS_PLAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_PLAYER))
#define BRASERO_IS_PLAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_PLAYER))
#define BRASERO_PLAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_PLAYER, BraseroPlayerClass))
typedef struct BraseroPlayerPrivate BraseroPlayerPrivate;

typedef struct {
	GtkAlignment parent;
	BraseroPlayerPrivate *priv;
} BraseroPlayer;

typedef struct {
	GtkAlignmentClass parent_class;
} BraseroPlayerClass;

GType brasero_player_get_type ();
GtkWidget *brasero_player_new ();

void
brasero_player_set_uri (BraseroPlayer *player, const char *uri);
void
brasero_player_add_source (BraseroPlayer *player, BraseroURIContainer *source);

#endif				/* PLAYER_H */

#endif
