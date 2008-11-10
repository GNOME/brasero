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
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */
 
/***************************************************************************
 *            burn-basics.c
 *
 *  Sat Feb 11 16:55:54 2006
 *  Copyright  2006  philippe
 *  <philippe@Rouquier Philippe.localdomain>
 ****************************************************************************/

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-caps.h"
#include "burn-hal-watch.h"
#include "burn-plugin-manager.h"
#include "burn-medium-monitor.h"
#include "burn-plugin-private.h"
#include "burn-drive.h"

static BraseroPluginManager *plugin_manager = NULL;
static BraseroMediumMonitor *medium_manager = NULL;

/**
 * This is defined in burn-caps.c it's for debugging mainly
 */

void brasero_caps_list_dump (void);

GQuark
brasero_burn_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("BraseroBurnError");

	return quark;
}
 
const gchar *
brasero_burn_action_to_string (BraseroBurnAction action)
{
	gchar *strings [BRASERO_BURN_ACTION_LAST] = { 	"",
							N_("Getting size"),
							N_("Creating image"),
							N_("Writing"),
							N_("Blanking"),
							N_("Creating checksum"),
							N_("Copying disc"),
							N_("Copying file"),
							N_("Analysing audio information"),
							N_("Transcoding song"),
							N_("Preparing to write"),
							N_("Writing leadin"),
							N_("Writing CD-TEXT information"),
							N_("Finalising"),
							N_("Writing leadout"),
						        N_("Starting to record"),
							N_("Success") };
	return _(strings [action]);
}

BraseroBurnResult
brasero_burn_library_init (void)
{
	BRASERO_BURN_LOG ("Initializing Brasero-%i.%i.%i",
			  BRASERO_MAJOR_VERSION,
			  BRASERO_MINOR_VERSION,
			  BRASERO_SUB);

	/* initialize all device list */
	if (!medium_manager)
		medium_manager = brasero_medium_monitor_get_default ();

	/* initialize plugins */
	brasero_burn_caps_get_default ();

	if (!plugin_manager)
		plugin_manager = brasero_plugin_manager_get_default ();

	brasero_caps_list_dump ();
	return BRASERO_BURN_OK;
}

GSList *
brasero_burn_library_get_plugins_list (void)
{
	plugin_manager = brasero_plugin_manager_get_default ();
	return brasero_plugin_manager_get_plugins_list (plugin_manager);
}

void
brasero_burn_library_shutdown (void)
{
	if (plugin_manager) {
		g_object_unref (plugin_manager);
		plugin_manager = NULL;
	}

	if (medium_manager) {
		g_object_unref (medium_manager);
		medium_manager = NULL;
	}

	/* close HAL connection */
	brasero_hal_watch_destroy ();
}
