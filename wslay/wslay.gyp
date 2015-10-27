{
  "includes": [
    "common.gypi"
  ],

  'target_defaults': {
    'cflags': [
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
      'target_name': 'libwslay',
      'type': 'static_library',
      'dependencies': [
       ],
      'include_dirs': [
          'lib/includes'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
            'lib/includes',
            'lib',
        ],
      },
      'sources': [
        'lib/wslay_net.c',
        'lib/wslay_event.h',
        'lib/wslay_queue.c',
        'lib/wslay_queue.h',
        'lib/include/wslay/wslay.h',
        'lib/wslay_stack.c',
        'lib/wslay_event.c',
        'lib/wslay_net.h',
        'lib/wslay_frame.h',
        'lib/wslayver.h',
        'lib/wslay_frame.c',
        'lib/wslay_stack.h'
      ],
      'defines': [
        'ENABLE_UNIT_TESTS'
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
            }],
#            ['_type=="shared_library" and OS!="mac"', {
#              'link_settings': {
#                'libraries': [ '-Wl,-soname,libwslay.so.1.0' ],
#              },
#            }],
          ],
        }],
        [ 'OS=="emscripten"', {
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
      ]
    },
  ]
}
