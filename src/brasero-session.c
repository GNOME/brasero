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
 *            brasero-session.c
 *
 *  Thu May 18 18:32:37 2006
 *  Copyright  2006  Philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-client.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>

#include "brasero-app.h"
#include "brasero-project-manager.h"

static GnomeClient *client = NULL;

#define SESSION_VERSION "0.1"

static char*
brasero_session_get_file_path (void)
{
	return gnome_util_home_file ("brasero.session");
}

gboolean
brasero_session_load (BraseroApp *app)
{
	char *height_str = NULL;
	char *width_str = NULL;
	char *state_str = NULL;
	char *pane_str = NULL;
	char *version = NULL;
	int pane_pos = -1;
	int height = 480;
	int width = 640;
	int state = 0;

	char *session_path;
	xmlNodePtr item;
	xmlDocPtr session = NULL;

	session_path = brasero_session_get_file_path ();
	if (!session_path)
		goto end;

	session = xmlParseFile (session_path);
	g_free (session_path);

	if (!session)
		goto end;

	item = xmlDocGetRootElement (session);
	if (!item)
		goto end;

	if (xmlStrcmp (item->name, (const xmlChar *) "Session") || item->next)
		goto end;

	item = item->children;
	while (item) {
		if (!xmlStrcmp (item->name, (const xmlChar *) "version")) {
			if (version)
				goto end;

			version = (char *) xmlNodeListGetString (session,
								 item->xmlChildrenNode,
								 1);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "width")) {
			if (width_str)
				goto end;

			width_str = (char *) xmlNodeListGetString (session,
								   item->xmlChildrenNode,
								   1);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "height")) {
			if (height_str)
				goto end;

			height_str = (char *) xmlNodeListGetString (session,
								    item->xmlChildrenNode,
								    1);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "state")) {
			if (state_str)
				goto end;

			state_str = (char *) xmlNodeListGetString (session,
								   item->xmlChildrenNode,
								   1);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "pane")) {
			if (pane_str)
				goto end;

			pane_str = (char *) xmlNodeListGetString (session,
								  item->xmlChildrenNode,
								  1);
		}
		else if (item->type == XML_ELEMENT_NODE)
			goto end;

		item = item->next;
	}

	if (!version || strcmp (version, SESSION_VERSION))
		goto end;

	/* restore the window state */
	if (height_str)
		height = (int) g_strtod (height_str, NULL);

	if (width_str)
		width = (int) g_strtod (width_str, NULL);

	if (state_str)
		state = (int) g_strtod (state_str, NULL);

	if (pane_str)
		pane_pos = (int) g_strtod (pane_str, NULL);

end:
	if (height_str)
		g_free (height_str);

	if (width_str)
		g_free (width_str);

	if (state_str)
		g_free (state_str);

	if (pane_str)
		g_free (pane_str);

	if (version)
		g_free (version);

	xmlFreeDoc (session);

	gtk_window_set_default_size (GTK_WINDOW (app->mainwin),
				     width,
				     height);

	if (pane_pos > 0)
		brasero_project_manager_set_pos (BRASERO_PROJECT_MANAGER (app->contents),
						 pane_pos);

	if (state)
		gtk_window_maximize (GTK_WINDOW (app->mainwin));

	return TRUE;
}

gboolean
brasero_session_save (BraseroApp *app)
{
	int success;
	gint pane_pos;
	char *session_path;
	xmlTextWriter *session;

	session_path = brasero_session_get_file_path ();
	if (!session_path)
		return FALSE;

	/* write information */
	session = xmlNewTextWriterFilename (session_path, 0);
	if (!session) {
		g_free (session_path);
		return FALSE;
	}

	xmlTextWriterSetIndent (session, 1);
	xmlTextWriterSetIndentString (session, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (session,
					      NULL,
					      NULL,
					      NULL);
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (session,
					     (xmlChar *) "Session");
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteElement (session,
					     (xmlChar *) "version",
					     (xmlChar *) SESSION_VERSION);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteFormatElement (session,
						   (xmlChar *) "width",
						   "%i",
						   app->width);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteFormatElement (session,
						   (xmlChar *) "height",
						   "%i",
						   app->height);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteFormatElement (session,
						   (xmlChar *) "state",
						   "%i",
						   app->is_maximised);
	if (success < 0)
		goto error;

	/* saves internal pane geometry */
	pane_pos = brasero_project_manager_get_pos (BRASERO_PROJECT_MANAGER (app->contents));
	success = xmlTextWriterWriteFormatElement (session,
						   (xmlChar *) "pane",
						   "%i",
						   pane_pos);
	if (success < 0)
		goto error;

	success = xmlTextWriterEndElement (session);
	if (success < 0)
		goto error;

	xmlTextWriterEndDocument (session);
	xmlFreeTextWriter (session);
	g_free (session_path);

	return TRUE;

error:
	xmlTextWriterEndDocument (session);
	xmlFreeTextWriter (session);
	g_remove (session_path);
	g_free (session_path);

	return FALSE;
}

static void
brasero_session_die_cb (GnomeClient *client_loc,
			BraseroApp *app)
{
	brasero_session_save (app);
	gtk_widget_destroy (app->mainwin);
}

static gboolean
brasero_session_save_yourself_cb (GnomeClient *client_loc,
				  gint phase,
				  GnomeSaveStyle arg2,
				  gboolean is_shutting_down,
				  GnomeInteractStyle allowed_interaction,
				  gboolean fast_shutdown,
				  BraseroApp *app)
{
	/* if we want to save the current open project, this need a
	 * modification in BraseroProject to bypass ask_status in case
	 * DataDisc has not finished exploration */
	brasero_session_save (app);
	gtk_widget_destroy (app->mainwin);
	return TRUE; /* successs */
}

gboolean
brasero_session_connect (BraseroApp *app)
{
	/* connect to the session manager */
	if (client)
		return TRUE;

	client = gnome_master_client ();
	if (client) {
		g_signal_connect (client,
				  "die",
				  G_CALLBACK (brasero_session_die_cb),
				  app);
		g_signal_connect (client,
				  "save-yourself",
				  G_CALLBACK (brasero_session_save_yourself_cb),
				  app);

		return TRUE;
	}

	return FALSE;
}

void
brasero_session_disconnect (BraseroApp *app)
{
	g_signal_handlers_disconnect_by_func (client,
					      brasero_session_die_cb,
					      app);
	g_signal_handlers_disconnect_by_func (client,
					      brasero_session_save_yourself_cb,
					      app);

	client = NULL;
}
