project('rig',
        ['c', 'cpp'],
        license: 'MIT')


public_includes = include_directories('rut', 'rig')
inc = include_directories('rut', 'rig', 'rig/protobuf-c-rpc')

warn_opts = []
c_defines = [
  '-DRIG_BIN_DIR=""',
  '-DICU_DATA_DIR="/FIXME"',
  '-DUSE_UV=1'
]

deps = []

thread_dep = dependency('threads', required: true)
deps += thread_dep

ft_sp = subproject('freetype')
deps += ft_sp.get_variable('freetype_dep')
fc_sp = subproject('fontconfig')
deps += fc_sp.get_variable('fc_dep')
hb_sp = subproject('harfbuzz')
deps += hb_sp.get_variable('hb_dep')
clib_sp = subproject('clib')
clib_dep = clib_sp.get_variable('clib_dep')
deps += clib_dep
cglib_sp = subproject('cglib')
deps += cglib_sp.get_variable('cglib_dep')
xdgmime_sp = subproject('xdgmime')
deps += xdgmime_sp.get_variable('xdgmime_dep')
uv_sp = subproject('libuv')
deps += uv_sp.get_variable('libuv_dep')
wslay_sp = subproject('wslay')
deps += wslay_sp.get_variable('wslay_dep')
h2o_sp = subproject('h2o')
deps += h2o_sp.get_variable('h2o_dep')
icu_sp = subproject('icu')
deps += icu_sp.get_variable('icudata_dep')
deps += icu_sp.get_variable('icuuc_dep')


nsgif_inc = include_directories('nsgif/include', 'nsgif/src/utils')
nsgif_public_inc = include_directories('nsgif/include')
nsgif_lib = library('nsgif', [ 'nsgif/src/libnsgif.c' ],
                    include_directories: nsgif_inc)
nsgif_dep = declare_dependency(link_with: nsgif_lib, include_directories: nsgif_public_inc)
deps += nsgif_dep

protobuf_c_dep = subproject('protobuf-c').get_variable('protobuf_c_dep')
deps += protobuf_c_dep

if meson.is_cross_build()
  protoc_c = subproject('protobuf-c').get_variable('protoc_c_native')
else
  protoc_c = subproject('protobuf-c').get_variable('protoc_c')
endif

avcodec_dep = dependency('libavcodec', required: get_option('support_ffmpeg'))
avformat_dep = dependency('libavformat', required: get_option('support_ffmpeg'))
avutil_dep = dependency('libavutil', required: get_option('support_ffmpeg'))
swscale_dep = dependency('libswscale', required: get_option('support_ffmpeg'))
swresample_dep = dependency('libswresample', required: get_option('support_ffmpeg'))
if avcodec_dep.found() and avformat_dep.found() and avutil_dep.found() and swscale_dep.found() and swresample_dep.found()
  deps += avcodec_dep
  deps += avformat_dep
  deps += avutil_dep
  deps += swscale_dep
  deps += swresample_dep
  c_defines += [ '-DUSE_FFMPEG=1' ]
endif


