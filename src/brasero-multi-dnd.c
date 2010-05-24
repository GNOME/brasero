/***************************************************************************
 *            multi-dnd.c
 *
 *  Wed Sep 27 17:34:41 2006
 *  Copyright  2006  Rouquier Philippe
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <gtk/gtk.h>

#include "brasero-multi-dnd.h"

#include "brasero-track-data-cfg.h"
#include "eggtreemultidnd.h"

static gboolean
brasero_multi_DND_row_draggable (EggTreeMultiDragSource *drag_source,
				 GList *path_list)
{
	GList *iter;

	for (iter = path_list; iter && iter->data; iter = iter->next) {
		GtkTreePath *path;
		GtkTreeRowReference *reference;

		reference = iter->data;
		path = gtk_tree_row_reference_get_path (reference);
		if (gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (drag_source), path) == FALSE) {
			gtk_tree_path_free (path);
			return FALSE;
		}
		gtk_tree_path_free (path);
	}

	return TRUE;
}

static gboolean
brasero_multi_DND_drag_data_get (EggTreeMultiDragSource *drag_source, 
				 GList *path_list, 
				 GtkSelectionData *selection_data)
{
	GtkSelectionData *selection_tmp;
	GList *uris_list = NULL;
	gchar **uris;
	GList *iter;
	gint i;

	if (gtk_selection_data_get_target (selection_data) != gdk_atom_intern ("text/uri-list", TRUE))
		return TRUE;

	for (iter = path_list; iter && iter->data; iter = iter->next) {
		gchar **tmp;
		gboolean result;
		GtkTreePath *path;
		GtkTreeRowReference *reference;

		reference = iter->data;
		path = gtk_tree_row_reference_get_path (reference);

		selection_tmp = gtk_selection_data_copy (selection_data);
		result = gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (drag_source),
							     path,
							     selection_tmp);
		gtk_tree_path_free (path);

		uris = gtk_selection_data_get_uris (selection_tmp);
		if (!uris) {
			const guchar *selection_data_raw;

			selection_data_raw = gtk_selection_data_get_data (selection_data);
			uris = g_uri_list_extract_uris ((gchar *) selection_data_raw);
		}

		for (tmp = uris; tmp && *tmp; tmp++)
			uris_list = g_list_prepend (uris_list, *tmp);
		g_free (uris);

		gtk_selection_data_free (selection_tmp);

		if (!result) {
			g_list_foreach (uris_list, (GFunc) g_free, NULL);
			g_list_free (uris_list);
			return FALSE;
		}
	}

	uris = g_new0 (gchar*, g_list_length (uris_list) + 1);
	uris_list = g_list_reverse (uris_list);
	for (iter = uris_list, i = 0; iter; i++, iter = iter->next)
		uris [i] = iter->data;

	g_list_free (uris_list);

	gtk_selection_data_set_uris (selection_data, uris);
	g_strfreev (uris);
	return TRUE;
}

static gboolean
brasero_multi_DND_drag_data_delete (EggTreeMultiDragSource *drag_source,
				    GList *path_list)
{
	return TRUE;
}

static void
brasero_multi_DND_drag_source_init (EggTreeMultiDragSourceIface *iface)
{
	iface->row_draggable = brasero_multi_DND_row_draggable;
	iface->drag_data_get = brasero_multi_DND_drag_data_get;
	iface->drag_data_delete = brasero_multi_DND_drag_data_delete;
}

static const GInterfaceInfo multi_DND_drag_source_info = {
	(GInterfaceInitFunc) brasero_multi_DND_drag_source_init,
	NULL,
	NULL
};

static gboolean
brasero_data_track_cfg_multi_DND_row_draggable (EggTreeMultiDragSource *drag_source,
						GList *path_list)
{
	GList *iter;

	/* at least one row must not be an imported row. */
	for (iter = path_list; iter && iter->data; iter = iter->next) {
		GtkTreePath *path;
		GtkTreeRowReference *reference;

		reference = iter->data;
		path = gtk_tree_row_reference_get_path (reference);
		if (gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (drag_source), path)) {
			gtk_tree_path_free (path);
			return TRUE;
		}
		gtk_tree_path_free (path);
	}

	return FALSE;
}

static gboolean
brasero_data_track_cfg_multi_DND_drag_data_get (EggTreeMultiDragSource *drag_source,
						GList *path_list,
						GtkSelectionData *selection_data)
{
	if (gtk_selection_data_get_target (selection_data) == gdk_atom_intern (BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST, TRUE)) {
		gtk_selection_data_set (selection_data,
					gdk_atom_intern_static_string (BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST),
					8,
					(void *) path_list,
					sizeof (GList));
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_data_track_cfg_multi_DND_drag_data_delete (EggTreeMultiDragSource *drag_source,
						   GList *path_list)
{
	/* NOTE: it's not the data in the selection_data here that should be
	 * deleted but rather the rows selected when there is a move. FALSE
	 * here means that we didn't delete anything. */
	/* return TRUE to stop other handlers */
	return TRUE;
}

static void
brasero_data_track_cfg_multi_DND_drag_source_init (EggTreeMultiDragSourceIface *iface)
{
	iface->row_draggable = brasero_data_track_cfg_multi_DND_row_draggable;
	iface->drag_data_get = brasero_data_track_cfg_multi_DND_drag_data_get;
	iface->drag_data_delete = brasero_data_track_cfg_multi_DND_drag_data_delete;
}

static const GInterfaceInfo brasero_data_track_cfg_multi_DND_drag_source_info = {
	(GInterfaceInitFunc) brasero_data_track_cfg_multi_DND_drag_source_init,
	NULL,
	NULL
};

gboolean
brasero_enable_multi_DND_for_model_type (GType type)
{
	g_type_add_interface_static (type,
				     EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
				     &multi_DND_drag_source_info);
	return TRUE;
}

void
brasero_enable_multi_DND (void)
{
	g_type_add_interface_static (GTK_TYPE_TREE_MODEL_SORT,
				     EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
				     &multi_DND_drag_source_info);
	g_type_add_interface_static (GTK_TYPE_TREE_STORE,
				     EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
				     &multi_DND_drag_source_info);
	g_type_add_interface_static (GTK_TYPE_LIST_STORE,
				     EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
				     &multi_DND_drag_source_info);
	g_type_add_interface_static (BRASERO_TYPE_TRACK_DATA_CFG,
				     EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
				     &brasero_data_track_cfg_multi_DND_drag_source_info);
}
 
