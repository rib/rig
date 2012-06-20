
lib_LTLIBRARIES = librig.la

components_source_h = \
    components/rig-animation-clip.h \
    components/rig-camcorder.h \
    components/rig-light.h \
    components/rig-mesh-renderer.h

source_h = \
    $(components_source_h) \
    mash-data.h \
    mash-data-loader.h \
    mash-ply-loader.h \
    rig-entity.h \
    rig-components.h \
    rig-type.h \
    rig-object.h \
    rig-property.h \
    rig-types.h \
    rig-interfaces.h \
    rig-planes.h \
    rig-timeline.h \
    rig-display-list.h \
    rig.h \
    rig-arcball.h \
    rig-bitmask.h \
    rig-shell.h \
    rig-keysyms.h \
    rig-stack.h \
    rig-global.h

librig_la_SOURCES = \
    components/rig-animation-clip.c \
    components/rig-camcorder.c \
    components/rig-light.c \
    components/rig-mesh-renderer.c \
    mash-data.c \
    mash-data-loader.c \
    mash-ply-loader.c \
    rply.c \
    rply.h \
    rig-entity.c \
    rig-type.c \
    rig-object.c \
    rig-property.c \
    rig-interfaces.c \
    rig.c \
    rig-arcball.c \
    rig-transform-private.h \
    rig-camera-private.h \
    rig-volume.c \
    rig-volume-private.h \
    rig-planes.c \
    rig-sdl-keysyms.c \
    rig-shell.c \
    rig-bitmask.h \
    rig-bitmask.c \
    rig-flags.h \
    rig-stack.c \
    rig-stack.h \
    rig-queue.h \
    rig-util.h \
    rig-util.c \
    rig-timeline.c \
    rig-display-list.c \
    $(source_h)

librig_la_LIBADD = \
    @RIG_DEP_LIBS@ \
    @RIG_EXTRA_LDFLAGS@
librig_la_LDFLAGS = \
    -no-undefined \
    -version-info @RIG_LT_CURRENT@:@RIG_LT_REVISION@:@RIG_LT_AGE@ \
    -export-dynamic \
    -export-symbols-regex "(^rig.*)|(^_rig_bitmask.*)"
librig_la_CFLAGS = \
    @RIG_DEP_CFLAGS@ \
    @RIG_EXTRA_CFLAGS@
librig_la_CPPFLAGS = \
    -I$(top_srcdir) \
    -I$(top_srcdir)/rig \
    -DRIG_DATA_DIR=\""$(datadir)/"\"


rig_headersdir =  $(includedir)/rig
rig_headers_DATA = $(source_h)

rig_components_headersdir =  $(includedir)/rig/components
rig_components_headers_DATA = $(components_source_h)

pc_files = rig.pc
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pc_files)

EXTRA_DIST = rig.pc.in
DISTCLEANFILES = $(pc_files)
