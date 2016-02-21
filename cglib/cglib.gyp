{
  'target_defaults': {
    'cflags': [
      '-std=c11',
    ],
  },

  'targets': [
    {
      'target_name': 'cglib',
      'type': 'static_library',
      'dependencies': [
         '../clib/clib.gyp:clib'
       ],
      'include_dirs': [
        '.',
        'cglib',
        'cglib/sfmt',
	'cglib/winsys',
	'cglib/driver/nop',
	'cglib/driver/gl',
	'cglib/driver/gl/gl',
	'cglib/driver/gl/gles'
      ],
      'all_dependent_settings': {
        'include_dirs': [ '.' ],
      },
      'sources': [
        'cglib-config.h',

        'cglib/cglib.h',
        'cglib/cglib.c',

        'cglib/cg-bitmask.c',

        'cglib/cg-display.h',
        'cglib/cg-display-private.h',
        'cglib/cg-display.c',

        'cglib/cg-renderer.h',
        'cglib/cg-renderer-private.h',
        'cglib/cg-renderer.c',

        'cglib/cg-pipeline.h',
        'cglib/cg-pipeline-private.h',
        'cglib/cg-pipeline.c',
        'cglib/cg-pipeline-state.h',
        'cglib/cg-pipeline-state-private.h',
        'cglib/cg-pipeline-state.c',
        'cglib/cg-pipeline-hash-table.h',
        'cglib/cg-pipeline-hash-table.c',
        'cglib/cg-pipeline-cache.h',
        'cglib/cg-pipeline-cache.c',
        'cglib/cg-pipeline-layer-private.h',
        'cglib/cg-pipeline-layer.c',
        'cglib/cg-pipeline-layer-state-private.h',
        'cglib/cg-pipeline-layer-state.h',
        'cglib/cg-pipeline-layer-state.c',
        'cglib/cg-pipeline-snippet-private.h',
        'cglib/cg-pipeline-snippet.c',
        'cglib/cg-pipeline-debug.c',

        'cglib/cg-attribute-buffer.c',
        'cglib/cg-output.h',
        'cglib/cg-pixel-format-private.h',
        'cglib/cg-rectangle-map.c',
        'cglib/cg-util.c',
        'cglib/cg-frame-info-private.h',
        'cglib/cg-point-in-poly-private.h',
        'cglib/cg-object.c',
        'cglib/cg-atlas-texture.h',
        'cglib/cg-matrix-stack.c',
        'cglib/cg-primitive-private.h',

        'cglib/cg-feature-private.c',
        'cglib/cg-gpu-info-private.h',
        'cglib/cg-snippet-private.h',
        'cglib/cg-framebuffer.c',
        'cglib/cg-primitive-texture.h',

        'cglib/cg-object.h',
        'cglib/cg-bitmask.h',
        'cglib/cg-driver.h',
        'cglib/cg-texture-2d.c',
        'cglib/cg-texture-2d.h',
        'cglib/cg-debug.c',
        'cglib/cg-framebuffer-private.h',
        'cglib/cg-blend-string.c',
        'cglib/cg-indices-private.h',
        'cglib/cg-spans.h',
        'cglib/cg-onscreen-template.c',
        'cglib/cg-snippet.h',
        'cglib/cg-fence.c',
        'cglib/cg-memory-stack.c',

        'cglib/cg-gles2-context-private.h',
        'cglib/cg-point-in-poly.c',

        'cglib/cg-blit.h',
        'cglib/cg-blit.c',
        'cglib/cg-onscreen-private.h',
        'cglib/cg-indices.h',
        'cglib/cg-primitive.h',
        'cglib/cg-flags.h',
        'cglib/cg-clip-stack.h',
        'cglib/cg-onscreen.h',
        'cglib/cg-framebuffer.h',
        'cglib/cg-atlas.h',
        'cglib/cg-frame-info.c',
        'cglib/cg-object-private.h',
        'cglib/cg-matrix-stack.h',
        'cglib/cg-atlas-set-private.h',
        'cglib/cg-private.h',
        'cglib/cg-index-buffer.c',
        'cglib/cg-sampler-cache-private.h',
        'cglib/cg-device.h',

        'cglib/cg-glsl-shader.c',

        'cglib/cg-buffer-private.h',
        'cglib/cg-texture-2d-sliced.c',
        'cglib/cg-texture-2d-sliced.h',
        'cglib/cg-depth-state.c',

        'cglib/cg-bitmap-private.h',
        'cglib/cg-bitmap.h',
        'cglib/cg-bitmap.c',
        'cglib/cg-bitmap-pack.h',
        'cglib/cg-bitmap-unpack-unsigned-normalized.h',
        'cglib/cg-bitmap-unpack-fallback.h',
        'cglib/cg-bitmap-conversion.c',
        'cglib/cg-bitmap-pixbuf.c',

        'cglib/cg-error.h',
        'cglib/cg-atlas-set.h',
        'cglib/cg-texture-3d.h',
        'cglib/cg-color.c',
        'cglib/cg-atlas-texture.c',
        'cglib/cg-texture.h',
        'cglib/cg-spans.c',
        'cglib/cg-loop.c',
        'cglib/cg-primitive-texture.c',
        'cglib/cg-pixel-buffer.c',
        'cglib/cg-i18n-private.h',
        'cglib/cg-config-private.h',
        'cglib/cg-buffer.c',
        'cglib/cg-attribute-buffer.h',
        'cglib/cg-types.h',
        'cglib/cg-memory-stack-private.h',
        'cglib/cg-color.h',
        'cglib/cg-glsl-shader-boilerplate.h',
        'cglib/cg-loop-private.h',
        'cglib/cg-texture-3d.c',
        'cglib/cg-debug.h',
        'cglib/cg-version.h',
        'cglib/cg-fence-private.h',
        'cglib/cg-offscreen.h',
        'cglib/cg-closure-list-private.h',
        'cglib/cg-error-private.h',
        'cglib/cg-gpu-info.c',
        'cglib/cg-matrix-stack-private.h',
        'cglib/cg-index-buffer-private.h',
        'cglib/cg-magazine-private.h',
        'cglib/cg-node-private.h',
        'cglib/cg-atlas-set.c',
        'cglib/cg-attribute-private.h',
        'cglib/cg-blend-string.h',
        'cglib/cg-attribute.h',
        'cglib/cg-pixel-format.c',
        'cglib/cg-texture-2d-sliced-private.h',
        'cglib/cg-loop.h',
        'cglib/cg-texture-private.h',
        'cglib/cg-fence.h',
        'cglib/cg-depth-state.h',
        'cglib/cg-attribute.c',
        'cglib/cg-onscreen-template-private.h',
        'cglib/cg-texture.c',
        'cglib/cg-defines.h',
        'cglib/cg-pixel-buffer-private.h',
        'cglib/cg-index-buffer.h',
        'cglib/cg-glsl-shader-private.h',
        'cglib/cg-sub-texture.c',
        'cglib/cg-sampler-cache.c',
        'cglib/cg-texture-2d-gl.h',
        'cglib/cg-gles2-types.h',
        'cglib/cg-magazine.c',
        'cglib/cg-rectangle-map.h',
        'cglib/cg-color-private.h',
        'cglib/cg-device.c',
        'cglib/cg-snippet.c',
        'cglib/cg-profile.c',
        'cglib/cg-texture-3d-private.h',
        'cglib/cg-texture-driver.h',
        'cglib/cg-atlas-texture-private.h',
        'cglib/cg-util.h',
        'cglib/cg-atlas-private.h',
        'cglib/cg-error.c',
        'cglib/cg-onscreen.c',
        'cglib/cg-meta-texture.h',
        'cglib/cg-primitive.c',
        'cglib/cg-meta-texture.c',
        'cglib/cg-debug-options.h',
        'cglib/cg-indices.c',
        'cglib/cg-onscreen-template.h',
        'cglib/cg-clip-stack.c',
        'cglib/cg-depth-state-private.h',
        'cglib/cg-boxed-value.c',
        'cglib/cg-buffer.h',
        'cglib/cg-closure-list.c',
        'cglib/cg-atlas.c',
        'cglib/cg-frame-info.h',
        'cglib/cg-output-private.h',
        'cglib/cg-profile.h',
        'cglib/cg-texture-2d-private.h',
        'cglib/cg-attribute-buffer-private.h',
        'cglib/cg-config.c',
        'cglib/cg-sub-texture-private.h',
        'cglib/cg-boxed-value.h',
        'cglib/cg-output.c',
        'cglib/cg-device-private.h',
        'cglib/cg-node.c',
        'cglib/cg-pixel-buffer.h',
        'cglib/cg-sub-texture.h',
        'cglib/cg-feature-private.h',

        'cglib/driver/nop/cg-attribute-nop.c',
        'cglib/driver/nop/cg-pipeline-vertend-nop-private.h',
        'cglib/driver/nop/cg-framebuffer-nop-private.h',
        'cglib/driver/nop/cg-pipeline-progend-nop-private.h',
        'cglib/driver/nop/cg-attribute-nop-private.h',
        'cglib/driver/nop/cg-pipeline-fragend-nop-private.h',
        'cglib/driver/nop/cg-pipeline-vertend-nop.c',
        'cglib/driver/nop/cg-texture-2d-nop.c',
        'cglib/driver/nop/cg-texture-2d-nop-private.h',
        'cglib/driver/nop/cg-clip-stack-nop.c',
        'cglib/driver/nop/cg-pipeline-fragend-nop.c',
        'cglib/driver/nop/cg-pipeline-progend-nop.c',
        'cglib/driver/nop/cg-driver-nop.c',
        'cglib/driver/nop/cg-clip-stack-nop-private.h',
        'cglib/driver/nop/cg-framebuffer-nop.c',

        'cglib/cg-gles2.h',
        'cglib/cg-gl-header.h',

        'cglib/cg-gles2-context.c',

        'cglib/gl-prototypes/cg-all-functions.h',
        'cglib/gl-prototypes/cg-glsl-functions.h',
        'cglib/gl-prototypes/cg-fixed-functions.h',
        'cglib/gl-prototypes/cg-in-gles2-core-functions.h',
        'cglib/gl-prototypes/cg-gles2-functions.h',
        'cglib/gl-prototypes/cg-core-functions.h',

        'cglib/driver/gl/cg-attribute-gl-private.h',
        'cglib/driver/gl/cg-clip-stack-gl.c',
        'cglib/driver/gl/cg-pipeline-opengl.c',
        'cglib/driver/gl/cg-pipeline-vertend-glsl.c',
        'cglib/driver/gl/cg-texture-2d-gl.c',
        'cglib/driver/gl/cg-pipeline-fragend-glsl.c',
        'cglib/driver/gl/cg-texture-2d-gl-private.h',
        'cglib/driver/gl/cg-pipeline-progend-glsl-private.h',
        'cglib/driver/gl/cg-texture-gl-private.h',
        'cglib/driver/gl/cg-pipeline-vertend-glsl-private.h',
        'cglib/driver/gl/cg-framebuffer-gl.c',
        'cglib/driver/gl/cg-pipeline-progend-glsl.c',
        'cglib/driver/gl/cg-buffer-gl.c',
        'cglib/driver/gl/cg-clip-stack-gl-private.h',
        'cglib/driver/gl/cg-attribute-gl.c',
        'cglib/driver/gl/cg-util-gl.c',
        'cglib/driver/gl/cg-util-gl-private.h',
        'cglib/driver/gl/cg-pipeline-fragend-glsl-private.h',
        'cglib/driver/gl/cg-buffer-gl-private.h',
        'cglib/driver/gl/cg-pipeline-opengl-private.h',
        'cglib/driver/gl/cg-framebuffer-gl-private.h',
        'cglib/driver/gl/cg-texture-gl.c',

        'cglib/driver/gl/gl/cg-texture-driver-gl.c',
        'cglib/driver/gl/gl/cg-driver-gl.c',

        'cglib/driver/gl/gles/cg-driver-gles.c',
        'cglib/driver/gl/gles/cg-texture-driver-gles.c',

        'cglib/winsys/cg-winsys-private.h',
        'cglib/winsys/cg-winsys-stub-private.h',
        'cglib/winsys/cg-winsys-stub.c',
        'cglib/winsys/cg-winsys.c',
      ],
      'defines': [
        'CG_COMPILATION',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'cglib/cg-win32-renderer.h',
            'cglib/cg-win32-renderer.c',
            'cglib/winsys/cg-winsys-wgl-feature-functions.h',
            'cglib/winsys/cg-winsys-wgl-private.h',
            'cglib/winsys/cg-winsys-wgl.c',
          ],
        }, { # Not Windows i.e. POSIX
          'cflags': [
            '-g',
            '-std=c99',
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
          ],
        }],
        [ 'OS=="emscripten"', {
          'sources': [
            'cglib/cg-webgl-private.h',
            'cglib/cg-webgl.h',
            'cglib/cg-webgl.c',
            'cglib/cg-webgl-renderer.c',
            'cglib/cg-emscripten-lib.c',
          ],
        }],
        [ 'OS=="android"', {
          'sources': [
            'cglib/winsys/cg-winsys-egl-android-private.h',
            'cglib/winsys/cg-winsys-egl-android.c',
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
            '../libuv/uv.gyp:libuv'
          ],
          'defines': [
            'USE_UV=1'
          ],
          'sources': [
            'cglib/cg-uv-private.h',
            'cglib/cg-uv.h',
            'cglib/cg-uv.c',
          ]
        }],
        [ 'enable_glib==1', {
          'sources': [
            'cglib/cg-glib-source.h',
            'cglib/cg-glib-source.c',
            'cglib/cg-gtype-private.h',
          ]
        }],
        [ 'enable_sdl==1', {
          'sources': [
            'cglib/cg-sdl.h',
            'cglib/cg-sdl.c',
            'cglib/winsys/cg-winsys-sdl2.c',
            'cglib/winsys/cg-winsys-sdl-private.h',
          ]
        }],
        [ 'enable_glx==1', {
          'sources': [
            'cglib/cg-glx-display-private.h',
            'cglib/cg-glx-renderer-private.h',
            'cglib/winsys/cg-winsys-glx-private.h',
            'cglib/winsys/cg-winsys-glx-feature-functions.h',
            'cglib/winsys/cg-winsys-glx.c',
          ]
        }],
        [ 'enable_egl==1', {
          'sources': [
            'cglib/cg-egl.h',
            'cglib/cg-egl-defines.h',
            'cglib/winsys/cg-winsys-egl.c',
            'cglib/winsys/cg-winsys-egl-private.h',
            'cglib/winsys/cg-winsys-egl-feature-functions.h',
            'cglib/winsys/cg-winsys-egl-null.c',
            'cglib/winsys/cg-winsys-egl-null-private.h',
          ]
        }],
        [ 'enable_egl_kms==1', {
          'sources': [
            'cglib/cg-kms-display.h',
            'cglib/cg-kms-renderer.h',
            'cglib/winsys/cg-winsys-egl-kms-private.h',
            'cglib/winsys/cg-winsys-egl-kms.c',
          ]
        }],
        [ 'enable_egl_wayland==1', {
          'sources': [
            'cglib/cg-wayland-server.h',
            'cglib/cg-wayland-renderer.h',
            'cglib/cg-wayland-client.h',
            'cglib/winsys/cg-winsys-egl-wayland-private.h',
            'cglib/winsys/cg-winsys-egl-wayland.c',
          ]
        }],
        [ 'enable_egl_xlib==1', {
          'sources': [
            'cglib/winsys/cg-winsys-egl-x11-private.h',
            'cglib/winsys/cg-winsys-egl-x11.c',
          ]
        }],
        [ 'enable_x11==1', {
          'sources': [
            'cglib/cg-xlib.h',
            'cglib/cg-xlib-renderer.h',
            'cglib/cg-xlib-renderer.c',
            'cglib/cg-x11-renderer-private.h',
            'cglib/cg-xlib-renderer-private.h',
            'cglib/winsys/cg-texture-pixmap-x11-private.h',
            'cglib/winsys/cg-texture-pixmap-x11.h',
            'cglib/winsys/cg-texture-pixmap-x11.c',
          ]
        }],
      ]
    },
  ]
}
