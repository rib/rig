SUBDIRS = test-fixtures cogl

if BUILD_COGL_PANGO
SUBDIRS += cogl-pango
endif

if BUILD_COGL_GST
SUBDIRS += cogl-gst
endif

if BUILD_COGL_PATH
SUBDIRS += cogl-path
endif

SUBDIRS += tests examples

EXTRA_DIST = \
	README.in \
	config-custom.h

# .changelog expects these to be initializes
CLEANFILES=
DISTCLEANFILES=

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-maintainer-flags \
	--enable-profile \
	--enable-gles2 \
	--enable-gl \
	--enable-xlib-egl-platform \
	--enable-wayland-egl-platform \
	--enable-glx \
	--enable-wayland-egl-server  \
	--enable-cogl-gst
