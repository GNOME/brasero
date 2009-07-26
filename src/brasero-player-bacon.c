/***************************************************************************
 *            player-bacon.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gdk/gdkx.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>

#include "brasero-player-bacon.h"
 
static void brasero_player_bacon_class_init(BraseroPlayerBaconClass *klass);
static void brasero_player_bacon_init(BraseroPlayerBacon *sp);
static void brasero_player_bacon_finalize(GObject *object);
static void brasero_player_bacon_destroy (GtkObject *object);

static void brasero_player_bacon_set_property (GObject *obj,
					       guint prop_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void brasero_player_bacon_get_property (GObject *obj,
					       guint prop_id,
					       GValue *value,
					       GParamSpec *pspec);

static void brasero_player_bacon_realize (GtkWidget *widget);
static void brasero_player_bacon_unrealize (GtkWidget *widget);
static void brasero_player_bacon_map (GtkWidget *widget);
static void brasero_player_bacon_unmap (GtkWidget *widget);

static gboolean brasero_player_bacon_expose (GtkWidget *widget,
					     GdkEventExpose *event);
static void brasero_player_bacon_size_request (GtkWidget *widget,
					       GtkRequisition *requisition);
static void brasero_player_bacon_size_allocate (GtkWidget *widget,
						GtkAllocation *allocation);

static gboolean brasero_player_bacon_bus_messages (GstBus *bus,
						   GstMessage *msg,
						   BraseroPlayerBacon *bacon);

struct BraseroPlayerBaconPrivate {
	GstElement *pipe;
	GstState state;

	GstXOverlay *xoverlay;

	gchar *uri;
	gint64 end;
};

enum {
	PROP_NONE,
	PROP_URI
};

typedef enum {
	STATE_CHANGED_SIGNAL,
	EOF_SIGNAL,
	LAST_SIGNAL
} BraseroPlayerBaconSignalType;
static guint brasero_player_bacon_signals [LAST_SIGNAL] = { 0 };

#define GCONF_PLAYER_VOLUME	"/apps/brasero/display/volume"

static GObjectClass *parent_class = NULL;

GType
brasero_player_bacon_get_type (void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroPlayerBaconClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_player_bacon_class_init,
			NULL,
			NULL,
			sizeof (BraseroPlayerBacon),
			0,
			(GInstanceInitFunc)brasero_player_bacon_init,
		};

		type = g_type_register_static (GTK_TYPE_WIDGET, 
					       "BraseroPlayerBacon",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_player_bacon_class_init (BraseroPlayerBaconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);

	object_class->finalize = brasero_player_bacon_finalize;
	object_class->set_property = brasero_player_bacon_set_property;
	object_class->get_property = brasero_player_bacon_get_property;
	gtk_object_class->destroy = brasero_player_bacon_destroy;

	widget_class->expose_event = brasero_player_bacon_expose;
	widget_class->map = brasero_player_bacon_map;
	widget_class->unmap = brasero_player_bacon_unmap;
	widget_class->realize = brasero_player_bacon_realize;
	widget_class->unrealize = brasero_player_bacon_unrealize;
	widget_class->size_request = brasero_player_bacon_size_request;
	widget_class->size_allocate = brasero_player_bacon_size_allocate;

	brasero_player_bacon_signals [STATE_CHANGED_SIGNAL] = 
			g_signal_new ("state-change",
				      G_TYPE_FROM_CLASS (klass),
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (BraseroPlayerBaconClass, state_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);

	brasero_player_bacon_signals [EOF_SIGNAL] = 
			g_signal_new ("eof",
				      G_TYPE_FROM_CLASS (klass),
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (BraseroPlayerBaconClass, eof),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);

	g_object_class_install_property (object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
							      "The uri of the media",
							      "The uri of the media",
							      NULL,
							      G_PARAM_READWRITE));
}

static void
brasero_player_bacon_set_property (GObject *obj,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	BraseroPlayerBacon *bacon;
	const char *uri;

	bacon = BRASERO_PLAYER_BACON (obj);

	switch (prop_id) {
	case PROP_URI:
		uri = g_value_get_string (value);
		brasero_player_bacon_set_uri (bacon, uri);
		break;

	default:
		break;
	}	
}

static void
brasero_player_bacon_get_property (GObject *obj,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	BraseroPlayerBacon *bacon;

	bacon = BRASERO_PLAYER_BACON (obj);
	switch (prop_id) {
	case PROP_URI:
		g_value_set_string (value, bacon->priv->uri);
		break;

	default:
		break;
	}
}

static void
brasero_player_bacon_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	BraseroPlayerBacon *bacon;
	gint attributes_mask;

	bacon = BRASERO_PLAYER_BACON (widget);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes,
					 attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	//gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
}

static void
brasero_player_bacon_unrealize (GtkWidget *widget)
{
	BraseroPlayerBacon *bacon;

	bacon = BRASERO_PLAYER_BACON (widget);

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
brasero_player_bacon_map (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	gdk_window_show (widget->window);

	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
}

static void
brasero_player_bacon_unmap (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	gdk_window_hide (widget->window);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
}

static gboolean
brasero_player_bacon_expose (GtkWidget *widget, GdkEventExpose *event)
{
	BraseroPlayerBacon *bacon;

	if (event && event->count > 0)
		return TRUE;

	g_return_val_if_fail (widget != NULL, FALSE);
	bacon = BRASERO_PLAYER_BACON (widget);

	if (bacon->priv->xoverlay
	&&  GST_IS_X_OVERLAY (bacon->priv->xoverlay)
	&&  bacon->priv->state >= GST_STATE_PAUSED) {
		gst_x_overlay_expose (bacon->priv->xoverlay);
	}
	else if (widget->window)
		gdk_window_clear (widget->window);

	return TRUE;
}

static void
brasero_player_bacon_size_request (GtkWidget *widget,
				   GtkRequisition *requisition)
{
	BraseroPlayerBacon *bacon;

	g_return_if_fail (widget != NULL);
	bacon = BRASERO_PLAYER_BACON (widget);

	requisition->width = PLAYER_BACON_WIDTH;
	requisition->height = PLAYER_BACON_HEIGHT;

	if (GTK_WIDGET_CLASS (parent_class)->size_request)
		GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);
}

static void
brasero_player_bacon_size_allocate (GtkWidget *widget,
				    GtkAllocation *allocation)
{
	int screen_x, screen_y;
	BraseroPlayerBacon *bacon;
	gfloat screen_width, screen_height, ratio;

	g_return_if_fail (widget != NULL);
	bacon = BRASERO_PLAYER_BACON (widget);

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	if (bacon->priv->xoverlay) {
		screen_width = allocation->width;
		screen_height = allocation->height;
	
		if ((gfloat) screen_width / PLAYER_BACON_WIDTH > 
		    (gfloat) screen_height / PLAYER_BACON_HEIGHT)
			ratio = (gfloat) screen_height / PLAYER_BACON_HEIGHT;
		else
			ratio = (gfloat) screen_width / PLAYER_BACON_WIDTH;
		
		screen_width = (gdouble) PLAYER_BACON_WIDTH * ratio;
		screen_height = (gdouble) PLAYER_BACON_HEIGHT * ratio;
		screen_x = allocation->x + (allocation->width - (gint) screen_width) / 2;
		screen_y = allocation->y + (allocation->height - (gint) screen_height) / 2;
	
		gdk_window_move_resize (widget->window,
					screen_x,
					screen_y,
					(gint) screen_width,
					(gint) screen_height);
		widget->allocation = *allocation;
	}
	else if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
}

static void
brasero_player_bacon_destroy (GtkObject *obj)
{
	BraseroPlayerBacon *cobj;

	cobj = BRASERO_PLAYER_BACON (obj);

	/* save volume */
	if (cobj->priv->pipe) {
		GConfClient *client;
		gdouble volume;

		client = gconf_client_get_default ();
		g_object_get (cobj->priv->pipe,
			      "volume", &volume,
			      NULL);

		volume = gconf_client_set_int (client,
					       GCONF_PLAYER_VOLUME,
					       (gint) (volume * 100.0),
					       NULL);
		g_object_unref (client);
	}

	if (cobj->priv->xoverlay
	&&  GST_IS_X_OVERLAY (cobj->priv->xoverlay)) {
		cobj->priv->xoverlay = NULL;
	}

	if (cobj->priv->pipe) {
		gst_element_set_state (cobj->priv->pipe, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (cobj->priv->pipe));
		cobj->priv->pipe = NULL;
	}

	if (cobj->priv->uri) {
		g_free (cobj->priv->uri);
		cobj->priv->uri = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static void
brasero_player_bacon_finalize (GObject *object)
{
	BraseroPlayerBacon *cobj;

	cobj = BRASERO_PLAYER_BACON (object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_player_bacon_new (void)
{
	BraseroPlayerBacon *obj;
	
	obj = BRASERO_PLAYER_BACON (g_object_new (BRASERO_TYPE_PLAYER_BACON, NULL));
	
	return GTK_WIDGET (obj);
}

static GstBusSyncReply
brasero_player_bacon_bus_messages_handler (GstBus *bus,
					   GstMessage *message,
					   BraseroPlayerBacon *bacon)
{
	const GstStructure *structure;
	XID window;

	structure = gst_message_get_structure (message);
	if (!structure)
		return GST_BUS_PASS;

	if (!gst_structure_has_name (structure, "prepare-xwindow-id")) {
		return GST_BUS_PASS;
	}

	window = GDK_WINDOW_XWINDOW (GTK_WIDGET (bacon)->window);
	bacon->priv->xoverlay = GST_X_OVERLAY (GST_MESSAGE_SRC (message));
	gst_x_overlay_set_xwindow_id (bacon->priv->xoverlay, window);

	return GST_BUS_DROP;
}

static void
brasero_player_bacon_clear_pipe (BraseroPlayerBacon *bacon)
{
	bacon->priv->xoverlay = NULL;
	bacon->priv->state = GST_STATE_NULL;

	if (bacon->priv->pipe) {
		GstBus *bus;

		bus = gst_pipeline_get_bus (GST_PIPELINE (bacon->priv->pipe));
		gst_bus_set_flushing (bus, TRUE);
		gst_object_unref (bus);

		gst_element_set_state (bacon->priv->pipe, GST_STATE_NULL);
		gst_object_unref (bacon->priv->pipe);
		bacon->priv->pipe = NULL;
	}

	if (bacon->priv->uri) {
		g_free (bacon->priv->uri);
		bacon->priv->uri = NULL;
	}
}

void
brasero_player_bacon_set_uri (BraseroPlayerBacon *bacon, const gchar *uri)
{
	if (bacon->priv->uri) {
		g_free (bacon->priv->uri);
		bacon->priv->uri = NULL;
	}

	if (uri) {
		bacon->priv->uri = g_strdup (uri);

		gst_element_set_state (bacon->priv->pipe, GST_STATE_NULL);
		bacon->priv->state = GST_STATE_NULL;

		g_object_set (G_OBJECT (bacon->priv->pipe),
			      "uri", uri,
			      NULL);

		gst_element_set_state (bacon->priv->pipe, GST_STATE_PAUSED);
	}
	else if (bacon->priv->pipe)
		gst_element_set_state (bacon->priv->pipe, GST_STATE_NULL);
}

gboolean
brasero_player_bacon_set_boundaries (BraseroPlayerBacon *bacon, gint64 start, gint64 end)
{
	if (!bacon->priv->pipe)
		return FALSE;

	bacon->priv->end = end;
	return gst_element_seek (bacon->priv->pipe,
				 1.0,
				 GST_FORMAT_TIME,
				 GST_SEEK_FLAG_FLUSH|
				 GST_SEEK_FLAG_ACCURATE,
				 GST_SEEK_TYPE_SET,
				 start,
				 GST_SEEK_TYPE_SET,
				 end);
}

void
brasero_player_bacon_set_volume (BraseroPlayerBacon *bacon, gdouble volume)
{
	if (!bacon->priv->pipe)
		return;

	volume = CLAMP (volume, 0, 100) / 100.0;
	g_object_set (bacon->priv->pipe,
		      "volume", volume * 100,
		      NULL);
}

gdouble
brasero_player_bacon_get_volume (BraseroPlayerBacon *bacon)
{
	gdouble volume;

	if (!bacon->priv->pipe)
		return -1.0;

	g_object_get (bacon->priv->pipe,
		      "volume", &volume,
		      NULL);

	return volume / 5.0;
}

static gboolean
brasero_player_bacon_bus_messages (GstBus *bus,
				   GstMessage *msg,
				   BraseroPlayerBacon *bacon)
{
	BraseroPlayerBaconState value;
	GstStateChangeReturn result;
	GError *error = NULL;
	GstState state;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_signal_emit (bacon,
			       brasero_player_bacon_signals [EOF_SIGNAL],
			       0);
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, NULL);
		g_warning ("%s", error->message);

		g_signal_emit (bacon,
			       brasero_player_bacon_signals [STATE_CHANGED_SIGNAL],
			       0,
			       BACON_STATE_ERROR);
		gtk_widget_queue_resize (GTK_WIDGET (bacon));
		break;

	case GST_MESSAGE_STATE_CHANGED:
		result = gst_element_get_state (GST_ELEMENT (bacon->priv->pipe),
						&state,
						NULL,
						500);

		if (result != GST_STATE_CHANGE_SUCCESS)
			break;

		if (bacon->priv->state == state || state < GST_STATE_PAUSED)
			break;

		if (state == GST_STATE_PLAYING)
			value = BACON_STATE_PLAYING;
		else
			value = BACON_STATE_PAUSED;
				
		if (bacon->priv->xoverlay)
			gtk_widget_queue_resize (GTK_WIDGET (bacon));

		bacon->priv->state = state;
		g_signal_emit (bacon,
			       brasero_player_bacon_signals [STATE_CHANGED_SIGNAL],
			       0,
			       value);
		break;

	default:
		break;
	}

	return TRUE;
}

gboolean
brasero_player_bacon_play (BraseroPlayerBacon *bacon)
{
	GstStateChangeReturn result;

	if (!bacon->priv->pipe)
		return FALSE;

	result = gst_element_set_state (bacon->priv->pipe, GST_STATE_PLAYING);
	return (result != GST_STATE_CHANGE_FAILURE);
}

gboolean
brasero_player_bacon_stop (BraseroPlayerBacon *bacon)
{
	GstStateChangeReturn result;

	if (!bacon->priv->pipe)
		return FALSE;

	result = gst_element_set_state (bacon->priv->pipe, GST_STATE_PAUSED);
	return (result != GST_STATE_CHANGE_FAILURE);
}

gboolean
brasero_player_bacon_set_pos (BraseroPlayerBacon *bacon,
			      gdouble pos)
{
	gint64 duration;
	GstFormat format = GST_FORMAT_TIME;

	if (!bacon->priv->pipe)
		return FALSE;

	gst_element_query_duration (bacon->priv->pipe,
				    &format,
				    &duration);

	return gst_element_seek (bacon->priv->pipe,
				 1.0,
				 GST_FORMAT_TIME,
				 GST_SEEK_FLAG_FLUSH,
				 GST_SEEK_TYPE_SET,
				 (gint64) pos,
				 bacon->priv->end ? GST_SEEK_TYPE_SET:GST_SEEK_TYPE_NONE,
				 bacon->priv->end);
}

gboolean
brasero_player_bacon_get_pos (BraseroPlayerBacon *bacon,
			      gint64 *pos)
{
	GstFormat format = GST_FORMAT_TIME;
	gboolean result;
	gint64 value;

	if (!bacon->priv->pipe)
		return FALSE;

	if (pos) {
		result = gst_element_query_position (bacon->priv->pipe,
						     &format,
						     &value);
		if (!result)
			return FALSE;

		*pos = value;
	}

	return TRUE;
}

static void
brasero_player_bacon_setup_pipe (BraseroPlayerBacon *bacon)
{
	GstElement *video_sink, *audio_sink;
	GstBus *bus = NULL;
	GConfClient *client;
	gdouble volume;

	bacon->priv->pipe = gst_element_factory_make ("playbin", NULL);
	if (!bacon->priv->pipe) {
		g_warning ("Pipe creation error : can't create pipe.\n");
		return;
	}

	audio_sink = gst_element_factory_make ("gconfaudiosink", NULL);
	if (audio_sink)
		g_object_set (G_OBJECT (bacon->priv->pipe),
			      "audio-sink", audio_sink,
			      NULL);
	else
		goto error;

	video_sink = gst_element_factory_make ("gconfvideosink", NULL);
	if (video_sink) {
		GstElement *element;

		g_object_set (G_OBJECT (bacon->priv->pipe),
			      "video-sink", video_sink,
			      NULL);

		element = gst_bin_get_by_interface (GST_BIN (video_sink),
						    GST_TYPE_X_OVERLAY);
		if (element && GST_IS_X_OVERLAY (element))
			bacon->priv->xoverlay = GST_X_OVERLAY (element);
	}

	bus = gst_pipeline_get_bus (GST_PIPELINE (bacon->priv->pipe));
	gst_bus_set_sync_handler (bus,
				  (GstBusSyncHandler) brasero_player_bacon_bus_messages_handler,
				  bacon);
	gst_bus_add_watch (bus,
			   (GstBusFunc) brasero_player_bacon_bus_messages,
			   bacon);
	gst_object_unref (bus);

	/* set saved volume */
	client = gconf_client_get_default ();
	volume = gconf_client_get_int (client, GCONF_PLAYER_VOLUME, NULL);
	volume = CLAMP (volume, 0, 500);
	g_object_set (bacon->priv->pipe,
		      "volume", (gdouble) volume / 100.0,
		      NULL);
	g_object_unref (client);

	return;

error:
	g_warning ("player creation error");
	brasero_player_bacon_clear_pipe (bacon);
	g_signal_emit (bacon,
		       brasero_player_bacon_signals [STATE_CHANGED_SIGNAL],
		       0,
		       BACON_STATE_ERROR);
	gtk_widget_queue_resize (GTK_WIDGET (bacon));
}

static void
brasero_player_bacon_init (BraseroPlayerBacon *obj)
{
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (obj), GTK_DOUBLE_BUFFERED);

	obj->priv = g_new0 (BraseroPlayerBaconPrivate, 1);
	brasero_player_bacon_setup_pipe (obj);
}
