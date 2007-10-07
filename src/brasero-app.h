/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/***************************************************************************
 *            brasero-app.h
 *
 *  Fri May 19 08:44:18 2006
 *  Copyright  2006  Philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

#ifndef _BRASERO_APP_H
#define _BRASERO_APP_H

#include <glib.h>

#include <gtk/gtkuimanager.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct _BraseroApp {
	GtkWidget *mainwin;
	GtkWidget *contents;
	GtkWidget *statusbar;
	GtkUIManager *manager;

	guint tooltip_ctx;

	gint width;
	gint height;

	gboolean is_maximised;
};
typedef struct _BraseroApp BraseroApp;

#ifdef __cplusplus
}
#endif

#endif /* _BRASERO_APP_H */



 
