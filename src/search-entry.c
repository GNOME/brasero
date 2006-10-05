/***************************************************************************
*            serach-entry.c
*
*  jeu mai 19 20:10:07 2005
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

#ifdef BUILD_SEARCH

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkcomboboxentry.h>
#include <gtk/gtkentry.h>
#include <gconf/gconf-client.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkdialog.h>

#include "search-entry.h"

static void brasero_search_entry_class_init (BraseroSearchEntryClass *klass);
static void brasero_search_entry_init (BraseroSearchEntry *sp);
static void brasero_search_entry_finalize (GObject *object);
static void brasero_search_entry_destroy (GtkObject *gtk_object);

struct BraseroSearchEntryPrivate {
	GtkWidget *button;
	GtkWidget *combo;
	GSList *history;
	GConfClient *client;
	guint cxn;
	gint search_id;

	char *keywords;

	GtkWidget *documents;
	GtkWidget *pictures;
	GtkWidget *music;
	GtkWidget *video;

	GtkTooltips *tooltip;
};

/* cut and pasted from nautilus */
struct _MimeTypeGroup{
	char *name;
	char *mimetypes[30];
};
typedef struct _MimeTypeGroup MimeTypeGroup;
const static MimeTypeGroup mime_type_groups [] = {
	{ N_("Find all available music"),
	  { "application/ogg",
	    "audio/ac3",
	    "audio/basic",
	    "audio/midi",
	    "audio/x-flac",
	    "audio/mp4",
	    "audio/mpeg",
	    "audio/x-vorbis+ogg", /* beagle stores ogg-vorbis files under this mime type */
	    "audio/x-mpeg",
	    "audio/x-ms-asx",
	    "audio/x-pn-realaudio",
	    NULL
	  }
	},
	{ N_("Find all available videos"),
	  { "video/mp4",
	    "video/3gpp",
	    "video/mpeg",
	    "video/quicktime",
	    "video/vivo",
	    "video/x-avi",
	    "video/x-mng",
	    "video/x-ms-asf",
	    "video/x-ms-wmv",
	    "video/x-msvideo",
	    "video/x-nsv",
	    "video/x-real-video",
	    NULL
	  }
	},
	{ N_("Find all available pictures"),
	  { "application/vnd.oasis.opendocument.image",
	    "application/x-krita",
	    "image/bmp",
	    "image/cgm",
	    "image/gif",
	    "image/jpeg",
	    "image/jpeg2000",
	    "image/png",
	    "image/svg+xml",
	    "image/tiff",
	    "image/x-compressed-xcf",
	    "image/x-pcx",
	    "image/x-photo-cd",
	    "image/x-psd",
	    "image/x-tga",
	    "image/x-xcf",
	    "application/illustrator",
	    "application/vnd.corel-draw",
	    "application/vnd.stardivision.draw",
	    "application/vnd.oasis.opendocument.graphics",
	    "application/x-dia-diagram",
	    "application/x-karbon",
	    "application/x-killustrator",
	    "application/x-kivio",
	    "application/x-kontour",
	    "application/x-wpg",
	    NULL
	  }
	},
	{ N_("Find all available documents"),
	  { "application/rtf",
	    "application/msword",
	    "application/vnd.sun.xml.writer",
	    "application/vnd.sun.xml.writer.global",
	    "application/vnd.sun.xml.writer.template",
	    "application/vnd.oasis.opendocument.text",
	    "application/vnd.oasis.opendocument.text-template",
	    "application/x-abiword",
	    "application/x-applix-word",
	    "application/x-mswrite",
	    "application/docbook+xml",
	    "application/x-kword",
	    "application/x-kword-crypt",
	    "application/x-lyx",
	    NULL
	  }
	},
	{ N_("Find all available spreadsheets"),
	  { "application/vnd.lotus-1-2-3",
	    "application/vnd.ms-excel",
	    "application/vnd.stardivision.calc",
	    "application/vnd.sun.xml.calc",
	    "application/vnd.oasis.opendocument.spreadsheet",
	    "application/x-applix-spreadsheet",
	    "application/x-gnumeric",
	    "application/x-kspread",
	    "application/x-kspread-crypt",
	    "application/x-quattropro",
	    "application/x-sc",
	    "application/x-siag",
	    NULL
	  }
	},
	{ N_("Find all available presentations"),
	  { "application/vnd.ms-powerpoint",
	    "application/vnd.sun.xml.impress",
	    "application/vnd.oasis.opendocument.presentation",
	    "application/x-magicpoint",
	    "application/x-kpresenter",
	    NULL
	  }
	},
	{ N_("Find all available Pdf / Postscripts"),
	  { "application/pdf",
	    "application/postscript",
	    "application/x-dvi",
	    "image/x-eps",
	    NULL
	  }
	},
	{NULL},
	{ N_("Text File"),
	  { "text/plain",
	    NULL
	  }
	},
	{ NULL }
};

