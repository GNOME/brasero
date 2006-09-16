/***************************************************************************
 *            burn-task.h
 *
 *  mer sep 13 09:16:29 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#ifndef BURN_TASK_H
#define BURN_TASK_H

#include <glib.h>
#include <glib-object.h>

#include "burn-basics.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_TASK         (brasero_task_get_type ())
#define BRASERO_TASK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_TASK, BraseroTask))
#define BRASERO_TASK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_TASK, BraseroTaskClass))
#define BRASERO_IS_TASK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_TASK))
#define BRASERO_IS_TASK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_TASK))
#define BRASERO_TASK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_TASK, BraseroTaskClass))

typedef struct _BraseroTask BraseroTask;
typedef struct _BraseroTaskPrivate BraseroTaskPrivate;
typedef struct _BraseroTaskClass BraseroTaskClass;

struct _BraseroTask {
	GObject parent;
	BraseroTaskPrivate *priv;
};

struct _BraseroTaskClass {
	GObjectClass parent_class;

	/* signals */
	void			(*progress_changed)	(BraseroTask *task,
							 gdouble fraction,
							 glong remaining_time);
	void			(*action_changed)	(BraseroTask *task,
							 BraseroBurnAction action);
	void			(*clock_tick)		(BraseroTask *task);
};

GType brasero_task_get_type ();
BraseroTask *brasero_task_new ();

BraseroBurnResult
brasero_task_start (BraseroTask *task,
		    GError **error);
void
brasero_task_stop (BraseroTask *task,
		   BraseroBurnResult retval,
		   GError *error);

BraseroBurnResult
brasero_task_start_progress (BraseroTask *task,
			     gboolean force);

BraseroBurnResult
brasero_task_get_rate (BraseroTask *task,
		       gint64 *rate);
BraseroBurnResult
brasero_task_get_average_rate (BraseroTask *task,
			       gint64 *rate);
BraseroBurnResult
brasero_task_get_remaining_time (BraseroTask *task,
				 long *remaining);
BraseroBurnResult
brasero_task_get_total (BraseroTask *task,
			gint64 *total);
BraseroBurnResult
brasero_task_get_written (BraseroTask *task,
			  gint64 *written);
BraseroBurnResult
brasero_task_get_action_string (BraseroTask *task,
				BraseroBurnAction action,
				gchar **string);
BraseroBurnResult
brasero_task_get_elapsed (BraseroTask *task,
			  gdouble *elapsed);
BraseroBurnResult
brasero_task_get_progress (BraseroTask *task, 
			   gdouble *progress);
BraseroBurnResult
brasero_task_get_action (BraseroTask *task,
			 BraseroBurnAction *action);

BraseroBurnResult
brasero_task_set_rate (BraseroTask *task,
		       gint64 rate);
BraseroBurnResult
brasero_task_set_total (BraseroTask *task,
			gint64 total);
BraseroBurnResult
brasero_task_set_written (BraseroTask *task,
			  gint64 written);
BraseroBurnResult
brasero_task_set_progress (BraseroTask *task,
			   gdouble progress);
BraseroBurnResult
brasero_task_set_action (BraseroTask *task,
			 BraseroBurnAction action,
			 const gchar *string,
			 gboolean force);

/* This is for apps with a jerky current rate (like cdrdao) */
void
brasero_task_set_use_average_rate (BraseroTask *task, gboolean value);

G_END_DECLS

#endif /* BURN_TASK_H */
