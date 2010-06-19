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

#ifndef _BRASERO_SESSION_CFG_H_
#define _BRASERO_SESSION_CFG_H_

#include <glib-object.h>

#include <brasero-session.h>
#include <brasero-session-span.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_SESSION_CFG             (brasero_session_cfg_get_type ())
#define BRASERO_SESSION_CFG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_SESSION_CFG, BraseroSessionCfg))
#define BRASERO_SESSION_CFG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_SESSION_CFG, BraseroSessionCfgClass))
#define BRASERO_IS_SESSION_CFG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_SESSION_CFG))
#define BRASERO_IS_SESSION_CFG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_SESSION_CFG))
#define BRASERO_SESSION_CFG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_SESSION_CFG, BraseroSessionCfgClass))

typedef struct _BraseroSessionCfgClass BraseroSessionCfgClass;
typedef struct _BraseroSessionCfg BraseroSessionCfg;

struct _BraseroSessionCfgClass
{
	BraseroSessionSpanClass parent_class;
};

struct _BraseroSessionCfg
{
	BraseroSessionSpan parent_instance;
};

GType brasero_session_cfg_get_type (void) G_GNUC_CONST;

/**
 * This is for the signal sent to tell whether or not session is valid
 */

typedef enum {
	BRASERO_SESSION_VALID				= 0,
	BRASERO_SESSION_NO_CD_TEXT			= 1,
	BRASERO_SESSION_NOT_READY,
	BRASERO_SESSION_EMPTY,
	BRASERO_SESSION_NO_INPUT_IMAGE,
	BRASERO_SESSION_UNKNOWN_IMAGE,
	BRASERO_SESSION_NO_INPUT_MEDIUM,
	BRASERO_SESSION_NO_OUTPUT,
	BRASERO_SESSION_INSUFFICIENT_SPACE,
	BRASERO_SESSION_OVERBURN_NECESSARY,
	BRASERO_SESSION_NOT_SUPPORTED,
	BRASERO_SESSION_DISC_PROTECTED
} BraseroSessionError;

#define BRASERO_SESSION_IS_VALID(result_MACRO)					\
	((result_MACRO) == BRASERO_SESSION_VALID ||				\
	 (result_MACRO) == BRASERO_SESSION_NO_CD_TEXT)

BraseroSessionCfg *
brasero_session_cfg_new (void);

BraseroSessionError
brasero_session_cfg_get_error (BraseroSessionCfg *cfg);

void
brasero_session_cfg_add_flags (BraseroSessionCfg *cfg,
			       BraseroBurnFlag flags);
void
brasero_session_cfg_remove_flags (BraseroSessionCfg *cfg,
				  BraseroBurnFlag flags);
gboolean
brasero_session_cfg_is_supported (BraseroSessionCfg *cfg,
				  BraseroBurnFlag flag);
gboolean
brasero_session_cfg_is_compulsory (BraseroSessionCfg *cfg,
				   BraseroBurnFlag flag);

gboolean
brasero_session_cfg_has_default_output_path (BraseroSessionCfg *cfg);

void
brasero_session_cfg_enable (BraseroSessionCfg *cfg);

void
brasero_session_cfg_disable (BraseroSessionCfg *cfg);

G_END_DECLS

#endif /* _BRASERO_SESSION_CFG_H_ */
