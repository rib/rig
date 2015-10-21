SUBDIRS = libuv clib test-fixtures cglib research/particle-engine h2o

if EDITOR_ENABLED
SUBDIRS += xdgmime
endif

if OCULUS_RIFT
SUBDIRS += LibOVR
endif

SUBDIRS += rut data rig tests libyuv toys po android

SUBDIRS += tools

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config-custom.h

emscripten_platformdir = $(datadir)/platforms/emscripten
emscripten_platform_DATA = \
	Makefile.emscripten \
	config.h.emscripten
