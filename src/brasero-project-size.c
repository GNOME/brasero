/***************************************************************************
 *            brasero-project-size.c
 *
 *  jeu jui 27 11:54:52 2006
 *  Copyright  2006  Rouquier Philippe
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkcontainer.h>

#include <nautilus-burn-drive.h>

#ifdef NCB_2_15
#include <nautilus-burn-drive-monitor.h>
#endif

#include "brasero-project-size.h"
#include "brasero-ncb.h"
#include "utils.h"

static void brasero_project_size_class_init (BraseroProjectSizeClass *klass);
static void brasero_project_size_init (BraseroProjectSize *sp);
static void brasero_project_size_finalize (GObject *object);

static void brasero_project_size_realize (GtkWidget *widget);
static void brasero_project_size_unrealize (GtkWidget *widget);

static void
brasero_project_size_size_request (GtkWidget *widget,
				   GtkRequisition *requisition);
static void
brasero_project_size_size_allocate (GtkWidget *widget,
				    GtkAllocation *allocation);

static gboolean
brasero_project_size_expose (GtkWidget *widget,
			     GdkEventExpose *event);

static void
brasero_project_size_button_toggled_cb (GtkToggleButton *button,
					BraseroProjectSize *self);

static gboolean
brasero_project_size_scroll_event (GtkWidget *widget,
				   GdkEventScroll *event);

static void
brasero_project_size_add_real_medias (BraseroProjectSize *self);

static void
brasero_project_size_disc_changed_cb (GtkMenuItem *item,
				      BraseroProjectSize *self);

static void
brasero_project_size_forall_children (GtkContainer *container,
				      gboolean include_internals,
				      GtkCallback callback,
				      gpointer callback_data);

struct _BraseroDrive {
	gint64 sectors;
	NautilusBurnMediaType media;
	NautilusBurnDrive *drive;
};
typedef struct _BraseroDrive BraseroDrive;

struct _BraseroProjectSizePrivate {
	GtkWidget *menu;
	GtkTooltips *tooltips;

	GtkWidget *frame;
	GtkWidget *button;

	gint ruler_height;

	gint refresh_id;

	PangoLayout *layout;

	gint64 sectors;
	GList *drives;
	BraseroDrive *current;

	gint is_audio_context:1;
	gint was_chosen:1;
	gint is_loaded:1;
};


enum _BraseroProjectSizeSignalType {
	DISC_CHANGED_SIGNAL,
	LAST_SIGNAL
};

#define BRASERO_PROJECT_SIZE_HEIGHT	42

#define BRASERO_ARROW_NUM	4
#define ARROW_WIDTH	6
#define ARROW_HEIGHT	8

#define AUDIO_SECTOR_SIZE 2352
#define DATA_SECTOR_SIZE 2048

#define AUDIO_INTERVAL_CD	67500
#define DATA_INTERVAL_CD	51200
#define DATA_INTERVAL_DVD	262144
#define MAX_INTERVAL		9
#define MIN_INTERVAL		5

#define DRIVE_STRUCT	"drive-struct"

static guint brasero_project_size_signals [LAST_SIGNAL] = { 0 };
static GtkWidgetClass *parent_class = NULL;

GType
brasero_project_size_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroProjectSizeClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_project_size_class_init,
			NULL,
			NULL,
			sizeof (BraseroProjectSize),
			0,
			(GInstanceInitFunc)brasero_project_size_init,
		};

		type = g_type_register_static (GTK_TYPE_CONTAINER, 
					       "BraseroProjectSize",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_project_size_class_init (BraseroProjectSizeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_project_size_finalize;

	widget_class->scroll_event = brasero_project_size_scroll_event;

	widget_class->size_request = brasero_project_size_size_request;
	widget_class->size_allocate = brasero_project_size_size_allocate;
	widget_class->expose_event = brasero_project_size_expose;
	widget_class->realize = brasero_project_size_realize;
	widget_class->unrealize = brasero_project_size_unrealize;

	container_class->forall = brasero_project_size_forall_children;

	brasero_project_size_signals [DISC_CHANGED_SIGNAL] =
	    g_signal_new ("disc_changed",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE|G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroProjectSizeClass, disc_changed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
}

static void
brasero_project_size_add_default_medias (BraseroProjectSize *self)
{
	const BraseroDrive drives [] =  { {333000, NAUTILUS_BURN_MEDIA_TYPE_CDR, NULL},
					    {360000, NAUTILUS_BURN_MEDIA_TYPE_CDR, NULL},
					    {405000, NAUTILUS_BURN_MEDIA_TYPE_CDR, NULL},
					    {450000, NAUTILUS_BURN_MEDIA_TYPE_CDR, NULL},
					    {2295104, NAUTILUS_BURN_MEDIA_TYPE_DVDR, NULL},
					    { 0 } };
	const BraseroDrive *iter;

	for (iter = drives; iter->sectors; iter ++) {
		BraseroDrive *drive;

		drive = g_new0 (BraseroDrive, 1);
		drive->sectors =  iter->sectors;
		drive->media = iter->media;
		self->priv->drives = g_list_prepend (self->priv->drives, drive);
	}
}

static void
brasero_project_size_init (BraseroProjectSize *obj)
{
	GtkWidget *image;

	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (obj), GTK_NO_WINDOW);

	obj->priv = g_new0 (BraseroProjectSizePrivate, 1);
	obj->priv->layout = gtk_widget_create_pango_layout (GTK_WIDGET (obj), "");

	obj->priv->tooltips = gtk_tooltips_new ();

	brasero_project_size_add_default_medias (obj);

	obj->priv->button = gtk_toggle_button_new ();
	gtk_tooltips_set_tip (obj->priv->tooltips,
			      obj->priv->button,
			      _("Show the available media to be burnt"),
			      _("Show the available media to be burnt"));
	gtk_button_set_relief (GTK_BUTTON (obj->priv->button), GTK_RELIEF_HALF);
	gtk_container_set_border_width (GTK_CONTAINER (obj->priv->button), 0);
	g_signal_connect (obj->priv->button,
			  "toggled",
			  G_CALLBACK (brasero_project_size_button_toggled_cb),
			  obj);
	gtk_button_set_focus_on_click (GTK_BUTTON (obj->priv->button), FALSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (obj->priv->button), GTK_CAN_DEFAULT);
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (obj->priv->button), GTK_CAN_FOCUS);

	image = gtk_image_new_from_stock (GTK_STOCK_CDROM, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (obj->priv->button), image);

	gtk_widget_set_parent (obj->priv->button, GTK_WIDGET (obj));
	gtk_widget_show_all (obj->priv->button);

	obj->priv->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (obj->priv->frame), GTK_SHADOW_IN);

	gtk_widget_set_parent (obj->priv->frame, GTK_WIDGET (obj));
	gtk_widget_show_all (obj->priv->frame);
}

static void
brasero_project_size_finalize (GObject *object)
{
	BraseroProjectSize *cobj;
	GList *iter;

	cobj = BRASERO_PROJECT_SIZE (object);

	if (cobj->priv->tooltips) {
		gtk_object_sink (GTK_OBJECT (cobj->priv->tooltips));
		cobj->priv->tooltips = NULL;
	}

	if (cobj->priv->frame) {
		gtk_widget_unparent (cobj->priv->frame);
		cobj->priv->frame = NULL;
	}

	if (cobj->priv->button) {
		gtk_widget_unparent (cobj->priv->button);
		cobj->priv->button = NULL;
	}

	if (cobj->priv->refresh_id) {
		g_source_remove (cobj->priv->refresh_id);
		cobj->priv->refresh_id = 0;
	}

	if (cobj->priv->menu) {
		gtk_widget_destroy (cobj->priv->menu);
		cobj->priv->menu = NULL;
	}

	if (cobj->priv->layout) {
		g_object_unref (cobj->priv->layout);
		cobj->priv->layout = NULL;
	}

	for (iter = cobj->priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;

		drive = iter->data;
		if (drive->drive)
			nautilus_burn_drive_unref (drive->drive);

		g_free (drive);
	}
	g_list_free (cobj->priv->drives);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize(object);
}

GtkWidget *
brasero_project_size_new ()
{
	GtkWidget *obj;
	
	obj = GTK_WIDGET (g_object_new (BRASERO_TYPE_PROJECT_SIZE, NULL));

	return obj;
}

static void
brasero_project_size_forall_children (GtkContainer *container,
				      gboolean include_internals,
				      GtkCallback callback,
				      gpointer callback_data)
{
	if (include_internals) {
		BraseroProjectSize *self;

		self = BRASERO_PROJECT_SIZE (container);

		if (self->priv->frame)
			(*callback) (self->priv->frame, callback_data);
		if (self->priv->button)
			(*callback) (self->priv->button, callback_data);
	}
}

static void
brasero_project_size_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	GdkColor color;

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK|
				 GDK_BUTTON_RELEASE_MASK|
				 GDK_SCROLL_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes,
					 attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	gdk_color_parse ("DarkOliveGreen2", &color);
	gtk_widget_modify_bg (widget, GTK_STATE_INSENSITIVE, &color);
	gdk_color_parse ("LightGoldenrod2", &color);
	gtk_widget_modify_bg (widget, GTK_STATE_ACTIVE, &color);
	gdk_color_parse ("IndianRed2", &color);
	gtk_widget_modify_bg (widget, GTK_STATE_PRELIGHT, &color);
	gdk_color_parse ("White", &color);
	gtk_widget_modify_bg (widget, GTK_STATE_SELECTED, &color);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
}

static void
brasero_project_size_unrealize (GtkWidget *widget)
{
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
brasero_project_size_get_ruler_min_width (BraseroProjectSize *self,
					  gint *ruler_width,
					  gint *ruler_height)
{
	PangoRectangle extents = { 0 };
	PangoLayout *layout;

	BraseroDrive *drive;

	gint max_width, max_height, total;
	gint interval_size;
	gdouble num, i;

	drive = self->priv->current;
	if (!drive) {
		*ruler_width = 0;
		*ruler_height = 0;
		return;
	}

	if (self->priv->is_audio_context)
		interval_size = AUDIO_INTERVAL_CD;
	else if (self->priv->current->media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
		interval_size = DATA_INTERVAL_DVD;
	else
		interval_size = DATA_INTERVAL_CD;

	/* the number of interval needs to be reasonable, not over 8 not under 5 */
	total = drive->sectors > self->priv->sectors ? drive->sectors:self->priv->sectors;
	do {
		num = (gdouble) total / (gdouble) interval_size;
		if (num > MAX_INTERVAL)
			interval_size *= 10;
		else if (num < MIN_INTERVAL)
			interval_size /= 2;
	} while (num > MAX_INTERVAL || num < MIN_INTERVAL);

	max_width = 0;
	max_height = ARROW_HEIGHT;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
	for (i = 1.0; i < num; i ++) {
		gchar *markup, *string;

		string = brasero_utils_get_sectors_string (i * interval_size,
							   self->priv->is_audio_context,
							   TRUE,
							   TRUE);

		markup = g_strdup_printf ("<span size='x-small' foreground='grey10'>%s</span>", string);
		g_free (string);
		pango_layout_set_markup (layout, markup, -1);
		g_free (markup);

		pango_layout_get_pixel_extents (layout, NULL, &extents);
		max_width = MAX (max_width, extents.width);
		max_height = MAX (max_height, extents.height);
	}
	g_object_unref (layout);

	*ruler_width = (max_width + GTK_WIDGET (self)->style->xthickness * 2 + ARROW_WIDTH) * num;
	*ruler_height = max_height;
}

