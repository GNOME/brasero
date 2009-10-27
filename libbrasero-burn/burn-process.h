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

#ifndef PROCESS_H
#define PROCESS_H

#include <glib.h>
#include <glib-object.h>

#include "burn-job.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_PROCESS         (brasero_process_get_type ())
#define BRASERO_PROCESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_PROCESS, BraseroProcess))
#define BRASERO_PROCESS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_PROCESS, BraseroProcessClass))
#define BRASERO_IS_PROCESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_PROCESS))
#define BRASERO_IS_PROCESS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_PROCESS))
#define BRASERO_PROCESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_PROCESS, BraseroProcessClass))

typedef struct {
	BraseroJob parent;
} BraseroProcess;

typedef struct {
	BraseroJobClass parent_class;

	/* virtual functions */
	BraseroBurnResult	(*stdout_func)	(BraseroProcess *process,
						 const gchar *line);
	BraseroBurnResult	(*stderr_func)	(BraseroProcess *process,
						 const gchar *line);
	BraseroBurnResult	(*set_argv)	(BraseroProcess *process,
						 GPtrArray *argv,
						 GError **error);

	/* since burn-process.c doesn't know if it should call finished_session
	 * of finished track this allows to override the default call which is
	 * brasero_job_finished_track */
	BraseroBurnResult      	(*post)       	(BraseroJob *job);
} BraseroProcessClass;

GType brasero_process_get_type (void);

/**
 * This function allows to set an error that is used if the process doesn't 
 * return 0.
 */
void
brasero_process_deferred_error (BraseroProcess *process,
				GError *error);

void
brasero_process_set_working_directory (BraseroProcess *process,
				       const gchar *directory);

G_END_DECLS

#endif /* PROCESS_H */
