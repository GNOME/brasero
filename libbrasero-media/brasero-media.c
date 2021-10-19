/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "brasero-media.h"
#include "brasero-media-private.h"

static gboolean debug = 0;

#define BRASERO_MEDIUM_TRUE_RANDOM_WRITABLE(media)				\
	(BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED) ||		\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS) ||		\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVD_RAM) || 			\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BDRE))

static const GOptionEntry options [] = {
	{ "brasero-media-debug", 0, 0, G_OPTION_ARG_NONE, &debug,
	  N_("Display debug statements on stdout for Brasero media library"),
	  NULL },
	{ NULL }
};

void
brasero_media_library_set_debug (gboolean value)
{
	debug = value;
}

static GSList *
brasero_media_add_to_list (GSList *retval,
			   BraseroMedia media)
{
	retval = g_slist_prepend (retval, GINT_TO_POINTER (media));
	return retval;
}

static GSList *
brasero_media_new_status (GSList *retval,
			  BraseroMedia media,
			  BraseroMedia type)
{
	if ((type & BRASERO_MEDIUM_BLANK)
	&& !(media & BRASERO_MEDIUM_ROM)) {
		/* If media is blank there is no other possible property. */
		retval = brasero_media_add_to_list (retval,
						    media|
						    BRASERO_MEDIUM_BLANK);

		/* NOTE about BR-R they can be "formatted" but they are never
		 * unformatted since by default they'll be used as sequential.
		 * We don't consider DVD-RW as unformatted as in this case
		 * they are treated as DVD-RW in sequential mode and therefore
		 * don't require any formatting. */
		if (!(media & BRASERO_MEDIUM_RAM)
		&&   (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS)
		||    BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BD|BRASERO_MEDIUM_REWRITABLE))) {
			if (type & BRASERO_MEDIUM_UNFORMATTED)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_BLANK|
								    BRASERO_MEDIUM_UNFORMATTED);
		}
	}

	if (type & BRASERO_MEDIUM_CLOSED) {
		if (media & (BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_BD))
			retval = brasero_media_add_to_list (retval,
							    media|
							    BRASERO_MEDIUM_CLOSED|
							    (type & BRASERO_MEDIUM_HAS_DATA)|
							    (type & BRASERO_MEDIUM_PROTECTED));
		else {
			if (type & BRASERO_MEDIUM_HAS_AUDIO)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_CLOSED|
								    BRASERO_MEDIUM_HAS_AUDIO);
			if (type & BRASERO_MEDIUM_HAS_DATA)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_CLOSED|
								    BRASERO_MEDIUM_HAS_DATA);
			if (BRASERO_MEDIUM_IS (type, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_CLOSED|
								    BRASERO_MEDIUM_HAS_DATA|
								    BRASERO_MEDIUM_HAS_AUDIO);
		}
	}

	if ((type & BRASERO_MEDIUM_APPENDABLE)
	&& !(media & BRASERO_MEDIUM_ROM)
	&& !BRASERO_MEDIUM_TRUE_RANDOM_WRITABLE (media)) {
		if (media & (BRASERO_MEDIUM_BD|BRASERO_MEDIUM_DVD))
			retval = brasero_media_add_to_list (retval,
							    media|
							    BRASERO_MEDIUM_APPENDABLE|
							    BRASERO_MEDIUM_HAS_DATA);
		else {
			if (type & BRASERO_MEDIUM_HAS_AUDIO)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_APPENDABLE|
								    BRASERO_MEDIUM_HAS_AUDIO);
			if (type & BRASERO_MEDIUM_HAS_DATA)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_APPENDABLE|
								    BRASERO_MEDIUM_HAS_DATA);
			if (BRASERO_MEDIUM_IS (type, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_HAS_DATA|
								    BRASERO_MEDIUM_APPENDABLE|
								    BRASERO_MEDIUM_HAS_AUDIO);
		}
	}

	return retval;
}

static GSList *
brasero_media_new_attribute (GSList *retval,
			     BraseroMedia media,
			     BraseroMedia type)
{
	/* NOTE: never reached by BDs, ROMs (any) or Restricted Overwrite
	 * and DVD- dual layer */

	/* NOTE: there is no dual layer DVD-RW */
	if (type & BRASERO_MEDIUM_REWRITABLE)
		retval = brasero_media_new_status (retval,
						   media|BRASERO_MEDIUM_REWRITABLE,
						   type);

	if (type & BRASERO_MEDIUM_WRITABLE)
		retval = brasero_media_new_status (retval,
						   media|BRASERO_MEDIUM_WRITABLE,
						   type);

	return retval;
}