rut_src = [
  'rut/rut.h',
  'rut/rut-types.h',
  'rut/rut-keysyms.h',
  'rut/color-table.h',
  'rut/rut-exception.h',
  'rut/rut-exception.c',
  'rut/rut-os.h',
  'rut/rut-os.c',
  'rut/rut-color.h',
  'rut/rut-color.c',
  'rut/rut-type.h',
  'rut/rut-type.c',
  'rut/rut-object.h',
  'rut/rut-object.c',
  'rut/rut-interfaces.h',
  'rut/rut-interfaces.c',
  'rut/rut-graphable.h',
  'rut/rut-graphable.c',
  'rut/rut-matrix-stack.h',
  'rut/rut-matrix-stack.c',
  'rut/rut-transform-private.h',
  'rut/rut-shell.h',
  'rut/rut-shell.c',
  'rut/rut-headless-shell.h',
  'rut/rut-headless-shell.c',
  'rut/rut-settings.h',
  'rut/rut-settings.c',
  'rut/rut-texture-cache.h',
  'rut/rut-texture-cache.c',
  'rut/rut-bitmask.h',
  'rut/rut-bitmask.c',
  'rut/rut-flags.h',
  'rut/rut-memory-stack.h',
  'rut/rut-memory-stack.c',
  'rut/rut-magazine.h',
  'rut/rut-magazine.c',
  'rut/rut-queue.h',
  'rut/rut-queue.c',
  'rut/rut-util.h',
  'rut/rut-util.c',
  'rut/rut-geometry.h',
  'rut/rut-geometry.c',
  'rut/rut-closure.h',
  'rut/rut-closure.c',
  'rut/rut-gaussian-blurrer.h',
  'rut/rut-gaussian-blurrer.c',
  'rut/rut-mesh.h',
  'rut/rut-mesh.c',
  'rut/rply.c',
  'rut/rply.h',
  'rut/rut-mesh-ply.h',
  'rut/rut-mesh-ply.c',
  'rut/rut-graph.h',
  'rut/rut-graph.c',
  'rut/rut-refcount-debug.h',
  'rut/rut-refcount-debug.c',
  'rut/rut-mimable.h',
  'rut/rut-mimable.c',
  'rut/rut-input-region.h',
  'rut/rut-input-region.c',
  'rut/rut-camera.h',
  'rut/rut-camera.c',
  'rut/rut-poll.h',
  'rut/rut-poll.c',
  'rut/rut-inputable.h',
  'rut/rut-meshable.h',
  'rut/rut-gradient.h',
  'rut/rut-gradient.c',
  'rut/edid-parse.h',
  'rut/edid-parse.c',
]

x11_dep = dependency('x11')
if x11_dep.found()
  deps += x11_dep
  deps += dependency('xdamage', required: true)
  deps += dependency('xcomposite', required: true)
  deps += dependency('xfixes', required: true)
  deps += dependency('xext', required: true)
  deps += dependency('xrandr', required: true)
  deps += dependency('xkbcommon-x11', required: true)
  deps += dependency('xi', required: true)
  deps += dependency('x11-xcb', required: true)
  c_defines += [ '-DUSE_X11=1' ]
  rut_src += [
    'rut/rut-x11-shell.h',
    'rut/rut-x11-shell.c',
  ]
endif

alsa_dep = dependency('alsa')
if alsa_dep.found()
  deps += alsa_dep
  c_defines += [ '-DUSE_ALSA=1' ]
  rut_src += [
    'rut/rut-alsa-shell.h',
    'rut/rut-alsa-shell.c',
  ]
endif


generated_pb_src = []
generated_pb_src += custom_target(
  'proto-files',
  input : [join_paths(meson.current_source_dir(), 'rig', 'rig.proto')],
  output : ['rig.pb-c.c', 'rig.pb-c.h'],
  command : [protoc_c, '--c_out=@OUTDIR@',
             '-I' + join_paths(meson.current_source_dir(), 'rig'),
             join_paths(meson.current_source_dir(), 'rig', 'rig.proto')])

rig_pb_rpc_src = [
  'rig/protobuf-c-rpc/rig-protobuf-c-stream.h',
  'rig/protobuf-c-rpc/rig-protobuf-c-stream.c',
  'rig/protobuf-c-rpc/rig-protobuf-c-rpc.h',
  'rig/protobuf-c-rpc/rig-protobuf-c-rpc.c',
]

