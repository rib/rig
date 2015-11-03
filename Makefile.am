SUBDIRS = libuv clib test-fixtures cglib research/particle-engine

if OCULUS_RIFT
SUBDIRS += LibOVR
endif

SUBDIRS += rut data rig tests toys po android

SUBDIRS += tools

ACLOCAL_AMFLAGS = -I build/autotools ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	config-custom.h

emscripten_platformdir = $(datadir)/platforms/emscripten
emscripten_platform_DATA = \
	Makefile.emscripten \
	config.h.emscripten
