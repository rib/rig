{
  "includes": [
    "common.gypi"
  ],

  'target_defaults': {
    'cflags_c': [
      '-std=c11',
      '-Wno-sign-compare',
    ],
    'xcode_settings': {
      'WARNING_CFLAGS': [
        '-Wall',
        '-Wextra',
        '-Wno-unused-parameter'
      ],
      'OTHER_LDFLAGS': [
      ],
      'OTHER_CFLAGS': [
        '-g',
        '-std=c11',
      ],
    }
  },

  'targets': [
    {
      'target_name': 'freetype',
      'type': 'static_library',
      'dependencies': [
#        'zlib/zlib.gyp:zlib'
       ],
      'include_dirs': [
          'freetype/include',
          'freetype/include/freetype',
          'freetype/objs',
          'freetype/builds/unix'
      ],
      'all_dependent_settings': {
        'include_dirs': [
          'freetype/include',
          'freetype/include/freetype',
          'freetype/objs',
          'freetype/builds/unix'
	],
        'libraries': [ '-lz' ],
      },
      'libraries': [ '-lz' ],
      'defines': [
        '_ALL_SOURCE=1',
        '_GNU_SOURCE=1',
        'FT_CONFIG_CONFIG_H=<ftconfig.h>',
        'FT2_BUILD_LIBRARY',
        'FT_CONFIG_OPTION_SYSTEM_ZLIB',
      ],
      'sources': [
        'freetype/builds/unix/ftsystem.c',
        'freetype/src/base/ftdebug.c',
        'freetype/src/base/ftinit.c',
        'freetype/src/base/ftbase.c',
        'freetype/src/base/ftbbox.c',
        'freetype/src/base/ftbdf.c',
        'freetype/src/base/ftbitmap.c',
        'freetype/src/base/ftcid.c',
        'freetype/src/base/ftfstype.c',
        'freetype/src/base/ftgasp.c',
        'freetype/src/base/ftglyph.c',
        'freetype/src/base/ftgxval.c',
        'freetype/src/base/ftlcdfil.c',
        'freetype/src/base/ftmm.c',
        'freetype/src/base/ftotval.c',
        'freetype/src/base/ftpatent.c',
        'freetype/src/base/ftpfr.c',
        'freetype/src/base/ftstroke.c',
        'freetype/src/base/ftsynth.c',
        'freetype/src/base/fttype1.c',
        'freetype/src/base/ftwinfnt.c',
        'freetype/src/base/ftxf86.c',
        'freetype/src/truetype/truetype.c',
        'freetype/src/type1/type1.c',
        'freetype/src/cff/cff.c',
        'freetype/src/cid/type1cid.c',
        'freetype/src/pfr/pfr.c',
        'freetype/src/type42/type42.c',
        'freetype/src/winfonts/winfnt.c',
        'freetype/src/pcf/pcf.c',
        'freetype/src/bdf/bdf.c',
        'freetype/src/sfnt/sfnt.c',
        'freetype/src/autofit/autofit.c',
        'freetype/src/pshinter/pshinter.c',
        'freetype/src/raster/raster.c',
        'freetype/src/smooth/smooth.c',
        'freetype/src/cache/ftcache.c',
        'freetype/src/gzip/ftgzip.c',
        'freetype/src/lzw/ftlzw.c',
        'freetype/src/psaux/psaux.c',
        'freetype/src/psnames/psmodule.c',
      ],
    },
    {
      'target_name': 'hb',
      'type': 'static_library',
      'dependencies': [
         'freetype',
         'icu/source/icu.gyp:icuuc'
       ],
      'include_dirs': [
        'harfbuzz/src',
        'harfbuzz/src/hb-ucdn'
      ],
      'all_dependent_settings': {
        'include_dirs': [
          'harfbuzz/src'
	],
      },
      'defines': [
        '_ALL_SOURCE=1',
        '_GNU_SOURCE=1',
        'STDC_HEADERS=1',
        'HAVE_ATEXIT=1',
        'HAVE_FALLBACK=1',
        'HAVE_FONTCONFIG=1',
        'HAVE_FREETYPE=1',
        'HAVE_GETPAGESIZE=1',
        'HAVE_ICU=1',
        'HAVE_INTEL_ATOMIC_PRIMITIVES=1',
        'HAVE_ISATTY=1',
        'HAVE_MMAP=1',
        'HAVE_MPROTECT=1',
        'HAVE_OT=1',
        'HAVE_PTHREAD=1',
        'HAVE_PTHREAD_PRIO_INHERIT=1',
        'HAVE_SYSCONF=1',
        'HAVE_SYS_MMAN_H=1',
        'HAVE_UCDN=1',
        'HAVE_UNISTD_H=1',
      ],
      'libraries': [
      ],
      'sources': [
	'harfbuzz/src/hb-atomic-private.hh',
	'harfbuzz/src/hb-blob.cc',
	'harfbuzz/src/hb-buffer-deserialize-json.hh',
	'harfbuzz/src/hb-buffer-deserialize-text.hh',
	'harfbuzz/src/hb-buffer-private.hh',
	'harfbuzz/src/hb-buffer-serialize.cc',
	'harfbuzz/src/hb-buffer.cc',
	'harfbuzz/src/hb-cache-private.hh',
	'harfbuzz/src/hb-common.cc',
	'harfbuzz/src/hb-face-private.hh',
	'harfbuzz/src/hb-face.cc',
	'harfbuzz/src/hb-font-private.hh',
	'harfbuzz/src/hb-font.cc',
	'harfbuzz/src/hb-mutex-private.hh',
	'harfbuzz/src/hb-object-private.hh',
	'harfbuzz/src/hb-open-file-private.hh',
	'harfbuzz/src/hb-open-type-private.hh',
	'harfbuzz/src/hb-ot-cmap-table.hh',
	'harfbuzz/src/hb-ot-glyf-table.hh',
	'harfbuzz/src/hb-ot-head-table.hh',
	'harfbuzz/src/hb-ot-hhea-table.hh',
	'harfbuzz/src/hb-ot-hmtx-table.hh',
	'harfbuzz/src/hb-ot-maxp-table.hh',
	'harfbuzz/src/hb-ot-name-table.hh',
	'harfbuzz/src/hb-ot-tag.cc',
	'harfbuzz/src/hb-private.hh',
	'harfbuzz/src/hb-set-private.hh',
	'harfbuzz/src/hb-set.cc',
	'harfbuzz/src/hb-shape.cc',
	'harfbuzz/src/hb-shape-plan-private.hh',
	'harfbuzz/src/hb-shape-plan.cc',
	'harfbuzz/src/hb-shaper-list.hh',
	'harfbuzz/src/hb-shaper-impl-private.hh',
	'harfbuzz/src/hb-shaper-private.hh',
	'harfbuzz/src/hb-shaper.cc',
	'harfbuzz/src/hb-unicode-private.hh',
	'harfbuzz/src/hb-unicode.cc',
	'harfbuzz/src/hb-utf-private.hh',
	'harfbuzz/src/hb-warning.cc',

	'harfbuzz/src/hb-ot-font.cc',
	'harfbuzz/src/hb-ot-layout.cc',
	'harfbuzz/src/hb-ot-layout-common-private.hh',
	'harfbuzz/src/hb-ot-layout-gdef-table.hh',
	'harfbuzz/src/hb-ot-layout-gpos-table.hh',
	'harfbuzz/src/hb-ot-layout-gsubgpos-private.hh',
	'harfbuzz/src/hb-ot-layout-gsub-table.hh',
	'harfbuzz/src/hb-ot-layout-jstf-table.hh',
	'harfbuzz/src/hb-ot-layout-private.hh',
	'harfbuzz/src/hb-ot-map.cc',
	'harfbuzz/src/hb-ot-map-private.hh',
	'harfbuzz/src/hb-ot-shape.cc',
	'harfbuzz/src/hb-ot-shape-complex-arabic.cc',
	'harfbuzz/src/hb-ot-shape-complex-arabic-fallback.hh',
	'harfbuzz/src/hb-ot-shape-complex-arabic-private.hh',
	'harfbuzz/src/hb-ot-shape-complex-arabic-table.hh',
	'harfbuzz/src/hb-ot-shape-complex-arabic-win1256.hh',
	'harfbuzz/src/hb-ot-shape-complex-default.cc',
	'harfbuzz/src/hb-ot-shape-complex-hangul.cc',
	'harfbuzz/src/hb-ot-shape-complex-hebrew.cc',
	'harfbuzz/src/hb-ot-shape-complex-indic.cc',
	'harfbuzz/src/hb-ot-shape-complex-indic-machine.hh',
	'harfbuzz/src/hb-ot-shape-complex-indic-private.hh',
	'harfbuzz/src/hb-ot-shape-complex-indic-table.cc',
	'harfbuzz/src/hb-ot-shape-complex-myanmar.cc',
	'harfbuzz/src/hb-ot-shape-complex-myanmar-machine.hh',
	'harfbuzz/src/hb-ot-shape-complex-thai.cc',
	'harfbuzz/src/hb-ot-shape-complex-tibetan.cc',
	'harfbuzz/src/hb-ot-shape-complex-use.cc',
	'harfbuzz/src/hb-ot-shape-complex-use-machine.hh',
	'harfbuzz/src/hb-ot-shape-complex-use-private.hh',
	'harfbuzz/src/hb-ot-shape-complex-use-table.cc',
	'harfbuzz/src/hb-ot-shape-complex-private.hh',
	'harfbuzz/src/hb-ot-shape-normalize-private.hh',
	'harfbuzz/src/hb-ot-shape-normalize.cc',
	'harfbuzz/src/hb-ot-shape-fallback-private.hh',
	'harfbuzz/src/hb-ot-shape-fallback.cc',
	'harfbuzz/src/hb-ot-shape-private.hh',

	'harfbuzz/src/hb-fallback-shape.cc',

	'harfbuzz/src/hb-ft.cc',
	'harfbuzz/src/hb-ft.h',

	'harfbuzz/src/hb-icu.cc',
	'harfbuzz/src/hb-icu.h',

	'harfbuzz/src/hb-ucdn.cc',
	'harfbuzz/src/hb-ucdn/ucdn.h',
	'harfbuzz/src/hb-ucdn/ucdn.c',
	'harfbuzz/src/hb-ucdn/unicodedata_db.h',

      ],
    },
    {
      'target_name': 'fontconfig',
      'type': 'static_library',
      'dependencies': [
        'freetype',
#        'libxml2/libxml.gyp:libxml'
       ],
      'include_dirs': [
        'fontconfig'
      ],
      'all_dependent_settings': {
        'include_dirs': [
          'fontconfig'
	],
        'libraries': [
            '-lexpat'
        ],
      },
      'defines': [
        '_ALL_SOURCE=1',
        '_GNU_SOURCE=1',
        'STDC_HEADERS=1',
        'FLEXIBLE_ARRAY_MEMBER',
        'HAVE_FCNTL_H=1',
        'HAVE_FSTATVFS=1',
        'HAVE_FSTATFS=1',
        'HAVE_PTHREAD=1',
        'HAVE_STRUCT_STATFS_F_FLAGS=1',
        'HAVE_FT_BITMAP_SIZE_Y_PPEM=1',
        'HAVE_FT_GET_BDF_PROPERTY=1',
        'HAVE_FT_GET_NEXT_CHAR=1',
        'HAVE_FT_GET_PS_FONT_INFO=1',
        'HAVE_FT_GET_X11_FONT_FORMAT=1',
        'HAVE_FT_HAS_PS_GLYPH_NAMES=1',
        'HAVE_FT_SELECT_SIZE=1',
        'HAVE_GETOPT=1',
        'HAVE_GETOPT_LONG=1',
        'HAVE_LINK=1',
        'HAVE_LSTAT=1',
        'HAVE_MKDTEMP=1',
        'HAVE_MKOSTEMP=1',
        'HAVE_MKSTEMP=1',
        'HAVE_MMAP=1',
        'HAVE_POSIX_FADVISE=1',
        'HAVE_RAND=1',
        'HAVE_RAND_R=1',
        'HAVE_RANDOM=1',
        'HAVE_RANDOM_R=1',
        'HAVE_LRAND48=1',
        'HAVE_STDINT_H=1',
        'HAVE_STRUCT_DIRENT_D_TYPE=1',
        'HAVE_SYS_MOUNT_H=1',
        'HAVE_SYS_PARAM_H=1',
        'HAVE_SYS_STATFS_H=1',
        'HAVE_SYS_STATVFS_H=1',
        'HAVE_SYS_STAT_H=1',
        'HAVE_SYS_TYPES_H=1',
        'HAVE_SYS_VFS_H=1',
        'HAVE_WARNING_CPP_DIRECTIVE=1',
        'HAVE_INTEL_ATOMIC_PRIMITIVES=1',
        'USE_ICONV=0',
#        'ENABLE_LIBXML2=1',
        'VERSION="2.11.93"',
        'FC_DEFAULT_FONTS="/usr/share/fonts"',
        'FC_ADD_FONTS="yes"',
        'FC_CACHEDIR="/usr/local/var/cache/fontconfig"',
        'FONTCONFIG_PATH="/usr/local/etc/fonts"',
      ],
      'cflags': [
        '-include config-fixups.h'
      ],
      'libraries': [
        '-lexpat'
      ],
      'sources': [
	'fontconfig/src/fcarch.h',
	'fontconfig/src/fcatomic.c',
	'fontconfig/src/fcatomic.h',
	'fontconfig/src/fcblanks.c',
	'fontconfig/src/fccache.c',
	'fontconfig/src/fccfg.c',
	'fontconfig/src/fccharset.c',
	'fontconfig/src/fccompat.c',
	'fontconfig/src/fcdbg.c',
	'fontconfig/src/fcdefault.c',
	'fontconfig/src/fcdir.c',
	'fontconfig/src/fcformat.c',
	'fontconfig/src/fcfreetype.c',
	'fontconfig/src/fcfs.c',
	'fontconfig/src/fcinit.c',
	'fontconfig/src/fclang.c',
	'fontconfig/src/fclist.c',
	'fontconfig/src/fcmatch.c',
	'fontconfig/src/fcmatrix.c',
	'fontconfig/src/fcmutex.h',
	'fontconfig/src/fcname.c',
	'fontconfig/src/fcobjs.c',
	'fontconfig/src/fcobjs.h',
	'fontconfig/src/fcobjshash.h',
	'fontconfig/src/fcpat.c',
	'fontconfig/src/fcrange.c',
	'fontconfig/src/fcserialize.c',
	'fontconfig/src/fcstat.c',
	'fontconfig/src/fcstr.c',
	'fontconfig/src/fcweight.c',
	'fontconfig/src/fcwindows.h',
	'fontconfig/src/fcxml.c',
	'fontconfig/src/ftglue.h',
	'fontconfig/src/ftglue.c',
      ],
    },
    {
      'target_name': 'xdgmime',
      'type': 'static_library',
      'dependencies': [
	 'libuv/uv.gyp:libuv',
         'clib/clib.gyp:clib',
       ],
      'include_dirs': [
          'xdgmime'
      ],
      'all_dependent_settings': {
        'include_dirs': [
          'xdgmime'
	],
      },
      'defines': [
        '_ALL_SOURCE=1',
        '_GNU_SOURCE=1',
      ],
      'sources': [
        'xdgmime/glob.c',
        'xdgmime/glob.h',
        'xdgmime/magic.c',
        'xdgmime/magic.h',
        'xdgmime/xdgmime-query.c',
        'xdgmime/xdgmime.c',
        'xdgmime/xdgmime.h',
      ],
    },
    {
      'target_name': 'rig',
      'type': 'static_library',
      'dependencies': [
         'clib/clib.gyp:clib',
         'cglib/cglib.gyp:cglib',
         'freetype',
         'fontconfig',
         'icu/source/icu.gyp:icuuc',
         'icu/source/icu.gyp:icudata',
         'hb',
          'protobuf-c/protobuf-c.gyp:protoc-c#host',
         'protobuf-c/protobuf-c.gyp:protobuf-c',
	 'test-fixtures/test-fixtures.gyp:libtest-fixtures',
         'LibOVR/libovr.gyp:libovr',
	 'wslay/wslay.gyp:libwslay',
	 'h2o/h2o.gyp:libh2o',
       ],
      'include_dirs': [
        '.',
        'rut',
        'rig',
        'rig/protobuf-c-rpc',
      ],
      'all_dependent_settings': {
        'include_dirs': [
	  'rut',
	  'rig',
	],
      },
      'actions': [
        {
          'action_name': 'rig-proto',
          'inputs': [
            'rig/rig.proto'
          ],
          'outputs': [
            'rig/rig.pb-c.h',
            'rig/rig.pb-c.c'
          ],
          'action': [ '<(PRODUCT_DIR)/protoc-c', '--c_out=rig', '-Irig', '<@(_inputs)' ]
        }
      ],
      'sources': [

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
	'rut/rut-property.h',
	'rut/rut-property.c',
	'rut/rut-introspectable.h',
	'rut/rut-introspectable.c',
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
	'rut/rut-paintable.h',
	'rut/rut-paintable.c',
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

	'rig/protobuf-c-rpc/rig-protobuf-c-stream.h',
	'rig/protobuf-c-rpc/rig-protobuf-c-stream.c',
	'rig/protobuf-c-rpc/rig-protobuf-c-rpc.h',
	'rig/protobuf-c-rpc/rig-protobuf-c-rpc.c',

	'rig/components/rig-hair.h',
	'rig/components/rig-hair.c',
	'rig/components/rig-camera.h',
	'rig/components/rig-camera.c',
	'rig/components/rig-light.h',
	'rig/components/rig-light.c',
	'rig/components/rig-model.h',
	'rig/components/rig-model.c',
	'rig/components/rig-material.h',
	'rig/components/rig-material.c',
	'rig/components/rig-diamond.h',
	'rig/components/rig-diamond.c',
	'rig/components/rig-shape.h',
	'rig/components/rig-shape.c',
	'rig/components/rig-nine-slice.h',
	'rig/components/rig-nine-slice.c',
	'rig/components/rig-pointalism-grid.h',
	'rig/components/rig-pointalism-grid.c',
	'rig/components/rig-button-input.h',
	'rig/components/rig-button-input.c',
	'rig/components/rig-text.h',
	'rig/components/rig-text.c',
	'rig/rig-logs.h',
	'rig/rig-logs.c',
	'rig/rig-entity.h',
	'rig/rig-entity.c',
	'rig/rig-asset.h',
	'rig/rig-asset.c',
	'rig/rig-image-source.h',
	'rig/rig-image-source.c',
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
	'rig/rig-c.c',
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

        'rig/rig.pb-c.h',
        'rig/rig.pb-c.c',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
          ],
        }, { # Not Windows i.e. POSIX
          'cflags': [
            '-g',
            '-std=c99',
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter',
          ],
          'link_settings': {
            'libraries': [ '-lm' ],
            'conditions': [
              ['OS != "android"', {
                'ldflags': [ '-pthread' ],
              }],
            ],
          },
          'conditions': [
            ['_type=="shared_library"', {
              'cflags': [ '-fPIC' ],
	      'defines': [
		'ENABLE_UNIT_TESTS'
	      ],
            }],
#            ['_type=="shared_library" and OS!="mac"', {
#              'link_settings': {
#                'libraries': [ '-Wl,-soname,librig.so.1.0' ],
#              },
#            }],
          ],
        }],
        [ 'OS=="emscripten"', {
          'sources': [
          ],
        }],
        [ 'OS=="android"', {
          'sources': [
          ],
        }],
        [ 'OS=="mac"', {
          'defines': [
            '_DARWIN_USE_64_BIT_INODE=1'
          ],
        }],
        [ 'OS!="mac"', {
          # Enable on all platforms except OS X. The antique gcc/clang that
          # ships with Xcode emits waaaay too many false positives.
          'cflags': [ '-Wstrict-aliasing' ],
        }],
        [ 'enable_uv==1', {
          'dependencies': [
            'libuv/uv.gyp:libuv'
          ],
          'defines': [ 'USE_UV=1' ],
          'sources': [
	    'rig/components/rig-native-module.h',
	    'rig/components/rig-native-module.c',
          ]
        }],
        [ 'enable_glib==1', {
          'sources': [
          ]
        }],
        [ 'enable_sdl==1', {
          'sources': [
          ]
        }],
        [ 'enable_x11==1', {
          'sources': [
	    'rut/rut-x11-shell.h',
	    'rut/rut-x11-shell.c'
          ]
        }],
        [ 'enable_oculus_rift==1', {
	  'include_dirs': [
	    'LibOVR/Src'
          ]
        }],
        [ 'enable_ncurses==1', {
          'sources': [
	    'rig/rig-curses-debug.h',
	    'rig/rig-curses-debug.c'
          ]
        }],

      ]
    },
    {
      'target_name': 'rig-hello',
      'type': 'executable',
      'dependencies': [
	'rig',
      ],
      'defines': [
        '_ALL_SOURCE=1',
        '_GNU_SOURCE=1',
      ],
      'sources': [
	'toys/rig-hello.c'
      ],
      'ldflags': [
        '-export-dynamic'
      ]
    }

  ]
}