static GSList *
brasero_media_new_subtype (GSList *retval,
			   BraseroMedia media,
			   BraseroMedia type)
{
	if (media & BRASERO_MEDIUM_BD) {
		/* There seems to be Dual layers BDs as well */

		if (type & BRASERO_MEDIUM_ROM) {
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_ROM,
							   type);
			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_status (retval,
								   media|
								   BRASERO_MEDIUM_ROM|
								   BRASERO_MEDIUM_DUAL_L,
								   type);
		}

		if (type & BRASERO_MEDIUM_RANDOM) {
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_RANDOM|
							   BRASERO_MEDIUM_WRITABLE,
							   type);
			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_status (retval,
								   media|
								   BRASERO_MEDIUM_RANDOM|
								   BRASERO_MEDIUM_WRITABLE|
								   BRASERO_MEDIUM_DUAL_L,
								   type);
		}

		if (type & BRASERO_MEDIUM_SRM) {
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_SRM|
							   BRASERO_MEDIUM_WRITABLE,
							   type);
			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_status (retval,
								   media|
								   BRASERO_MEDIUM_SRM|
								   BRASERO_MEDIUM_WRITABLE|
								   BRASERO_MEDIUM_DUAL_L,
								   type);
		}

		if (type & BRASERO_MEDIUM_POW) {
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_POW|
							   BRASERO_MEDIUM_WRITABLE,
							   type);
			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_status (retval,
								   media|
								   BRASERO_MEDIUM_POW|
								   BRASERO_MEDIUM_WRITABLE|
								   BRASERO_MEDIUM_DUAL_L,
								   type);
		}

		/* BD-RE */
		if (type & BRASERO_MEDIUM_REWRITABLE) {
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_REWRITABLE,
							   type);
			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_status (retval,
								   media|
								   BRASERO_MEDIUM_REWRITABLE|
								   BRASERO_MEDIUM_DUAL_L,
								   type);
		}
	}

	if (media & BRASERO_MEDIUM_DVD) {
		/* There is no such thing as DVD-RW DL nor DVD-RAM DL*/

		/* The following is always a DVD-R dual layer */
		if (type & BRASERO_MEDIUM_JUMP)
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_JUMP|
							   BRASERO_MEDIUM_DUAL_L|
							   BRASERO_MEDIUM_WRITABLE,
							   type);

		if (type & BRASERO_MEDIUM_SEQUENTIAL) {
			retval = brasero_media_new_attribute (retval,
							      media|
							      BRASERO_MEDIUM_SEQUENTIAL,
							      type);

			/* This one has to be writable only, no RW */
			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_status (retval,
								   media|
								   BRASERO_MEDIUM_SEQUENTIAL|
								   BRASERO_MEDIUM_WRITABLE|
								   BRASERO_MEDIUM_DUAL_L,
								   type);
		}

		/* Restricted Overwrite media are always rewritable */
		if (type & BRASERO_MEDIUM_RESTRICTED)
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_RESTRICTED|
							   BRASERO_MEDIUM_REWRITABLE,
							   type);

		if (type & BRASERO_MEDIUM_PLUS) {
			retval = brasero_media_new_attribute (retval,
							      media|
							      BRASERO_MEDIUM_PLUS,
							      type);

			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_attribute (retval,
								      media|
								      BRASERO_MEDIUM_PLUS|
								      BRASERO_MEDIUM_DUAL_L,
								      type);

		}

		if (type & BRASERO_MEDIUM_ROM) {
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_ROM,
							   type);

			if (type & BRASERO_MEDIUM_DUAL_L)
				retval = brasero_media_new_status (retval,
								   media|
								   BRASERO_MEDIUM_ROM|
								   BRASERO_MEDIUM_DUAL_L,
								   type);
		}

		/* RAM media are always rewritable */
		if (type & BRASERO_MEDIUM_RAM)
			retval = brasero_media_new_status (retval,
							   media|
							   BRASERO_MEDIUM_RAM|
							   BRASERO_MEDIUM_REWRITABLE,
							   type);
	}

	return retval;
}

GSList *
brasero_media_get_all_list (BraseroMedia type)
{
	GSList *retval = NULL;

	if (type & BRASERO_MEDIUM_FILE)
		retval = brasero_media_add_to_list (retval, BRASERO_MEDIUM_FILE);					       

	if (type & BRASERO_MEDIUM_CD) {
		if (type & BRASERO_MEDIUM_ROM)
			retval = brasero_media_new_status (retval,
							   BRASERO_MEDIUM_CD|
							   BRASERO_MEDIUM_ROM,
							   type);

		retval = brasero_media_new_attribute (retval,
						      BRASERO_MEDIUM_CD,
						      type);
	}

	if (type & BRASERO_MEDIUM_DVD)
		retval = brasero_media_new_subtype (retval,
						    BRASERO_MEDIUM_DVD,
						    type);


	if (type & BRASERO_MEDIUM_BD)
		retval = brasero_media_new_subtype (retval,
						    BRASERO_MEDIUM_BD,
						    type);

	return retval;
}

GQuark
brasero_media_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("BraseroMediaError");

	return quark;
}

