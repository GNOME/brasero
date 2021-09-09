/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#ifndef _BURN_PLUGIN_H_REGISTRATION_
#define _BURN_PLUGIN_H_REGISTRATION_

#include <glib.h>

#include "brasero-medium.h"

#include "brasero-enums.h"
#include "brasero-track.h"
#include "brasero-track-stream.h"
#include "brasero-track-data.h"
#include "brasero-plugin.h"

G_BEGIN_DECLS

#define BRASERO_PLUGIN_BURN_FLAG_MASK	(BRASERO_BURN_FLAG_DUMMY|		\
					 BRASERO_BURN_FLAG_MULTI|		\
					 BRASERO_BURN_FLAG_DAO|			\
					 BRASERO_BURN_FLAG_RAW|			\
					 BRASERO_BURN_FLAG_BURNPROOF|		\
					 BRASERO_BURN_FLAG_OVERBURN|		\
					 BRASERO_BURN_FLAG_NOGRACE|		\
					 BRASERO_BURN_FLAG_APPEND|		\
					 BRASERO_BURN_FLAG_MERGE)

#define BRASERO_PLUGIN_IMAGE_FLAG_MASK	(BRASERO_BURN_FLAG_APPEND|		\
					 BRASERO_BURN_FLAG_MERGE)

#define BRASERO_PLUGIN_BLANK_FLAG_MASK	(BRASERO_BURN_FLAG_NOGRACE|		\
					 BRASERO_BURN_FLAG_FAST_BLANK)

/**
 * These are the functions a plugin must implement
 */

GType brasero_plugin_register_caps (BraseroPlugin *plugin, gchar **error);

void
brasero_plugin_define (BraseroPlugin *plugin,
		       const gchar *name,
                       const gchar *display_name,
		       const gchar *description,
		       const gchar *author,
		       guint priority);
void
brasero_plugin_set_compulsory (BraseroPlugin *self,
			       gboolean compulsory);

void
brasero_plugin_register_group (BraseroPlugin *plugin,
			       const gchar *name);

typedef enum {
	BRASERO_PLUGIN_IO_NONE			= 0,
	BRASERO_PLUGIN_IO_ACCEPT_PIPE		= 1,
	BRASERO_PLUGIN_IO_ACCEPT_FILE		= 1 << 1,
} BraseroPluginIOFlag;

GSList *
brasero_caps_image_new (BraseroPluginIOFlag flags,
			BraseroImageFormat format);

GSList *
brasero_caps_audio_new (BraseroPluginIOFlag flags,
			BraseroStreamFormat format);

GSList *
brasero_caps_data_new (BraseroImageFS fs_type);

GSList *
brasero_caps_disc_new (BraseroMedia media);

GSList *
brasero_caps_checksum_new (BraseroChecksumType checksum);

void
brasero_plugin_link_caps (BraseroPlugin *plugin,
			  GSList *outputs,
			  GSList *inputs);

void
brasero_plugin_blank_caps (BraseroPlugin *plugin,
			   GSList *caps);

/**
 * This function is important since not only does it set the flags but it also 
 * tells brasero which types of media are supported. So even if a plugin doesn't
 * support any flag, use it to tell brasero which media are supported.
 * That's only needed if the plugin supports burn/blank operations.
 */
void
brasero_plugin_set_flags (BraseroPlugin *plugin,
			  BraseroMedia media,
			  BraseroBurnFlag supported,
			  BraseroBurnFlag compulsory);
void
brasero_plugin_set_blank_flags (BraseroPlugin *self,
				BraseroMedia media,
				BraseroBurnFlag supported,
				BraseroBurnFlag compulsory);

void
brasero_plugin_process_caps (BraseroPlugin *plugin,
			     GSList *caps);

void
brasero_plugin_set_process_flags (BraseroPlugin *plugin,
				  BraseroPluginProcessFlag flags);

void
brasero_plugin_check_caps (BraseroPlugin *plugin,
			   BraseroChecksumType type,
			   GSList *caps);

/**
 * Plugin configure options
 */

BraseroPluginConfOption *
brasero_plugin_conf_option_new (const gchar *key,
				const gchar *description,
				BraseroPluginConfOptionType type);

BraseroBurnResult
brasero_plugin_add_conf_option (BraseroPlugin *plugin,
				BraseroPluginConfOption *option);

BraseroBurnResult
brasero_plugin_conf_option_bool_add_suboption (BraseroPluginConfOption *option,
					       BraseroPluginConfOption *suboption);