static gchar *
brasero_project_size_get_media_string (BraseroProjectSize *self)
{
	gchar *text = NULL;
	BraseroDrive *drive;
	gchar *drive_name = NULL;
	gchar *disc_sectors_str = NULL;
	gchar *selection_size_str = NULL;

	drive = self->priv->current;
	if (!drive)
		return NULL;

	/* we should round the disc sizes / length */
	if (drive->sectors == -2) {
		/* this is an empty drive */
		return NULL;
	}
	else if (drive->sectors == -1) {
		gchar *name;

		/* this is a mounted drive */
		name = nautilus_burn_drive_get_name_for_display (drive->drive);
		disc_sectors_str = g_strdup_printf (_("(mounted drive) <i>%s</i>"), name);
		g_free (name);
	}
	else {
		disc_sectors_str = brasero_utils_get_sectors_string (drive->sectors,
								     self->priv->is_audio_context,
								     TRUE,
								     TRUE);

		if (drive->drive)
			drive_name = nautilus_burn_drive_get_name_for_display (drive->drive);
	}

	selection_size_str = brasero_utils_get_sectors_string (self->priv->sectors,
							       self->priv->is_audio_context,
							       TRUE,
							       TRUE);

	if (self->priv->sectors > drive->sectors) {
		if (drive_name)
			text = g_strdup_printf (_("<b>Oversized</b> (%s / %s in <i>%s</i>)"),
						selection_size_str,
						disc_sectors_str,
						drive_name);
		else
			text = g_strdup_printf (_("<b>Oversized</b> (%s / %s)"),
						selection_size_str,
						disc_sectors_str);
	}
	else if (self->priv->sectors == 0) {
		if (drive_name)
			text = g_strdup_printf (_("<b>Empty</b> (%s / %s in <i>%s</i>)"),
						selection_size_str,
						disc_sectors_str,
						drive_name);
		else
			text = g_strdup_printf (_("<b>Empty</b> (%s / %s)"),
						selection_size_str,
						disc_sectors_str);
	}
	else if (drive_name)
		text = g_strdup_printf ("%s / %s (in <i>%s</i>)",
					selection_size_str,
					disc_sectors_str,
					drive_name);
	else
		text = g_strdup_printf ("%s / %s",
					selection_size_str,
					disc_sectors_str);

	g_free (selection_size_str);
	g_free (disc_sectors_str);
	g_free (drive_name);

	return text;
}

