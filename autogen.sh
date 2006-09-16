#!/bin/sh
libtoolize -c
glib-gettextize --copy 
intltoolize -c --force  
aclocal 
autoconf  
autoheader 
automake --gnu --copy -a 