BraseroBurnResult
brasero_plugin_conf_option_int_set_range (BraseroPluginConfOption *option,
					  gint min,
					  gint max);

BraseroBurnResult
brasero_plugin_conf_option_choice_add (BraseroPluginConfOption *option,
				       const gchar *string,
				       gint value);

void
brasero_plugin_add_error (BraseroPlugin *plugin,
                          BraseroPluginErrorType type,
                          const gchar *detail);

void
brasero_plugin_test_gstreamer_plugin (BraseroPlugin *plugin,
                                      const gchar *name);

void
brasero_plugin_test_app (BraseroPlugin *plugin,
                         const gchar *name,
                         const gchar *version_arg,
                         const gchar *version_format,
                         gint version [3]);

/**
 * Boiler plate for plugin definition to save the hassle of definition.
 * To be put at the beginning of the .c file.
 */
typedef GType	(* BraseroPluginRegisterType)	(BraseroPlugin *plugin);

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin);

#define BRASERO_PLUGIN_BOILERPLATE(PluginName, plugin_name, PARENT_NAME, ParentName) \
typedef struct {								\
	ParentName parent;							\
} PluginName;									\
										\
typedef struct {								\
	ParentName##Class parent_class;						\
} PluginName##Class;								\
										\
static GType plugin_name##_type = 0;						\
										\
static GType									\
plugin_name##_get_type (void)							\
{										\
	return plugin_name##_type;						\
}										\
										\
static void plugin_name##_class_init (PluginName##Class *klass);		\
static void plugin_name##_init (PluginName *sp);				\
static void plugin_name##_finalize (GObject *object);			\
static void plugin_name##_export_caps (BraseroPlugin *plugin);	\
G_MODULE_EXPORT GType								\
brasero_plugin_register (BraseroPlugin *plugin);				\
G_MODULE_EXPORT GType								\
brasero_plugin_register (BraseroPlugin *plugin)				\
{														\
	if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE)	\
		plugin_name##_export_caps (plugin);					\
	static const GTypeInfo our_info = {					\
		sizeof (PluginName##Class),					\
		NULL,										\
		NULL,										\
		(GClassInitFunc)plugin_name##_class_init,			\
		NULL,										\
		NULL,										\
		sizeof (PluginName),							\
		0,											\
		(GInstanceInitFunc)plugin_name##_init,			\
	};												\
	plugin_name##_type = g_type_module_register_type (G_TYPE_MODULE (plugin),		\
							  PARENT_NAME,			\
							  G_STRINGIFY (PluginName),		\
							  &our_info,				\
							  0);						\
	return plugin_name##_type;						\
}

#define BRASERO_PLUGIN_ADD_STANDARD_CDR_FLAGS(plugin_MACRO, unsupported_MACRO)	\
	/* Use DAO for first session since AUDIO need it to write CD-TEXT */	\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_CD|				\
				  BRASERO_MEDIUM_WRITABLE|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_DAO|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_OVERBURN|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	/* This is a CDR with data data can be merged or at least appended */	\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_CD|				\
				  BRASERO_MEDIUM_WRITABLE|			\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_HAS_AUDIO|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_APPEND|			\
				  BRASERO_BURN_FLAG_MERGE|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_OVERBURN|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_APPEND);

#define BRASERO_PLUGIN_ADD_STANDARD_CDRW_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	/* Use DAO for first session since AUDIO needs it to write CD-TEXT */	\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_CD|				\
				  BRASERO_MEDIUM_REWRITABLE|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_DAO|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_OVERBURN|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	/* It is a CDRW we want the CD to be either blanked before or appended	\
	 * that's why we set MERGE as compulsory. That way if the CD is not 	\
	 * MERGED we force the blank before writing to avoid appending sessions	\
	 * endlessly until there is no free space. */				\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_CD|				\
				  BRASERO_MEDIUM_REWRITABLE|			\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_HAS_AUDIO|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_APPEND|			\
				  BRASERO_BURN_FLAG_MERGE|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_OVERBURN|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MERGE);

#define BRASERO_PLUGIN_ADD_STANDARD_DVDR_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	/* DAO and MULTI are exclusive */					\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDR|				\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_JUMP|				\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_DAO|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDR|				\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_JUMP|				\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	/* This is a DVDR with data; data can be merged or at least appended */	\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDR|				\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_JUMP|				\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_APPEND|			\
				  BRASERO_BURN_FLAG_MERGE|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_APPEND);