enum {
	BRASERO_SEARCH_ENTRY_DISPLAY_COL,
	BRASERO_SEARCH_ENTRY_BACKGRD_COL,
	BRASERO_SEARCH_ENTRY_NB_COL
};

static GObjectClass *parent_class = NULL;

#define BRASERO_CONF_DIR "/apps/brasero"
#define BRASERO_SEARCH_ENTRY_HISTORY_KEY "/apps/brasero/search_history"
#define BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS	10

static void brasero_search_entry_history_changed_cb (GConfClient *client,
						      guint cxn_id,
						      GConfEntry *entry,
						      BraseroSearchEntry *widget);
static void brasero_search_entry_button_clicked_cb (GtkButton *button,
						      BraseroSearchEntry *entry);
static void brasero_search_entry_entry_activated_cb (GtkComboBox *combo,
						      BraseroSearchEntry *entry);
static void brasero_search_entry_activated (BraseroSearchEntry *entry, gboolean query_now);
static void brasero_search_entry_set_history (BraseroSearchEntry *entry);
static void brasero_search_entry_save_history (BraseroSearchEntry *entry);
static void brasero_search_entry_add_current_keyword_to_history (BraseroSearchEntry *entry);
static void brasero_search_entry_category_clicked_cb (GtkWidget *button, BraseroSearchEntry *entry);

typedef enum {
	ACTIVATED_SIGNAL,
	LAST_SIGNAL
} BraseroSearchEntrySignalType;

static guint brasero_search_entry_signals[LAST_SIGNAL] = { 0 };

