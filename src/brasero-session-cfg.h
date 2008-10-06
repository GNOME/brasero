/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_SESSION_CFG_H_
#define _BRASERO_SESSION_CFG_H_

#include <glib-object.h>

#include "burn-basics.h"
#include "burn-session.h"

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
	BraseroBurnSessionClass parent_class;
};

struct _BraseroSessionCfg
{
	BraseroBurnSession parent_instance;
};

GType brasero_session_cfg_get_type (void) G_GNUC_CONST;

#define BRASERO_DRIVE_PROPERTIES_FLAGS	       (BRASERO_BURN_FLAG_DUMMY|	\
						BRASERO_BURN_FLAG_EJECT|	\
						BRASERO_BURN_FLAG_BURNPROOF|	\
						BRASERO_BURN_FLAG_NO_TMP_FILES)

/**
 * This is for the signal sent to tell whether or not session is valid
 */

typedef enum {
	BRASERO_SESSION_VALID				= 0,
	BRASERO_SESSION_NO_INPUT_IMAGE			= 1,
	BRASERO_SESSION_UNKNOWN_IMAGE,
	BRASERO_SESSION_NO_INPUT_MEDIUM,
	BRASERO_SESSION_NO_OUTPUT,
	BRASERO_SESSION_INSUFFICIENT_SPACE,
	BRASERO_SESSION_OVERBURN_NECESSARY,
	BRASERO_SESSION_NOT_SUPPORTED,
	BRASERO_SESSION_DISC_PROTECTED
} BraseroSessionError;

BraseroSessionCfg *
brasero_session_cfg_new (void);

BraseroSessionError
brasero_session_cfg_get_error (BraseroSessionCfg *cfg);

void
brasero_session_cfg_add_flags (BraseroSessionCfg *cfg,
			       BraseroBurnFlag flags);

void
brasero_session_cfg_disable (BraseroSessionCfg *self);


/**
 * This tag (for sessions) is used to set an estimated size, used to determine
 * in the burn option dialog is the selected medium is big enough.
 */

#define BRASERO_DATA_TRACK_SIZE_TAG	"track::data::estimated_size"
#define BRASERO_AUDIO_TRACK_SIZE_TAG	"track::audio::estimated_size"

G_END_DECLS

#endif /* _BRASERO_SESSION_CFG_H_ */
