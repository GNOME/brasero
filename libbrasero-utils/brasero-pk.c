/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Luis Medinas 2008 <lmedinas@gmail.com>
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#include <sys/utsname.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <dbus/dbus-glib.h>

#include <gst/pbutils/install-plugins.h>
#include <gst/pbutils/missing-plugins.h>

#include "brasero-misc.h"
#include "brasero-pk.h"

static GSList *already_tested = NULL;

typedef struct _BraseroPKPrivate BraseroPKPrivate;
struct _BraseroPKPrivate
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	DBusGProxyCall *call;

	GMainLoop *loop;
	gboolean res;
};

#define BRASERO_PK_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PK, BraseroPKPrivate))

G_DEFINE_TYPE (BraseroPK, brasero_pk, G_TYPE_OBJECT);

static void
brasero_pk_install_missing_files_result (DBusGProxy *proxy,
                                         DBusGProxyCall *call,
                                         gpointer user_data)
{
	GError *error = NULL;
	BraseroPKPrivate *priv = BRASERO_PK_PRIVATE (user_data);

	priv->call = NULL;
	priv->res = dbus_g_proxy_end_call (proxy,
	                                   call,
	                                   &error,
	                                   G_TYPE_INVALID);
	if (!priv->res) {
		BRASERO_UTILS_LOG ("%s", error->message);
		g_error_free (error);
	}

	g_main_loop_quit (priv->loop);
}

static void
brasero_pk_install_gst_plugin_result (GstInstallPluginsReturn res,
                                      gpointer user_data)
{
	BraseroPKPrivate *priv = BRASERO_PK_PRIVATE (user_data);

	switch (res) {
	case GST_INSTALL_PLUGINS_SUCCESS:
		priv->res = TRUE;
		break;

	case GST_INSTALL_PLUGINS_PARTIAL_SUCCESS:
	case GST_INSTALL_PLUGINS_USER_ABORT:

	case GST_INSTALL_PLUGINS_NOT_FOUND:
	case GST_INSTALL_PLUGINS_ERROR:
	case GST_INSTALL_PLUGINS_CRASHED:
	default:
		priv->res = FALSE;
		break;
	}

	g_main_loop_quit (priv->loop);
}

static void
brasero_pk_cancelled (GCancellable *cancel,
                      BraseroPK *package)
{
	BraseroPKPrivate *priv = BRASERO_PK_PRIVATE (package);

	priv->res = FALSE;
	g_main_loop_quit (priv->loop);
}

static gboolean
brasero_pk_wait_for_call_end (BraseroPK *package,
                              GCancellable *cancel)
{
	BraseroPKPrivate *priv;
	GMainLoop *loop;
	gulong sig_int;

	priv = BRASERO_PK_PRIVATE (package);

	loop = g_main_loop_new (NULL, FALSE);
	priv->loop = loop;

	sig_int = g_signal_connect (cancel,
	                            "cancelled",
	                            G_CALLBACK (brasero_pk_cancelled),
	                            loop);

	GDK_THREADS_LEAVE ();
	g_main_loop_run (loop);
	GDK_THREADS_ENTER ();

	g_signal_handler_disconnect (cancel, sig_int);

	g_main_loop_unref (loop);
	priv->loop = NULL;

	return priv->res;
}

static gboolean
brasero_pk_connect (BraseroPK *package)
{
	BraseroPKPrivate *priv;
	GError *error = NULL;

	priv = BRASERO_PK_PRIVATE (package);

	/* check dbus connections, exit if not valid */
	priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (priv->connection == NULL) {
		BRASERO_UTILS_LOG ("%s", error->message);
		return FALSE;
	}

	/* get a connection */
	priv->proxy = dbus_g_proxy_new_for_name (priv->connection,
	                                         "org.freedesktop.PackageKit",
	                                         "/org/freedesktop/PackageKit",
	                                         "org.freedesktop.PackageKit.Modify");
	if (priv->proxy == NULL) {
		BRASERO_UTILS_LOG ("Cannot connect to session service");
		return FALSE;
	}

	/* don't timeout, as dbus-glib sets the timeout ~25 seconds */
	dbus_g_proxy_set_default_timeout (priv->proxy, INT_MAX);

	return TRUE;
}

gboolean
brasero_pk_install_gstreamer_plugin (BraseroPK *package,
                                     const gchar *element_name,
                                     int xid,
                                    GCancellable *cancel)
{
	GstInstallPluginsContext *context;
	GPtrArray *gst_plugins = NULL;
	GstInstallPluginsReturn status;
	gboolean res = FALSE;
	gchar *detail;

	gst_plugins = g_ptr_array_new ();
	detail = gst_missing_element_installer_detail_new (element_name);
	g_ptr_array_add (gst_plugins, detail);
	g_ptr_array_add (gst_plugins, NULL);

	context = gst_install_plugins_context_new ();
	gst_install_plugins_context_set_xid (context, xid);
	status = gst_install_plugins_async ((gchar **) gst_plugins->pdata,
	                                    context,
	                                    brasero_pk_install_gst_plugin_result,
	                                    package);

	if (status == GST_INSTALL_PLUGINS_SUCCESS)
		res = brasero_pk_wait_for_call_end (package, cancel);

	gst_install_plugins_context_free (context);
	g_strfreev ((gchar **) gst_plugins->pdata);
	g_ptr_array_free (gst_plugins, FALSE);

	return res;
}

