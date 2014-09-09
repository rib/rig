SUBDIRS = clib test-fixtures cogl research/particle-engine

if BUILD_COGL_PANGO
SUBDIRS += cogl-pango
endif

if BUILD_COGL_GST
SUBDIRS += cogl-gst
endif

if BUILD_COGL_PATH
SUBDIRS += cogl-path
endif

SUBDIRS += rut data rig tests cogl-examples po

if USE_GLIB
SUBDIRS += tools
endif

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config-custom.h

