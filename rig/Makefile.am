
lib_LTLIBRARIES = librig.la

components_source_h = \
    components/rig-animation-clip.h \
    components/rig-camera.h \
    components/rig-light.h \
    components/rig-mesh-renderer.h \
    components/rig-material.h \
    components/rig-diamond.h

source_h = \
    $(components_source_h) \
    mash-data.h \
    mash-data-loader.h \
    mash-ply-loader.h \
    rig-color.h \
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
    rig-global.h \
    rig-util.h \
    rig-text-buffer.h \
    rig-text.h \
    rig-tool.h \
    rig-geometry.h \
    rig-context.h \
    rig-number-slider.h \
    rig-vec3-slider.h \
    rig-paintable.h \
    rig-drop-down.h \
    rig-inspector.h \
    rig-particle-engine.h \
    rig-ease.h

librig_la_SOURCES = \
    components/rig-animation-clip.c \
    components/rig-camera.c \
    components/rig-light.c \
    components/rig-mesh-renderer.c \
    components/rig-material.c \
    components/rig-diamond.c \
    mash-data.c \
    mash-data-loader.c \
    mash-ply-loader.c \
    rply.c \
    rply.h \
    color-table.h \
    rig-color.c \
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
    rig-shell.c \
    rig-bitmask.h \
    rig-bitmask.c \
    rig-flags.h \
    rig-stack.c \
    rig-stack.h \
    rig-list.c \
    rig-list.h \
    rig-util.c \
    rig-timeline.c \
    rig-display-list.c \
    rig-text-buffer.c \
    rig-text.c \
    rig-tool.c \
    rig-geometry.c \
    rig-number-slider.c \
    rig-vec3-slider.c \
    rig-paintable.c \
    rig-drop-down.c \
    rig-inspector.c \
    rig-particle-engine.c \
    rig-ease.c \
    $(source_h)

if !HAVE_ANDROID
librig_la_SOURCES += \
    rig-sdl-keysyms.c
endif

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
