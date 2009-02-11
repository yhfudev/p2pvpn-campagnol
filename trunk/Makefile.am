ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = man common
if BUILD_CLIENT
SUBDIRS += client samples
endif
if BUILD_SERVER
SUBDIRS += rdvserver
endif
EXTRA_DIST = Changelog \
	lib/getline.c lib/getline.h \
	lib/tdestroy.c lib/tdestroy.h
