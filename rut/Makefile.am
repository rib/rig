
lib_LTLIBRARIES = librut.la

source_h = \
    rut-exception.h \
    rut-os.h \
    rut-color.h \
    rut-type.h \
    rut-object.h \
    rut-types.h \
    rut-planes.h \
    rut-timeline.h \
    rut-display-list.h \
    rut.h \
    rut-arcball.h \
    rut-bitmask.h \
    rut-shell.h \
    rut-keysyms.h \
    rut-global.h \
    rut-util.h \
    rut-text-buffer.h \
    rut-text.h \
    rut-geometry.h \
    rut-context.h \
    rut-number-slider.h \
    rut-vec3-slider.h \
    rut-paintable.h \
    rut-drop-down.h \
    rut-closure.h \
    rut-stack.h \
    rut-entry.h \
    rut-gaussian-blurrer.h \
    rut-toggle.h \
    rut-mesh.h \
    rut-mesh-ply.h \
    rut-ui-viewport.h \
    rut-scroll-bar.h \
    rut-image.h \
    rut-volume.h \
    rut-color-picker.h \
    rut-color-button.h \
    rut-box-layout.h \
    rut-bin.h \
    rut-icon.h \
    rut-flow-layout.h \
    rut-button.h \
    rut-slider.h \
    rut-graph.h \
    rut-transform.h \
    rut-rectangle.h \
    rut-scale.h \
    rut-fixed.h \
    rut-fold.h \
    rut-refcount-debug.h \
    rut-icon-button.h

# Headers that will need to be available for compiling code at runtime
# and so need to be installed...
runtime_headers = \
    rut-property-bare.h \
    rut-property-types.h

runtime_headersdir = $(prefix)/share/rig/codegen_includes
runtime_headers_DATA = $(runtime_headers)

librut_la_SOURCES = \
    rply.c \
    rply.h \
    color-table.h \
    rut-exception.c \
    rut-os.c \
    rut-color.c \
    rut-type.c \
    rut-object.c \
    rut-property.h \
    rut-property.c \
    rut-introspectable.h \
    rut-introspectable.c \
    rut-interfaces.h \
    rut-interfaces.c \
    rut-graphable.h \
    rut-graphable.c \
    rut-matrix-stack.h \
    rut-matrix-stack.c \
    rut.c \
    rut-arcball.c \
    rut-transform-private.h \
    rut-volume.c \
    rut-volume-private.h \
    rut-planes.c \
    rut-shell.c \
    rut-bitmask.h \
    rut-bitmask.c \
    rut-flags.h \
    rut-memory-stack.h \
    rut-memory-stack.c \
    rut-magazine.h \
    rut-magazine.c \
    rut-queue.h \
    rut-queue.c \
    rut-list.c \
    rut-list.h \
    rut-util.c \
    rut-timeline.c \
    rut-display-list.c \
    rut-text-buffer.c \
    rut-text.c \
    rut-geometry.c \
    rut-number-slider.c \
    rut-vec3-slider.c \
    rut-paintable.c \
    rut-drop-down.c \
    rut-closure.c \
    rut-stack.c \
    rut-entry.c \
    rut-gaussian-blurrer.c \
    rut-toggle.c \
    rut-mesh.c \
    rut-mesh-ply.c \
    rut-ui-viewport.c \
    rut-scroll-bar.c \
    rut-image.c \
    rut-nine-slice.h \
    rut-nine-slice.c \
    rut-color-picker.c \
    rut-color-button.c \
    rut-box-layout.c \
    rut-bin.c \
    rut-icon.c \
    rut-flow-layout.c \
    rut-button.c \
    rut-slider.c \
    rut-graph.c \
    rut-transform.c \
    rut-rectangle.c \
    rut-scale.c \
    rut-fixed.c \
    rut-fold.c \
    rut-refcount-debug.c \
    rut-icon-button.c \
    rut-composite-sizable.h \
    rut-composite-sizable.c \
    rut-shim.h \
    rut-shim.c \
    rut-mimable.h \
    rut-mimable.c \
    rut-drag-bin.h \
    rut-drag-bin.c \
    rut-icon-toggle.h \
    rut-icon-toggle.c \
    rut-icon-toggle-set.h \
    rut-icon-toggle-set.c \
    rut-rotation-inspector.h \
    rut-rotation-inspector.c \
    rut-input-region.h \
    rut-input-region.c \
    rut-camera.h \
    rut-camera.c \
    rut-poll.h \
    rut-poll.c \
    rut-sdl-keysyms.c \
    rut-sdl-keysyms.h \
    rut-adb.h \
    rut-adb.c \
    $(source_h)

if USE_GSTREAMER
librut_la_SOURCES += \
    gstmemsrc.h \
    gstmemsrc.c
endif

if NEED_FMEMOPEN
librut_la_SOURCES += \
    fmemopen.h \
    fmemopen.c
endif

librut_la_LIBADD = \
    @RIG_DEP_LIBS@ \
    @RIG_EXTRA_LDFLAGS@
librut_la_LDFLAGS = \
    -no-undefined \
    -version-info @RIG_LT_CURRENT@:@RIG_LT_REVISION@:@RIG_LT_AGE@ \
    -export-dynamic \
    -export-symbols-regex "(^rut.*)|(^_rut.*)"
librut_la_CFLAGS = \
    $(RIG_EXTRA_CPPFLAGS) \
    @RIG_DEP_CFLAGS@ \
    @RIG_EXTRA_CFLAGS@
librut_la_CPPFLAGS = \
    -I$(top_srcdir) \
    -I$(top_builddir) \
    -I$(top_srcdir)/clib/src \
    -I$(top_builddir)/clib/src \
    -I$(top_srcdir)/rut

# rut_headersdir =  $(includedir)/rut
# rut_headers_DATA = $(source_h)