static void
brasero_project_size_size_request (GtkWidget *widget,
				   GtkRequisition *requisition)
{
	gint width, height, text_width, ruler_width, ruler_height;
	PangoRectangle extents = { 0 };
	BraseroProjectSize *self;
	GtkRequisition req;

	self = BRASERO_PROJECT_SIZE (widget);

	if (self->priv->layout) {
		gchar *text;

		text = brasero_project_size_get_media_string (self);
		pango_layout_set_markup (self->priv->layout, text, -1);
		g_free (text);

		pango_layout_get_pixel_extents (self->priv->layout,
						NULL,
						&extents);
		text_width = extents.width;
	}
	else
		text_width = 0;

	brasero_project_size_get_ruler_min_width (self, &ruler_width, &ruler_height);
	gtk_widget_size_request (self->priv->button, &req);

	width = MAX (ruler_width, text_width) +
		self->priv->frame->style->xthickness * 2 +
		req.width;

	height = extents.height + self->priv->frame->style->ythickness * 2;
	height = MAX (height, req.height);
	height += ruler_height;
	
	requisition->height = height > BRASERO_PROJECT_SIZE_HEIGHT ? height:BRASERO_PROJECT_SIZE_HEIGHT;
	requisition->width = width;

	self->priv->ruler_height = ruler_height;
}

