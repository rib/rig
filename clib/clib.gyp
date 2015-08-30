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
      'target_name': 'libclib',
      'type': '<(clib_library)',
      'include_dirs': [
        '.',
        'clib',
        'clib/sfmt',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'clib' ],
        'conditions': [
          ['OS != "win"', {
            'defines': [
              '_LARGEFILE_SOURCE',
              '_FILE_OFFSET_BITS=64',
            ],
          }],
          ['OS == "mac"', {
            'defines': [ '_DARWIN_USE_64_BIT_INODE=1' ],
          }],
          ['OS == "linux"', {
            'defines': [ '_POSIX_C_SOURCE=200112' ],
          }],
        ],
      },
      'sources': [
	'clib-config.h',
        'clib/ascii_snprintf.c',
        'clib/carray.c',
        'clib/cbytearray.c',
        'clib/cdebugkey.c',
        'clib/cerror.c',
        'clib/ceuler.c',
        'clib/ceuler.h',
        'clib/cfile.c',
        'clib/chashtable.c',
        'clib/ciconv.c',
        'clib/clib-platform.h',
        'clib/clib.h',
        'clib/clist.c',
        'clib/cllist.c',
        'clib/cmatrix.c',
        'clib/cmatrix.h',
        'clib/cmem.c',
        'clib/cmodule.c',
        'clib/cmodule.h',
        'clib/coutput.c',
        'clib/cpath.c',
        'clib/cptrarray.c',
        'clib/cqsort.c',
        'clib/cquark.c',
        'clib/cquaternion-private.h',
        'clib/cquaternion.c',
        'clib/cquaternion.h',
        'clib/cqueue.c',
        'clib/crand.c',
        'clib/crbtree.c',
        'clib/crbtree.h',
        'clib/cshell.c',
        'clib/cslist.c',
        'clib/cspawn.c',
        'clib/cstr.c',
        'clib/cstring.c',
        'clib/ctimer.c',
        'clib/cunicode.c',
        'clib/cutf8.c',
        'clib/cvector.c',
        'clib/cvector.h',
        'clib/sort.frag.h',
        'clib/unicode-data.h',
        'clib/sfmt/SFMT.h',
        'clib/sfmt/SFMT.c',
        'clib/sfmt/SFMT-common.h',
        'clib/sfmt/SFMT-params607.h',
        'clib/sfmt/SFMT-params1279.h',
        'clib/sfmt/SFMT-params2281.h',
        'clib/sfmt/SFMT-params4253.h',
        'clib/sfmt/SFMT-params11213.h',
        'clib/sfmt/SFMT-params19937.h',
        'clib/sfmt/SFMT-params44497.h',
        'clib/sfmt/SFMT-params86243.h',
        'clib/sfmt/SFMT-params216091.h',
        'clib/sfmt/SFMT-params.h',
      ],
      'defines': [
        'SFMT_MEXP=19937',
        '_C_COMPILATION',
        'ENABLE_UNIT_TESTS'
      ],
      'conditions': [
        [ 'OS=="win"', {
          'defines': [
            '_WIN32_WINNT=0x0600',
            '_GNU_SOURCE',
          ],
          'sources': [
            'clib/cdate-win32.c',
            'clib/cdir-win32.c',
            'clib/cmisc-win32.c',
            'clib/ctls-win32.cc',
            'clib/mkstemp.c',
            'clib/vasprintf.c',
            'clib/fmemopen.c',
          ],
          'link_settings': {
            'libraries': [
              '-ladvapi32',
              '-liphlpapi',
              '-lpsapi',
              '-lshell32',
              '-lws2_32'
            ],
          },
        }, { # Not Windows i.e. POSIX
          'cflags': [
            '-g',
            '-std=c99',
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter',
          ],
          'sources': [
            'clib/cdate-unix.c',
            'clib/cdir-unix.c',
            'clib/cmisc-unix.c',
            'clib/cxdg-unix.c',
	    'clib/ctls.c',
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
            ['clib_library=="shared_library"', {
              'cflags': [ '-fPIC' ],
            }],
            ['clib_library=="shared_library" and OS!="mac"', {
              'link_settings': {
                'libraries': [ '-Wl,-soname,libclib.so.1.0' ],
              },
            }],
          ],
        }],
        [ 'OS=="emscripten"', {
          'sources': [
            'clib/clib-web.h',
            'clib/cmisc-emscripten.c'
            'clib/cfile-emscripten.c',
          ],
        }],
        [ 'OS=="mac"', {
          'defines': [
            '_DARWIN_USE_64_BIT_INODE=1'
          ],
          'sources': [
            'clib/fmemopen.c',
	  ]
        }],
        [ 'OS!="mac"', {
          # Enable on all platforms except OS X. The antique gcc/clang that
          # ships with Xcode emits waaaay too many false positives.
          'cflags': [ '-Wstrict-aliasing' ],
        }],
        [ 'OS=="linux"', {
          'link_settings': {
            'libraries': [ '-ldl', '-lrt' ],
          },
        }],
        [ 'OS=="android"', {
          'link_settings': {
            'libraries': [ '-ldl' ],
          },
          'sources': [
            'clib/vasprintf.c',
          ]
        }],
        ['clib_library=="shared_library"', {
          'defines': [ 'BUILDING_CLIB_SHARED=1' ]
        }],
      ]
    },
  ]
}