#define BRASERO_PLUGIN_ADD_STANDARD_DVDR_PLUS_FLAGS(plugin_MACRO, unsupported_MACRO)		\
	/* DVD+R don't have a DUMMY mode */					\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDR_PLUS|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_DAO|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDR_PLUS|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	/* DVD+R with data: data can be merged or at least appended */		\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDR_PLUS|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_MERGE|			\
				  BRASERO_BURN_FLAG_APPEND|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_APPEND);

#define BRASERO_PLUGIN_ADD_STANDARD_DVDRW_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDRW|				\
				  BRASERO_MEDIUM_UNFORMATTED|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_DAO|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDRW|				\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	/* This is a DVDRW we want the DVD to be either blanked before or	\
	 * appended that's why we set MERGE as compulsory. That way if the DVD	\
	 * is not MERGED we force the blank before writing to avoid appending	\
	 * sessions endlessly until there is no free space. */			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDRW|				\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_MERGE|			\
				  BRASERO_BURN_FLAG_APPEND|			\
				  BRASERO_BURN_FLAG_BURNPROOF|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MERGE);

/**
 * These kind of media don't support:
 * - BURNPROOF
 * - DAO
 * - APPEND
 * since they don't behave and are not written in the same way.
 * They also can't be closed so MULTI is compulsory.
 */
#define BRASERO_PLUGIN_ADD_STANDARD_DVDRW_PLUS_FLAGS(plugin_MACRO, unsupported_MACRO)		\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDRW_PLUS|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_UNFORMATTED|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MULTI);			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVDRW_PLUS|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_CLOSED|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE|			\
				  BRASERO_BURN_FLAG_MERGE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MULTI);

/**
 * The above statement apply to these as well. There is no longer dummy mode
 * NOTE: there is no such thing as a DVD-RW DL.
 */
#define BRASERO_PLUGIN_ADD_STANDARD_DVDRW_RESTRICTED_FLAGS(plugin_MACRO, unsupported_MACRO)	\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVD|				\
				  BRASERO_MEDIUM_RESTRICTED|			\
				  BRASERO_MEDIUM_REWRITABLE|			\
				  BRASERO_MEDIUM_UNFORMATTED|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MULTI);			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_DVD|				\
				  BRASERO_MEDIUM_RESTRICTED|			\
				  BRASERO_MEDIUM_REWRITABLE|			\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_CLOSED|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE|			\
				  BRASERO_BURN_FLAG_MERGE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MULTI);

#define BRASERO_PLUGIN_ADD_STANDARD_BD_R_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	/* DAO and MULTI are exclusive */					\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_BDR_RANDOM|			\
				  BRASERO_MEDIUM_BDR_SRM|			\
				  BRASERO_MEDIUM_BDR_SRM_POW|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_DAO|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_BDR_RANDOM|			\
				  BRASERO_MEDIUM_BDR_SRM|			\
				  BRASERO_MEDIUM_BDR_SRM_POW|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_NONE);			\
	/* This is a DVDR with data data can be merged or at least appended */	\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_BDR_RANDOM|			\
				  BRASERO_MEDIUM_BDR_SRM|			\
				  BRASERO_MEDIUM_BDR_SRM_POW|			\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_APPEND|			\
				  BRASERO_BURN_FLAG_MERGE|			\
				  BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_DUMMY|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_APPEND);

/**
 * These kind of media don't support:
 * - BURNPROOF
 * - DAO
 * - APPEND
 * since they don't behave and are not written in the same way.
 * They also can't be closed so MULTI is compulsory.
 */
#define BRASERO_PLUGIN_ADD_STANDARD_BD_RE_FLAGS(plugin_MACRO, unsupported_MACRO)			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_BDRE|				\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_UNFORMATTED|			\
				  BRASERO_MEDIUM_BLANK,				\
				  (BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MULTI);			\
	brasero_plugin_set_flags (plugin_MACRO,					\
				  BRASERO_MEDIUM_BDRE|				\
				  BRASERO_MEDIUM_DUAL_L|			\
				  BRASERO_MEDIUM_APPENDABLE|			\
				  BRASERO_MEDIUM_CLOSED|			\
				  BRASERO_MEDIUM_HAS_DATA,			\
				  (BRASERO_BURN_FLAG_MULTI|			\
				  BRASERO_BURN_FLAG_NOGRACE|			\
				  BRASERO_BURN_FLAG_MERGE) &			\
				  (~(unsupported_MACRO)),				\
				  BRASERO_BURN_FLAG_MULTI);

G_END_DECLS

#endif