rig_src = [
  'rig/components/rig-camera.h',
  'rig/components/rig-camera.c',
  'rig/components/rig-light.h',
  'rig/components/rig-light.c',
  'rig/components/rig-mesh.h',
  'rig/components/rig-mesh.c',
  'rig/components/rig-source.h',
  'rig/components/rig-source.c',
  'rig/components/rig-material.h',
  'rig/components/rig-material.c',
  'rig/components/rig-nine-slice.h',
  'rig/components/rig-nine-slice.c',
  'rig/components/rig-button-input.h',
  'rig/components/rig-button-input.c',
  'rig/components/rig-text.h',
  'rig/components/rig-text.c',
  'rig/rig-logs.h',
  'rig/rig-logs.c',
  'rig/rig-property.h',
  'rig/rig-property.c',
  'rig/rig-introspectable.h',
  'rig/rig-introspectable.c',
  'rig/rig-entity.h',
  'rig/rig-entity.c',
  'rig/rig-asset.h',
  'rig/rig-asset.c',
  'rig/rig-downsampler.h',
  'rig/rig-downsampler.c',
  'rig/rig-dof-effect.h',
  'rig/rig-dof-effect.c',
  'rig/rig-engine.c',
  'rig/rig-engine-op.h',
  'rig/rig-engine-op.c',
  'rig/rig-timeline.h',
  'rig/rig-timeline.c',
  'rig/rig-node.c',
  'rig/rig-node.h',
  'rig/rig-path.c',
  'rig/rig-path.h',
  'rig/rig-controller.c',
  'rig/rig-controller.h',
  'rig/rig-binding.h',
  'rig/rig-binding.c',
  'rig/rig-pb.h',
  'rig/rig-pb.c',
  'rig/rig-load-save.h',
  'rig/rig-load-save.c',
  'rig/rig-camera-view.h',
  'rig/rig-camera-view.c',
  'rig/rig-types.h',
  'rig/rut-renderer.h',
  'rig/rut-renderer.c',
  'rig/rig-renderer.h',
  'rig/rig-renderer.c',
  'rig/rig-engine.h',
  'rig/rig-rpc-network.h',
  'rig/rig-rpc-network.c',
  'rig/rig-slave-address.h',
  'rig/rig-slave-address.c',
  'rig/rig-simulator.h',
  'rig/rig-simulator-impl.c',
  'rig/rig-frontend.c',
  'rig/rig-frontend.h',
  'rig/rig-ui.h',
  'rig/rig-ui.c',
  'rig/rig-code.h',
  'rig/rig-code.c',
  'rig/rig-code-module.h',
  'rig/rig-code-module.c',
  'rig/rig-c.h',
  'rig/rig-c.c',
  'rig/rig-c-mesh.h',
  'rig/rig-c-mesh.c',
  'rig/usc_impl.h',
  'rig/usc_impl.c',
  'rig/rig-text-pipeline-cache.h',
  'rig/rig-text-pipeline-cache.c',
  'rig/rig-text-engine-funcs.h',
  'rig/rig-text-engine-funcs.c',
  'rig/rig-text-engine.h',
  'rig/rig-text-engine-private.h',
  'rig/rig-text-engine.c',
  'rig/rig-text-renderer.h',
  'rig/rig-text-renderer.c',

  'rig/components/rig-native-module.h',
  'rig/components/rig-native-module.c',
]

ncurses_dep = dependency('ncursesw')
if ncurses_dep.found()
  deps += ncurses_dep
  c_defines += [ '-DUSE_NCURSES=1' ]
  rig_src += [
    'rig/rig-curses-debug.h',
    'rig/rig-curses-debug.c'
  ]
endif

src = rut_src + generated_pb_src + rig_pb_rpc_src + rig_src

rig_lib = library(
        'rig', src,
        include_directories: inc,
        c_args: c_defines + warn_opts,
        cpp_args: c_defines + warn_opts,
        dependencies: deps)
rig_dep = declare_dependency(link_with: rig_lib, include_directories: public_includes)

rig_hello = executable('rig-hello', [ 'toys/rig-hello.c' ],
                       c_args: c_defines + warn_opts,
                       cpp_args: c_defines + warn_opts,
                       dependencies: [ rig_dep, clib_dep ],
                       export_dynamic: true)
