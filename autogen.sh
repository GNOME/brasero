#!/bin/sh
# Run this to generate all the initial makefiles, etc.
set -e

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.

PKG_NAME="brasero"

(test -f "$srcdir/configure.ac" \
  && test -f "$srcdir/README" \
  && test -d "$srcdir/src") || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

# Run standard autoreconf to generate build files
autoreconf --force --install --verbose "$srcdir"

# Run configure unless NOCONFIGURE is set (for tools like GNOME Builder / flatpak-builder)
if [ -z "$NOCONFIGURE" ]; then
    "$srcdir/configure" "$@"
fi
