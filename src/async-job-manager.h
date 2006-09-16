/***************************************************************************
 *            async-job-manager.h
 *
 *  ven avr  7 14:39:35 2006
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

#ifndef ASYNC_JOB_MANAGER_H
#define ASYNC_JOB_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_ASYNC_JOB_MANAGER         (brasero_async_job_manager_get_type ())
#define BRASERO_ASYNC_JOB_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_ASYNC_JOB_MANAGER, BraseroAsyncJobManager))
#define BRASERO_ASYNC_JOB_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_ASYNC_JOB_MANAGER, BraseroAsyncJobManagerClass))
#define BRASERO_IS_ASYNC_JOB_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_ASYNC_JOB_MANAGER))
#define BRASERO_IS_ASYNC_JOB_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_ASYNC_JOB_MANAGER))
#define BRASERO_ASYNC_JOB_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_ASYNC_JOB_MANAGER, BraseroAsyncJobManagerClass))

typedef struct BraseroAsyncJobManagerPrivate BraseroAsyncJobManagerPrivate;

typedef gboolean	(*BraseroAsyncRunJob)		(GObject *object, gpointer data);
typedef gboolean	(*BraseroSyncGetResult)	(GObject *object, gpointer data);
typedef void		(*BraseroAsyncDestroy)	(GObject *object, gpointer data);
typedef void		(*BraseroAsyncCancelJob)	(gpointer data);

typedef gboolean	(*BraseroAsyncFindJob)	(gpointer data, gpointer user_data);

typedef struct {
	GObject parent;
	BraseroAsyncJobManagerPrivate *priv;
} BraseroAsyncJobManager;

typedef struct {
	GObjectClass parent_class;
} BraseroAsyncJobManagerClass;

GType brasero_async_job_manager_get_type ();
BraseroAsyncJobManager *brasero_async_job_manager_get_default ();

void
brasero_async_job_manager_cancel_by_object (BraseroAsyncJobManager *manager,
					    GObject *obj);
gboolean
brasero_async_job_manager_queue (BraseroAsyncJobManager *manager,
				 gint type,
				 gpointer data);
gboolean
brasero_async_job_manager_find_urgent_job (BraseroAsyncJobManager *manager,
					   gint type,
					   BraseroAsyncFindJob func,
					   gpointer user_data);
gint
brasero_async_job_manager_register_type (BraseroAsyncJobManager *manager,
					 GObject *object,
					 BraseroAsyncRunJob run,
					 BraseroSyncGetResult results,
					 BraseroAsyncDestroy destroy,
					 BraseroAsyncCancelJob cancel);
void
brasero_async_job_manager_unregister_type (BraseroAsyncJobManager *manager,
					   gint type);
#endif /* ASYNC_JOB_MANAGER_H */