static void
brasero_project_size_size_allocate (GtkWidget *widget,
				    GtkAllocation *allocation)
{
	BraseroProjectSize *self;
	GtkAllocation alloc;
	GtkRequisition req;

	gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_NO_WINDOW (widget)) {
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
	}

	/* allocate the size for the button */
	self = BRASERO_PROJECT_SIZE (widget);
	gtk_widget_size_request (self->priv->button, &req);

	/* NOTE: since we've got our own window, we don't need to take into
	 * account alloc.x and alloc.y */
	if (is_rtl)
		alloc.x = 0;
	else
		alloc.x = allocation->width - req.width;
	alloc.y = 0;
	alloc.width = MAX (1, req.width);
	alloc.height = MAX (req.height, allocation->height - self->priv->ruler_height);
	gtk_widget_size_allocate (self->priv->button, &alloc);
}

static gboolean
brasero_project_size_expose (GtkWidget *widget, GdkEventExpose *event)
{
	PangoRectangle extents = { 0 };
	PangoLayout *layout;

	gint interval_size, interval_width;
	gint x, y, width, total;
	gdouble num, i;

	BraseroProjectSize *self;
	gdouble fraction = 0.0;
	gint text_height = 0;
	BraseroDrive *drive;

	gint bar_width, bar_height;
	gchar *markup;

	GtkAllocation alloc;

	gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

	if (!GTK_WIDGET_DRAWABLE (widget))
		return TRUE;

	self = BRASERO_PROJECT_SIZE (widget);

	/* paint the button */
	gtk_container_propagate_expose (GTK_CONTAINER (widget),
					self->priv->button,
					event);

	drive = self->priv->current;
	if (!drive)
		return FALSE;

	/* the number of interval needs to be reasonable, not over 8 not under 5 */
	if (self->priv->is_audio_context)
		interval_size = AUDIO_INTERVAL_CD;
	else if (self->priv->current->media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
		interval_size = DATA_INTERVAL_DVD;
	else
		interval_size = DATA_INTERVAL_CD;

	total = self->priv->current->sectors > self->priv->sectors ? self->priv->current->sectors:self->priv->sectors;
	do {
		num = (gdouble) total / (gdouble) interval_size;
		if (num > MAX_INTERVAL)
			interval_size *= 10;
		else if (num < MIN_INTERVAL)
			interval_size /= 2;
	} while (num > MAX_INTERVAL || num < MIN_INTERVAL);

	/* calculate the size of the interval in pixels */
	bar_width = widget->allocation.width - self->priv->button->allocation.width - self->priv->frame->style->xthickness * 2;
	interval_width = bar_width / num;

	/* draw the ruler */
	layout = gtk_widget_create_pango_layout (widget, NULL);
	for (i = 1.0; i < num; i ++) {
		gchar *string;

		string = brasero_utils_get_sectors_string (i * interval_size,
							   self->priv->is_audio_context,
							   TRUE,
							   TRUE);

		markup = g_strdup_printf ("<span size='x-small' foreground='grey10'>%s</span>", string);
		g_free (string);
		pango_layout_set_markup (layout, markup, -1);
		g_free (markup);

		pango_layout_get_pixel_extents (layout, NULL, &extents);

		text_height = MAX (text_height, extents.height);
		y = widget->allocation.height - extents.height;

		if (is_rtl)
			x = widget->allocation.width - self->priv->frame->style->xthickness - i * interval_width + widget->style->xthickness + ARROW_WIDTH / 2;
		else
			x = i * interval_width + self->priv->frame->style->xthickness - widget->style->xthickness - extents.width - ARROW_WIDTH / 2;

		gtk_paint_layout (widget->style,
				  widget->window,
				  GTK_STATE_NORMAL,
				  TRUE,
				  &event->area,
				  widget,
				  NULL,
				  x,
				  y,
				  layout);
	}
	g_object_unref (layout);

	for (i = 1.0; i < num; i ++) {
		if (is_rtl)
			x = widget->allocation.width - self->priv->frame->style->xthickness - i * interval_width - ARROW_WIDTH / 2;
		else
			x = i * interval_width + self->priv->frame->style->xthickness - ARROW_WIDTH / 2;

  		gtk_paint_arrow (widget->style,
				 widget->window,
				 GTK_STATE_NORMAL,
				 GTK_SHADOW_ETCHED_IN,
				 &event->area,
				 widget,
				 NULL,
				 GTK_ARROW_UP,
				 FALSE,
				 x,
				 widget->allocation.height - text_height,
				 ARROW_WIDTH,
				 ARROW_HEIGHT);
	}

	bar_height = widget->allocation.height - text_height;

	fraction = ((gdouble) self->priv->sectors / (gdouble) drive->sectors);
	if (fraction > 1.0)
		width = bar_width / fraction * 1.0;
	else
		width = fraction * bar_width;

	if (is_rtl)
		x = widget->allocation.width - width - self->priv->frame->style->xthickness;
	else
		x = self->priv->frame->style->xthickness;

	gtk_paint_flat_box (widget->style,
			    widget->window,
			    GTK_STATE_INSENSITIVE,
			    GTK_SHADOW_NONE,
			    &event->area,
			    NULL,
			    NULL,
			    x,
			    self->priv->frame->style->ythickness,
			    width,
			    bar_height - self->priv->frame->style->ythickness);

	if (fraction > 1.0) {
		gint width2;

		if (fraction > 1.03)
			width2 = bar_width / fraction * 0.03;
		else
			width2 = bar_width / fraction * (fraction - 1.0);

		if (is_rtl)
			x = widget->allocation.width - width - width2 - self->priv->frame->style->xthickness;
		else
			x = width + self->priv->frame->style->xthickness;

		gtk_paint_flat_box (widget->style,
				    widget->window,
				    GTK_STATE_ACTIVE,
				    GTK_SHADOW_NONE,
				    &event->area,
				    NULL,
				    NULL,
				    x,
				    self->priv->frame->style->ythickness,
				    width2,
				    bar_height - self->priv->frame->style->ythickness);

		if (fraction > 1.03) {
			if (is_rtl)
				x = widget->allocation.width - bar_width - self->priv->frame->style->xthickness;
			else
				x = width + width2 + self->priv->frame->style->xthickness;

			gtk_paint_flat_box (widget->style,
					    widget->window,
					    GTK_STATE_PRELIGHT,
					    GTK_SHADOW_NONE,
					    &event->area,
					    NULL,
					    NULL,
					    x,
					    self->priv->frame->style->ythickness,
					    bar_width - width - width2,
					    bar_height - self->priv->frame->style->ythickness);
		}
	}
	else {
		if (is_rtl)
			x = width ? widget->allocation.width - bar_width + width - self->priv->frame->style->xthickness:widget->allocation.width - bar_width - self->priv->frame->style->xthickness;
		else
			x = width ? width:self->priv->frame->style->xthickness;

		gtk_paint_flat_box (widget->style,
				    widget->window,
				    GTK_STATE_SELECTED,
				    GTK_SHADOW_NONE,
				    &event->area,
				    NULL,
				    NULL,
				    x,
				    self->priv->frame->style->ythickness,
				    bar_width - width + self->priv->frame->style->xthickness,
				    bar_height - self->priv->frame->style->ythickness);
	}

	alloc.x = 0;
	alloc.y = 0;
	alloc.width = bar_width + self->priv->frame->style->xthickness * 2;
	alloc.height = bar_height;
	gtk_widget_size_allocate (self->priv->frame, &alloc);
	gtk_container_propagate_expose (GTK_CONTAINER (widget),
					self->priv->frame,
					event);

	/* set the text */
	markup = brasero_project_size_get_media_string (self);
	pango_layout_set_markup (self->priv->layout, markup, -1);
	g_free (markup);
	pango_layout_get_pixel_extents (self->priv->layout, NULL, &extents);

	x = (widget->allocation.width - extents.width) / 2;
	y = (widget->allocation.height - extents.height - text_height) / 2;

	gtk_paint_layout (widget->style,
			  widget->window,
			  GTK_STATE_NORMAL,
			  TRUE,
			  &event->area,
			  widget,
			  NULL,
			  x,
			  y,
			  self->priv->layout);

 	return FALSE;
}

static void
brasero_project_size_disc_changed (BraseroProjectSize *self)
{
	gtk_widget_queue_resize (GTK_WIDGET (self));
	g_signal_emit (self,
		       brasero_project_size_signals [DISC_CHANGED_SIGNAL],
		       0);
}

static void
brasero_project_size_disc_changed_cb (GtkMenuItem *item,
				      BraseroProjectSize *self)
{
	BraseroDrive *drive;

	drive = g_object_get_data (G_OBJECT (item), DRIVE_STRUCT);
	self->priv->current = drive;
	brasero_project_size_disc_changed (self);
}

static void
brasero_project_size_menu_position_cb (GtkMenu *menu,
				       gint *x,
				       gint *y,
				       gboolean *push_in,
				       gpointer user_data)
{
	gint sx, sy;
	gint width, height;
	GtkRequisition req;
	GdkScreen *screen;
	gint monitor_num;
	GdkRectangle monitor;
	GtkWidget *self = user_data;
 
	/* All this comes from GTK+ gtkcombobox.c */
	gdk_window_get_origin (self->window, &sx, &sy);
	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	gdk_drawable_get_size (GDK_DRAWABLE (self->window), &width, &height);
	gtk_widget_set_size_request (GTK_WIDGET (menu), width, -1);

	if (gtk_widget_get_direction (self) == GTK_TEXT_DIR_LTR)
		*x = sx;
	else
		*x = sx - req.width;
	*y = sy;

	screen = gtk_widget_get_screen (self);
	monitor_num = gdk_screen_get_monitor_at_window (screen, self->window);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
  
	if (*x < monitor.x)
		*x = monitor.x;
	else if (*x + req.width > monitor.x + monitor.width)
		*x = monitor.x + monitor.width - req.width;
  
	if (monitor.height - (*y - monitor.y) >= req.height)
		*y += height - BRASERO_PROJECT_SIZE (self)->priv->ruler_height;
	else
		*y -= req.height;

	*push_in = FALSE;
}

static GtkWidget *
brasero_project_size_build_menu (BraseroProjectSize *self)
{
	gboolean separator;
	GtkWidget *menu;
	GtkWidget *item;
	GList *iter;

	menu = gtk_menu_new ();

	separator = TRUE;
	for (iter = self->priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;
		GtkWidget *image;
		gchar *size_str;
		gchar *label;

		drive = iter->data;

		if (drive->media <= NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN)
			continue;

		if (self->priv->is_audio_context
		&&  drive->media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
			continue;

		if (!drive->drive && !separator) {
			item = gtk_separator_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			separator = TRUE;
		}
		else if (drive->drive)
			separator = FALSE;

		if (self->priv->is_audio_context)
			size_str = brasero_utils_get_time_string_from_size (drive->sectors * DATA_SECTOR_SIZE, TRUE, TRUE);
		else
			size_str = brasero_utils_get_size_string (drive->sectors * DATA_SECTOR_SIZE, TRUE, TRUE); 

		if (drive->drive)
			label = g_strdup_printf (_("%s (%s) inserted in %s"),
						 size_str,
						 nautilus_burn_drive_media_type_get_string (drive->media),
						 nautilus_burn_drive_get_name_for_display (drive->drive));
		else
			label = g_strdup_printf (_("%s (%s)"),
						 size_str,
						 nautilus_burn_drive_media_type_get_string (drive->media));
		g_free (size_str);

		item = gtk_image_menu_item_new_with_label (label);
		g_free (label);

		if (!drive->drive)
			image = gtk_image_new_from_stock (BRASERO_STOCK_DRIVE, GTK_ICON_SIZE_MENU);
		else if (drive->media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
			image = gtk_image_new_from_stock (BRASERO_STOCK_DVDR, GTK_ICON_SIZE_MENU);
		else
			image = gtk_image_new_from_stock (BRASERO_STOCK_CDR, GTK_ICON_SIZE_MENU);

		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

		g_signal_connect (item,
				  "activate",
				  G_CALLBACK (brasero_project_size_disc_changed_cb),
				  self);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_object_set_data (G_OBJECT (item), DRIVE_STRUCT, drive);
	}

	gtk_widget_show_all (menu);

	return menu;
}

static void
brasero_project_size_menu_finished_cb (GtkMenuShell *shell,
				       BraseroProjectSize *self)
{
	gtk_widget_destroy (self->priv->menu);
	self->priv->menu = NULL;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->button), FALSE);
}

static void
brasero_project_size_show_menu_real (BraseroProjectSize *self,
				     GdkEventButton *event)
{
	GtkWidget *menu;

	menu = brasero_project_size_build_menu (self);
	if (!menu)
		return;

	if (self->priv->menu)
		gtk_widget_destroy (self->priv->menu);

	self->priv->menu = menu;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->button), TRUE);

	gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			brasero_project_size_menu_position_cb,
			GTK_WIDGET (self),
			event ? event->button:1,
			event ? event->time:gtk_get_current_event_time ());

	g_signal_connect (menu,
			  "selection-done",
			  G_CALLBACK (brasero_project_size_menu_finished_cb),
			  self);
}