GType
brasero_search_entry_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroSearchEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_search_entry_class_init,
			NULL,
			NULL,
			sizeof (BraseroSearchEntry),
			0,
			(GInstanceInitFunc) brasero_search_entry_init,
		};

		type = g_type_register_static (GTK_TYPE_VBOX,
					       "BraseroSearchEntry",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_search_entry_class_init (BraseroSearchEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_search_entry_finalize;
	gtk_object_class->destroy = brasero_search_entry_destroy;

	brasero_search_entry_signals[ACTIVATED_SIGNAL] =
	    g_signal_new ("activated", G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (BraseroSearchEntryClass,
					   activated), NULL, NULL,
			  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static gboolean
brasero_search_entry_separator_func (GtkTreeModel *model,
				     GtkTreeIter *iter,
				     gpointer data)
{
	gchar *display;
	gboolean result;

	gtk_tree_model_get (model, iter,
			    BRASERO_SEARCH_ENTRY_DISPLAY_COL, &display,
			    -1);

	if (display) {
		g_free (display);
		result = FALSE;
	}
	else
		result = TRUE;

	return result;
}

static void
brasero_search_entry_init (BraseroSearchEntry *obj)
{
	GtkWidget *box;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry;
	GError *error = NULL;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkEntryCompletion *completion;

	gtk_box_set_spacing (GTK_BOX (obj), 6);
	obj->priv = g_new0 (BraseroSearchEntryPrivate, 1);

	box = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (obj), box, FALSE, FALSE, 0);

	label = gtk_label_new (_("<b>Search:\t</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	obj->priv->button = gtk_button_new_from_stock (GTK_STOCK_FIND);
	/* gtk_widget_set_sensitive (obj->priv->button, FALSE); */
	gtk_box_pack_end (GTK_BOX (box), obj->priv->button, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (obj->priv->button),
			  "clicked",
			  G_CALLBACK (brasero_search_entry_button_clicked_cb),
			  obj);

	/* Set up the combo box */
	store = gtk_list_store_new (BRASERO_SEARCH_ENTRY_NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	obj->priv->combo = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (store),
							       BRASERO_SEARCH_ENTRY_DISPLAY_COL);
	g_object_unref (store);
	g_signal_connect (G_OBJECT (obj->priv->combo),
			  "changed",
			  G_CALLBACK (brasero_search_entry_entry_activated_cb),
			  obj);

	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (obj->priv->combo),
					      brasero_search_entry_separator_func,
					      obj,
					      NULL);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (obj->priv->combo));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->priv->combo),
				    renderer,
				    TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (obj->priv->combo),
					renderer,
					"text", BRASERO_SEARCH_ENTRY_DISPLAY_COL,
					"background", BRASERO_SEARCH_ENTRY_BACKGRD_COL,
					NULL);

	/* set auto completion */
	entry = GTK_BIN (obj->priv->combo)->child;
	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
	gtk_entry_completion_set_text_column (completion, BRASERO_SEARCH_ENTRY_DISPLAY_COL);
	gtk_entry_set_completion (GTK_ENTRY (entry), completion);

	gtk_box_pack_start (GTK_BOX (box),
			    obj->priv->combo,
			    TRUE,
			    TRUE,
			    0);

	/* table */
	table = gtk_table_new (2, 3, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);

	gtk_box_pack_end (GTK_BOX (obj), table, FALSE, FALSE, 0);

	label = gtk_label_new (_("<b>only in\t</b>"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  0, 1,
			  0,
			  0,
			  0,
			  0);

	obj->priv->documents = gtk_check_button_new_with_mnemonic (_("_text documents"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->documents), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->documents,
			  1, 2,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->documents,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	obj->priv->pictures = gtk_check_button_new_with_mnemonic (_("_pictures"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->pictures), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->pictures,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->pictures,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	obj->priv->music = gtk_check_button_new_with_mnemonic (_("_music"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->music), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->music,
			  2, 3,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->music,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	obj->priv->video = gtk_check_button_new_with_mnemonic (_("_video"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->video), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->video,
			  2, 3,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->video,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	/* add tooltips */
	obj->priv->tooltip = gtk_tooltips_new ();

	gtk_tooltips_set_tip (obj->priv->tooltip, 
			      GTK_BIN (obj->priv->combo)->child,
			      _("Type your keywords or choose 'All files' from the menu"),
			      NULL);
	gtk_tooltips_set_tip (obj->priv->tooltip, 
			      obj->priv->pictures,
			      _("Select if you want to search among image files only"),
			      NULL);
	gtk_tooltips_set_tip (obj->priv->tooltip, 
			      obj->priv->video,
			      _("Select if you want to search among video files only"),
			      NULL);
	gtk_tooltips_set_tip (obj->priv->tooltip, 
			      obj->priv->music,
			      _("Select if you want to search among audio files only"),
			      NULL);
	gtk_tooltips_set_tip (obj->priv->tooltip, 
			      obj->priv->documents,
			      _("Select if you want to search among your text documents only"),
			      NULL);
	gtk_tooltips_set_tip (obj->priv->tooltip, 
			      obj->priv->button,
			      _("Click to start the search"),
			      NULL);

	/* Set up GConf Client */
	obj->priv->client = gconf_client_get_default ();
	if (obj->priv->client) {
		gconf_client_add_dir (obj->priv->client, BRASERO_CONF_DIR,
				      GCONF_CLIENT_PRELOAD_NONE, &error);
		if (error) {
			g_warning ("ERROR : %s\n", error->message);
			g_error_free (error);
			error = NULL;
		}
		else
			obj->priv->cxn = gconf_client_notify_add (obj->priv->client,
								  BRASERO_SEARCH_ENTRY_HISTORY_KEY,
								  (GConfClientNotifyFunc) brasero_search_entry_history_changed_cb,
								  obj, 
								  NULL,
								  &error);

		if (error) {
			g_warning ("ERROR : %s\n", error->message);
			g_error_free (error);
			error = NULL;
		}

		obj->priv->history = gconf_client_get_list (obj->priv->client,
							    BRASERO_SEARCH_ENTRY_HISTORY_KEY,
							    GCONF_VALUE_STRING,
							    &error);

		if (error) {
			g_warning ("ERROR : %s\n", error->message);
			g_error_free (error);
		}
		else if (obj->priv->history)
			brasero_search_entry_set_history (obj);
	}
	else
		g_warning ("ERROR : could not connect to GCONF.\n");
}

static void
brasero_search_entry_destroy (GtkObject *gtk_object)
{
	BraseroSearchEntry *cobj;

	cobj = BRASERO_SEARCH_ENTRY (gtk_object);

	/* release gconf client */
	g_object_unref (cobj->priv->client);

	GTK_OBJECT_CLASS (parent_class)->destroy (gtk_object);
}

static void
brasero_search_entry_finalize (GObject *object)
{
	BraseroSearchEntry *cobj;

	cobj = BRASERO_SEARCH_ENTRY (object);

	g_slist_foreach (cobj->priv->history, (GFunc) g_free, NULL);
	g_slist_free (cobj->priv->history);
	cobj->priv->history = NULL;

	if (cobj->priv->search_id) {
		g_source_remove (cobj->priv->search_id);
		cobj->priv->search_id = 0;
	}

	if (cobj->priv->tooltip)
		g_object_ref_sink (GTK_OBJECT (cobj->priv->tooltip));
		cobj->priv->tooltip = NULL;

	g_free (cobj->priv);
	cobj->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_search_entry_new ()
{
	BraseroSearchEntry *obj;

	obj = BRASERO_SEARCH_ENTRY (g_object_new (BRASERO_TYPE_SEARCH_ENTRY, NULL));

	return GTK_WIDGET (obj);
}

static void
brasero_search_entry_category_clicked_cb (GtkWidget *button,
					  BraseroSearchEntry *entry)
{
	if (entry->priv->keywords) {
		g_free (entry->priv->keywords);
		entry->priv->keywords = NULL;
	}
}

static void
brasero_search_entry_history_changed_cb (GConfClient *client,
					 guint cnx_id,
					 GConfEntry *entry,
					 BraseroSearchEntry *widget)
{
	GConfValue *value;
	GSList *list = NULL, *iter;
	const char *keywords;

	/* a few checks */
	value = gconf_entry_get_value (entry);
	if (value->type != GCONF_VALUE_LIST)
		return;

	if (gconf_value_get_list_type (value) != GCONF_VALUE_STRING)
		return;

	/* clears up history */
	g_slist_foreach (widget->priv->history, (GFunc) g_free, NULL);
	g_slist_free (widget->priv->history);
	widget->priv->history = NULL;

	/* get the new history */
	list = gconf_value_get_list (value);
	for (iter = list; iter && g_slist_length (widget->priv->history) < BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS; iter = iter->next) {
		value = (GConfValue *) iter->data;
		keywords = gconf_value_get_string (value);
		widget->priv->history = g_slist_append (widget->priv->history,
							g_strdup (keywords));
	}

	brasero_search_entry_set_history (widget);
}

static void
brasero_search_entry_button_clicked_cb (GtkButton *button,
					BraseroSearchEntry *entry)
{
	brasero_search_entry_activated (entry, TRUE);
}

static void
brasero_search_entry_entry_activated_cb (GtkComboBox *combo,
					 BraseroSearchEntry *entry)
{
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (entry->priv->combo)) != -1)
		brasero_search_entry_activated (entry, TRUE);
	else
		brasero_search_entry_activated (entry, FALSE);
}

static void
brasero_search_entry_check_keywords (BraseroSearchEntry *entry) 
{
	const char *keywords;
	char *tmp;

	/* NOTE: there is a strange thing done in beagle (a BUG ?) in
	 * beagle_query_part_human_set_string the string arg is const
	 * but instead of copying the string for internal use it just
	 * sets a pointer to the string; that's why we can't free the
	 * keywords until the query has been sent and even then ... */

	keywords = gtk_entry_get_text (GTK_ENTRY (GTK_BIN (entry->priv->combo)->child));
	if (!keywords)
		return;

	/* we make sure the search is not empty */
	tmp = g_strdup (keywords);
	tmp = g_strchug (tmp);
	if (*tmp == '\0') {
		g_free (tmp);
		return;
	}

	/* make sure it's has effectively changed */
	if (entry->priv->keywords && !strcmp (tmp, entry->priv->keywords)) {
		g_free (tmp);
		return;
	}

	g_free (entry->priv->keywords);
	entry->priv->keywords = tmp;

	g_signal_emit (entry,
		       brasero_search_entry_signals [ACTIVATED_SIGNAL],
		       0);
	brasero_search_entry_add_current_keyword_to_history (entry);
/*	gtk_widget_set_sensitive (entry->priv->button, FALSE); */
}

/* to avoid sending a query every time the user types something
 * we simply check that the keywords haven't changed for 2 secs */
static gboolean
brasero_search_entry_activated_real (BraseroSearchEntry *entry)
{
	entry->priv->search_id = 0;
	brasero_search_entry_check_keywords (entry);
	return FALSE;
}

static void
brasero_search_entry_activated (BraseroSearchEntry *entry,
				gboolean search_now)
{
	/* we reset the timer */
	if (entry->priv->search_id) {
		g_source_remove (entry->priv->search_id);
		entry->priv->search_id = 0;
	}

	if (!search_now) {
		/* gtk_widget_set_sensitive (entry->priv->button, TRUE); */
		entry->priv->search_id = g_timeout_add (2000,
							(GSourceFunc) brasero_search_entry_activated_real,
							entry);
	}
	else
		brasero_search_entry_check_keywords (entry);
}

static void
brasero_search_entry_set_history (BraseroSearchEntry *entry)
{
	int i;
	GSList *iter;
	GtkTreeIter row;
	GtkListStore *store;

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (entry->priv->combo)));
	gtk_list_store_clear (GTK_LIST_STORE (store));

	if (entry->priv->history == NULL)
		return;

	i = 0;
	for (iter = entry->priv->history; iter && i < BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS; iter = iter->next) {
		gtk_list_store_append (store, &row);
		gtk_list_store_set (store, &row,
				    BRASERO_SEARCH_ENTRY_DISPLAY_COL, iter->data,
				    BRASERO_SEARCH_ENTRY_BACKGRD_COL, NULL,
				    -1);
		i ++;
	}

	if (iter) {
		GSList *next;

		next = iter->next;

		/* if there are other items simply free them */
		entry->priv->history = g_slist_remove_link (entry->priv->history, iter);
		g_free (iter->data);
		g_slist_free (iter);

		g_slist_foreach (next, (GFunc) g_free, NULL);
		g_slist_free (next);
	}

	/* separator */
	gtk_list_store_append (store, &row);
	gtk_list_store_set (store, &row,
			    BRASERO_SEARCH_ENTRY_DISPLAY_COL, NULL,
			    BRASERO_SEARCH_ENTRY_BACKGRD_COL, NULL,
			    -1);

	/* all files entry */
	gtk_list_store_append (store, &row);
	gtk_list_store_set (store, &row,
			    BRASERO_SEARCH_ENTRY_DISPLAY_COL, _("All files"),
			    BRASERO_SEARCH_ENTRY_BACKGRD_COL, NULL,
			    //BRASERO_SEARCH_ENTRY_BACKGRD_COL, "grey90",
			    -1);
}

static void
brasero_search_entry_save_history (BraseroSearchEntry *entry)
{
	GError *error = NULL;

	gconf_client_notify_remove (entry->priv->client, entry->priv->cxn);
	gconf_client_set_list (entry->priv->client,
			       BRASERO_SEARCH_ENTRY_HISTORY_KEY,
			       GCONF_VALUE_STRING,
			       entry->priv->history, &error);
	if (error) {
		g_warning ("ERROR : %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	entry->priv->cxn = gconf_client_notify_add (entry->priv->client,
						    BRASERO_SEARCH_ENTRY_HISTORY_KEY,
						    (GConfClientNotifyFunc)
						    brasero_search_entry_history_changed_cb,
						    entry, NULL, &error);
	if (error) {
		g_warning ("ERROR : %s\n", error->message);
		g_error_free (error);
	}
}

static void
brasero_search_entry_add_current_keyword_to_history (BraseroSearchEntry *entry)
{
	const char *keywords = NULL;
	GSList *iter;

	/* we don't want to add static entry */
	keywords =  gtk_entry_get_text (GTK_ENTRY (GTK_BIN (entry->priv->combo)->child));
	if (!keywords || !strcmp (keywords, _("All files")))
		return;

	/* make sure the item is not already in the list
	 * otherwise just move it up in first position */
	for (iter = entry->priv->history; iter; iter = iter->next) {
		if (!strcmp (keywords, (char *) iter->data)) {
			keywords = iter->data;
			entry->priv->history = g_slist_remove (entry->priv->history,
							       keywords);
			entry->priv->history = g_slist_prepend (entry->priv->history,
								(char *) keywords);
			goto end;
		}
	}

	if (g_slist_length (entry->priv->history) == BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS) {
		iter = g_slist_last (entry->priv->history);
		entry->priv->history = g_slist_remove (entry->priv->history, iter);
	}

	entry->priv->history = g_slist_prepend (entry->priv->history,
						g_strdup (keywords));

      end:
	brasero_search_entry_save_history (entry);
	brasero_search_entry_set_history (entry);
}

static void
_add_mime_types_to_query (BeagleQuery *query, const MimeTypeGroup *group)
{
	gchar **mime;

	mime = (gchar **) group->mimetypes;
	while (*mime) {
		beagle_query_add_mime_type (query, *mime);
		mime ++;
	}
}

BeagleQuery *
brasero_search_entry_get_query (BraseroSearchEntry *entry)
{
	BeagleQuery *query;

	query = beagle_query_new ();
	beagle_query_add_source (query, "Files");

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->documents))) {
		_add_mime_types_to_query (query, mime_type_groups + 3);
	}
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->pictures))) {
		_add_mime_types_to_query (query, mime_type_groups + 2);
	}
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->music))) {
		_add_mime_types_to_query (query, mime_type_groups);
	}
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->video))) {
		_add_mime_types_to_query (query, mime_type_groups + 1);
	}

	if (strcmp (entry->priv->keywords, _("All files")))
		beagle_query_add_text (query, entry->priv->keywords);

	return query;
}

#endif /*BUILD_SEARCH*/
