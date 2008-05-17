
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>

void
brasero_uninhibit_suspend (guint cookie);

gint
brasero_inhibit_suspend (const char *reason);