static void
brasero_project_size_button_toggled_cb (GtkToggleButton *button,
					BraseroProjectSize *self)
{
	if (gtk_toggle_button_get_active (button)) {
		if (self->priv->menu)
			return;
	}
	else if (!self->priv->menu)
		return;

	brasero_project_size_show_menu_real (self, NULL);
}

static gboolean
brasero_project_size_scroll_event (GtkWidget *widget,
				   GdkEventScroll *event)
{
	BraseroProjectSize *self;

	self = BRASERO_PROJECT_SIZE (widget);
	if (event->direction == GDK_SCROLL_DOWN) {
		GList *node, *iter;

		node = g_list_find (self->priv->drives, self->priv->current);
		iter = g_list_next (node);
		if (!iter)
			return TRUE;

		while (iter != node) {
			BraseroDrive *drive;

			drive = iter->data;
		
			if ((!self->priv->is_audio_context
			||    drive->media <= NAUTILUS_BURN_MEDIA_TYPE_CDRW)
			&&  drive->media > NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN) {
				self->priv->current = drive;
				break;
			}

			iter = g_list_next (iter);
			if (!iter)
				return TRUE;
		}
		brasero_project_size_disc_changed (self);
	}
	else if (event->direction == GDK_SCROLL_UP) {
		GList *node, *iter;

		node = g_list_find (self->priv->drives, self->priv->current);
		iter = g_list_previous (node);
		if (!iter)
			return TRUE;

		while (iter != node) {
			BraseroDrive *drive;

			drive = iter->data;
		
			if ((!self->priv->is_audio_context
			||    drive->media <= NAUTILUS_BURN_MEDIA_TYPE_CDRW)
			&&  drive->media > NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN) {
				self->priv->current = drive;
				break;
			}

			iter = g_list_previous (iter);
			if (!iter)
				return TRUE;
		}
		brasero_project_size_disc_changed (self);
	}

	return FALSE;
}