void
brasero_media_to_string (BraseroMedia media,
			 gchar *buffer)
{
	if (media & BRASERO_MEDIUM_FILE)
		strcat (buffer, "file ");

	if (media & BRASERO_MEDIUM_CD)
		strcat (buffer, "CD ");

	if (media & BRASERO_MEDIUM_DVD)
		strcat (buffer, "DVD ");

	if (media & BRASERO_MEDIUM_RAM)
		strcat (buffer, "RAM ");

	if (media & BRASERO_MEDIUM_BD)
		strcat (buffer, "BD ");

	if (media & BRASERO_MEDIUM_DUAL_L)
		strcat (buffer, "DL ");

	/* DVD subtypes */
	if (media & BRASERO_MEDIUM_PLUS)
		strcat (buffer, "+ ");

	if (media & BRASERO_MEDIUM_SEQUENTIAL)
		strcat (buffer, "- (sequential) ");

	if (media & BRASERO_MEDIUM_RESTRICTED)
		strcat (buffer, "- (restricted) ");

	if (media & BRASERO_MEDIUM_JUMP)
		strcat (buffer, "- (jump) ");

	/* BD subtypes */
	if (media & BRASERO_MEDIUM_SRM)
		strcat (buffer, "SRM ");

	if (media & BRASERO_MEDIUM_POW)
		strcat (buffer, "POW ");

	if (media & BRASERO_MEDIUM_RANDOM)
		strcat (buffer, "RANDOM ");

	/* discs attributes */
	if (media & BRASERO_MEDIUM_REWRITABLE)
		strcat (buffer, "RW ");

	if (media & BRASERO_MEDIUM_WRITABLE)
		strcat (buffer, "W ");

	if (media & BRASERO_MEDIUM_ROM)
		strcat (buffer, "ROM ");

	/* status of the disc */
	if (media & BRASERO_MEDIUM_CLOSED)
		strcat (buffer, "closed ");

	if (media & BRASERO_MEDIUM_BLANK)
		strcat (buffer, "blank ");

	if (media & BRASERO_MEDIUM_APPENDABLE)
		strcat (buffer, "appendable ");

	if (media & BRASERO_MEDIUM_PROTECTED)
		strcat (buffer, "protected ");

	if (media & BRASERO_MEDIUM_HAS_DATA)
		strcat (buffer, "with data ");

	if (media & BRASERO_MEDIUM_HAS_AUDIO)
		strcat (buffer, "with audio ");

	if (media & BRASERO_MEDIUM_UNFORMATTED)
		strcat (buffer, "Unformatted ");
}

/**
 * brasero_media_get_option_group:
 *
 * Returns a GOptionGroup for the commandline arguments recognized by libbrasero-media.
 * You should add this to your GOptionContext if your are using g_option_context_parse ()
 * to parse your commandline arguments.
 *
 * Return value: a #GOptionGroup *
 **/
GOptionGroup *
brasero_media_get_option_group (void)
{
	GOptionGroup *group;

	group = g_option_group_new ("brasero-media",
				    N_("Brasero optical media library"),
				    N_("Display options for Brasero media library"),
				    NULL,
				    NULL);
	g_option_group_add_entries (group, options);
	return group;
}

void
brasero_media_message (const gchar *location,
		       const gchar *format,
		       ...)
{
	va_list arg_list;
	gchar *format_real;

	if (!debug)
		return;

	format_real = g_strdup_printf ("BraseroMedia: (at %s) %s\n",
				       location,
				       format);

	va_start (arg_list, format);
	vprintf (format_real, arg_list);
	va_end (arg_list);

	g_free (format_real);
}

#include <gtk/gtk.h>

#include "brasero-medium-monitor.h"

static BraseroMediumMonitor *default_monitor = NULL;

/**
 * brasero_media_library_start:
 *
 * Initialize the library.
 *
 * You should call this function before using any other from the library.
 *
 * Rename to: init
 **/
void
brasero_media_library_start (void)
{
	if (default_monitor) {
		g_object_ref (default_monitor);
		return;
	}

	BRASERO_MEDIA_LOG ("Initializing Brasero-media %i.%i.%i",
	                    BRASERO_MAJOR_VERSION,
	                    BRASERO_MINOR_VERSION,
	                    BRASERO_SUB);

#if defined(HAVE_STRUCT_USCSI_CMD)
	/* Work around: because on OpenSolaris brasero posiblely be run
	 * as root for a user with 'Primary Administrator' profile,
	 * a root dbus session will be autospawned at that time.
	 * This fix is to work around
	 * http://bugzilla.gnome.org/show_bug.cgi?id=526454
	 */
	g_setenv ("DBUS_SESSION_BUS_ADDRESS", "autolaunch:", TRUE);
#endif

	/* Initialize i18n */
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Initialize icon-theme */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   BRASERO_DATADIR "/icons");

	/* Take a reference for the monitoring library */
	default_monitor = brasero_medium_monitor_get_default ();
}

/**
 * brasero_media_library_stop:
 *
 * De-initialize the library once you do not need the library anymore.
 *
 * Rename to: deinit
 **/
void
brasero_media_library_stop (void)
{
	g_object_unref (default_monitor);
	default_monitor = NULL;
}
