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
      'target_name': 'libtest-fixtures',
      'type': 'static_library',
      'dependencies': [
         '../clib/clib.gyp:clib',
         '../cglib/cglib.gyp:cglib'
       ],
      'include_dirs': [
      ],
      'direct_dependent_settings': {
        'include_dirs': [ ],
      },
      'sources': [
	'test.h',
	'test-fixtures.h',
	'test-fixtures.c',
	'test-cg-fixtures.h',
	'test-cg-fixtures',
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
          'defines': [
            '_ALL_SOURCE=1',
            '_GNU_SOURCE=1',
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
#                'libraries': [ '-Wl,-soname,libtest-fixtures.so.1.0' ],
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
      ]
    }
  ]
}
