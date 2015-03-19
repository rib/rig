if EMSCRIPTEN
SUBDIRS = clib cogl cogl-path rut rig toys

else

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

if EDITOR_ENABLED
SUBDIRS += xdgmime
endif

if OCULUS_RIFT
SUBDIRS += LibOVR
endif

SUBDIRS += rut data rig tests toys po android

if USE_GLIB
SUBDIRS += tools
endif

endif #EMSCRIPTEN

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config-custom.h