static gboolean
brasero_pk_install_file_requirement (BraseroPK *package,
                                     GPtrArray *missing_files,
                                     int xid,
                                    GCancellable *cancel)
{
	BraseroPKPrivate *priv;

	priv = BRASERO_PK_PRIVATE (package);

	if (!brasero_pk_connect (package))
		return FALSE;

	priv->call = dbus_g_proxy_begin_call_with_timeout (priv->proxy, "InstallProvideFiles",
							   brasero_pk_install_missing_files_result,
	                                                   package,
							   NULL,
							   INT_MAX,
							   G_TYPE_UINT, xid,
							   G_TYPE_STRV, missing_files->pdata,
							   G_TYPE_STRING, "hide-finished,hide-warnings",
							   G_TYPE_INVALID);

	return brasero_pk_wait_for_call_end (package, cancel);
}

gboolean
brasero_pk_install_missing_app (BraseroPK *package,
                                const gchar *file_name,
                                int xid,
                                GCancellable *cancel)
{
	gchar *path;
	gboolean res;
	GPtrArray *missing_files;

	path = g_build_path (G_DIR_SEPARATOR_S,
	                     "/usr/bin/",
	                     file_name,
	                     NULL);

	if (g_slist_find_custom (already_tested, path, (GCompareFunc) g_strcmp0)) {
		g_free (path);
		return FALSE;
	}
	already_tested = g_slist_prepend (already_tested, g_strdup (path));

	missing_files = g_ptr_array_new ();
	g_ptr_array_add (missing_files, path);
	g_ptr_array_add (missing_files, NULL);

	res = brasero_pk_install_file_requirement (package, missing_files, xid, cancel);

	g_strfreev ((gchar **) missing_files->pdata);
	g_ptr_array_free (missing_files, FALSE);

	return res;
}

/**
 * pk_gst_get_arch_suffix:
 *
 * Return value: something other than blank if we are running on 64 bit.
 **/
static gboolean
pk_gst_is_x64_arch (void)
{
	gint retval;
	struct utsname buf;

	retval = uname (&buf);

	/* did we get valid value? */
	if (retval != 0 || buf.machine == NULL) {
		g_warning ("PackageKit: cannot get machine type");
		return FALSE;
	}

	/* 64 bit machines */
	if (g_strcmp0 (buf.machine, "x86_64") == 0)
		return TRUE;

	/* 32 bit machines and unrecognized arch */
	return FALSE;
}

gboolean
brasero_pk_install_missing_library (BraseroPK *package,
                                    const gchar *library_name,
                                    int xid,
                                    GCancellable *cancel)
{
	gchar *path;
	gboolean res;
	GPtrArray *missing_files;

	if (pk_gst_is_x64_arch ())
		path = g_strdup_printf ("/usr/lib64/%s", library_name);
	else
		path = g_strdup_printf ("/usr/lib/%s", library_name);

	if (g_slist_find_custom (already_tested, path, (GCompareFunc) g_strcmp0)) {
		g_free (path);
		return FALSE;
	}
	already_tested = g_slist_prepend (already_tested, g_strdup (path));

	missing_files = g_ptr_array_new ();
	g_ptr_array_add (missing_files, path);
	g_ptr_array_add (missing_files, NULL);

	res = brasero_pk_install_file_requirement (package, missing_files, xid, cancel);

	g_strfreev ((gchar **) missing_files->pdata);
	g_ptr_array_free (missing_files, FALSE);

	return res;
}

static void
brasero_pk_init (BraseroPK *object)
{}

static void
brasero_pk_finalize (GObject *object)
{
	BraseroPKPrivate *priv;

	priv = BRASERO_PK_PRIVATE (object);

	if (priv->call)
		dbus_g_proxy_cancel_call (priv->proxy, priv->call);

	if (priv->loop)
		g_main_loop_quit (priv->loop);

	if (priv->proxy) {
		g_object_unref (priv->proxy);
		priv->proxy = NULL;
	}

	if (priv->connection) {
		dbus_g_connection_unref (priv->connection);
		priv->connection = NULL;
	}

	G_OBJECT_CLASS (brasero_pk_parent_class)->finalize (object);
}

static void
brasero_pk_class_init (BraseroPKClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroPKPrivate));

	object_class->finalize = brasero_pk_finalize;
}

BraseroPK *
brasero_pk_new (void)
{
	return g_object_new (BRASERO_TYPE_PK, NULL);
}