static gboolean
brasero_project_size_update_sectors (BraseroProjectSize *self)
{
	gtk_widget_queue_draw (GTK_WIDGET (self));
	self->priv->refresh_id = 0;
	return FALSE;
}

void
brasero_project_size_set_sectors (BraseroProjectSize *self,
				  gint64 sectors)
{
	/* we don't update the size right now but in half a second.
	 * when exploring directories size can changed repeatedly
	 * and we don't want to lose too much time updating.
	 * if a size is already set, we know that we're waiting for
	 * a size update, so, just replace the old size. otherwise
	 * we add a g_timeout_add */

	/* we add 175 sectors for a single session track (that's the needed
	 * overhead.
	 * for multisessions (we'll need ncb 2.15), the overhead is much
	 * larger since we'll have to add 2 sec gap between tracks (300 sectors)
	 * - first track : 6750 sectors (1.5 min) and leadout 4500 sectors (1 mn)
	 *   and 2 sec gap (150 sectors)
	 * - next tracks : leadin 6750 sectors, leadout 2250 sectors (0.5 mn)
	 * Now, for the moment we don't know exactly how much we need ...
	 * so we add the maximum number of sectors and if the user wants he can
	 * still use overburn
	 */
	/* FIXME: for now just add 500 sectors = 1Mib */
	if (sectors)
		self->priv->sectors = sectors + 500;
	else
		self->priv->sectors = 0;
	
	if (!self->priv->refresh_id)
		self->priv->refresh_id = g_timeout_add (500,
						       (GSourceFunc) brasero_project_size_update_sectors,
						       self);
}

