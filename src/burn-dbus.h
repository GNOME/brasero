#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef BUILD_DBUS

#include <glib.h>
#include <dbus/dbus-glib.h>

void
brasero_uninhibit_suspend (guint cookie);

DBusGConnection *
brasero_inhibit_suspend (const char *reason);

#endif