static void
brasero_project_size_find_proper_drive (BraseroProjectSize *self)
{
	GList *iter;
	BraseroDrive *candidate = NULL;

	/* The rule:
	 * we don't change the current drive if it is real unless another
	 * real drive comes up with a size fitting the size of the selection. */

	if (self->priv->current) {
		BraseroDrive *current;

		/* we check the current drive to see if it is suitable */
		current = self->priv->current;

		if (self->priv->is_audio_context
		&&  current->media > NAUTILUS_BURN_MEDIA_TYPE_CDRW) {
			current = NULL;
		}
		else if (current->media < NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN) {
			current = NULL;
		}
		else if (current->sectors >= self->priv->sectors && current->drive)
			return;
		else /* see if there is better */
			candidate = self->priv->current;
	}
		 
	/* Try to find the first best candidate */
	for (iter = self->priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;

		drive = iter->data;

		/* No DVD if context is audio */
		if (self->priv->is_audio_context
		&&  drive->media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
			continue;

		if (drive->media < NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN)
			continue;

		/* we must have at least one candidate */
		if (!candidate)
			candidate = drive;

		/* Try to find a drive large enough */
		if (drive->sectors < self->priv->sectors)
			continue;

		if (candidate->sectors < self->priv->sectors) {
			if (!candidate->drive) {
				candidate = drive;
				if (drive->drive)
					break;
			}
			else if (drive->drive) {
				candidate = drive;
				break;
			}
		}
		else if (drive->drive) {
			candidate = drive;
			break;
		}
	}
	self->priv->current = candidate;
}

void
brasero_project_size_set_context (BraseroProjectSize *self,
				  gboolean is_audio)
{
	self->priv->sectors = 0;
	self->priv->is_audio_context = is_audio;

	if (!self->priv->is_loaded) {
		brasero_project_size_add_real_medias (self);
		self->priv->is_loaded = 1;
	}

	if (!self->priv->current)
		brasero_project_size_find_proper_drive (self);
	else if (is_audio
	      &&  self->priv->current->media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
		brasero_project_size_find_proper_drive (self);
	else if (!self->priv->current->drive)
		brasero_project_size_find_proper_drive (self);

	brasero_project_size_disc_changed (self);
}

gboolean
brasero_project_size_check_status (BraseroProjectSize *self,
				   gboolean *overburn)
{
	gint64 max_sectors;
	gint64 disc_sectors;

	if (!self->priv->current)
		return FALSE;
	
	disc_sectors = self->priv->current->sectors;
	if (disc_sectors < 0)
		disc_sectors = 0;

	/* FIXME: This is not good since with a DVD 3% of 4.3G may be too much
	 * with 3% we are slightly over the limit of the most overburnable discs
	 * but at least users can try to overburn as much as they can. */

	/* The idea would be to test write the disc with cdrecord from /dev/null
	 * until there is an error and see how much we were able to write. So,
	 * when we propose overburning to the user, we could ask if he wants
	 * us to determine how much data can be written to a particular disc
	 * provided he has chosen a real disc. */
	max_sectors = disc_sectors * 103 / 100;

	if (disc_sectors <= 0) {
		if (overburn)
			*overburn = FALSE;

		return TRUE;
	}

	if (max_sectors < self->priv->sectors) {
		if (overburn)
			*overburn = FALSE;

		return FALSE;
	}

	if (disc_sectors < self->priv->sectors) {
		if (overburn)
			*overburn = TRUE;

		return FALSE;
	}

	return TRUE;
}

/********************************* real drives *********************************/
static void
brasero_project_size_disc_added_cb (NautilusBurnDrive *ndrive,
				    BraseroProjectSize *self)
{
	GList *iter;

	for (iter = self->priv->drives; iter; iter = iter->next) {
		BraseroDrive *bdrive;

		bdrive = iter->data;
		if (bdrive->drive
		&&  nautilus_burn_drive_equal (ndrive, bdrive->drive)) {
			gint64 size;

			bdrive->media = nautilus_burn_drive_get_media_type (ndrive);

			size = NCB_MEDIA_GET_CAPACITY (ndrive);
			size = size > 0 ? size : 0;
			bdrive->sectors = size % 2048 ? size / 2048 + 1 : size / 2048;	

			brasero_project_size_find_proper_drive (self);
			brasero_project_size_disc_changed (self);
		}
	}

	/* we need to rebuild the menu is any */
	if (self->priv->menu)
		brasero_project_size_show_menu_real (self, NULL);
}

static void
brasero_project_size_disc_removed_cb (NautilusBurnDrive *ndrive,
				      BraseroProjectSize *self)
{
	GList *iter;

	for (iter = self->priv->drives; iter; iter = iter->next) {
		BraseroDrive *bdrive;

		bdrive = iter->data;
		if (bdrive->drive
		&&  nautilus_burn_drive_equal (ndrive, bdrive->drive)) {
			bdrive->media = NAUTILUS_BURN_MEDIA_TYPE_ERROR;
			bdrive->sectors = 0;

			brasero_project_size_find_proper_drive (self);
			brasero_project_size_disc_changed (self);
		}
	}

	/* we need to rebuild the menu is any */
	if (self->priv->menu)
		brasero_project_size_show_menu_real (self, NULL);
}

static void
brasero_project_size_add_real_medias (BraseroProjectSize *self)
{
	GList *iter, *list;

	NCB_DRIVE_GET_LIST (list, TRUE, FALSE);
	for (iter = list; iter; iter = iter->next) {
		BraseroDrive *drive;
		gint64 size;

		drive = g_new0 (BraseroDrive, 1);
		drive->drive = iter->data;
		self->priv->drives = g_list_prepend (self->priv->drives, drive);

		/* add a callback if media changes */
		g_object_set (drive->drive, "enable-monitor", TRUE, NULL);
		g_signal_connect (drive->drive,
				  "media-added",
				  G_CALLBACK (brasero_project_size_disc_added_cb),
				  self);
		g_signal_connect (drive->drive,
				  "media-removed",
				  G_CALLBACK (brasero_project_size_disc_removed_cb),
				  self);

		/* get all the information about the current media */
		drive->media = nautilus_burn_drive_get_media_type (drive->drive);
		if (drive->media <= NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN) {
			drive->sectors = 0;
			continue;
		}

		size = NCB_MEDIA_GET_CAPACITY (drive->drive);
		size = size > 0 ? size : 0;
		drive->sectors = size % 2048 ? size / 2048 + 1 : size / 2048;

		/* FIXME: when we add an appendable disc maybe we could display 
		 * its label. Moreover we should get the name of the already 
		 * existing label in burn-dialog.c */
	}
	g_list_free (list);

	brasero_project_size_find_proper_drive (self);
	brasero_project_size_disc_changed (self);
}
