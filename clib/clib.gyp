{
  'variables': {
    'enable_uv%': 0,
  },

  'target_defaults': {
    'cflags': [
      '-std=c11',
    ],
  },

  'targets': [
    {
      'target_name': 'clib',
      'type': 'static_library',
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
      'all_dependent_settings': {
        'include_dirs': [ 'clib' ],
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
        'clib/cparse-url.c',
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
      ],
      'conditions': [
        [ 'enable_uv==1 and is_nodejs_build!=1', {
          'dependencies': [
            '../libuv/uv.gyp:libuv'
          ],
        }],
        [ 'enable_uv==1', {
          'defines': [
            'USE_UV=1'
          ],
          'sources': [
            'clib/cmodule.c',
            'clib/cmodule.h',
          ]
        }],
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
            'clib/clib-web.h',
            'clib/cmisc-emscripten.c',
          ],
        }],
        [ 'OS=="mac"', {
          'sources': [
            'clib/cdir-unix.c',
            'clib/cdate-unix.c',
            'clib/cmisc-unix.c',
            'clib/ctls.c',
            'clib/fmemopen.c',
          ]
        }],
        [ 'OS=="linux"', {
          'link_settings': {
            'libraries': [ '-ldl', '-lrt' ],
          },
          'sources': [
            'clib/cdir-unix.c',
            'clib/cdate-unix.c',
            'clib/cmisc-unix.c',
            'clib/cxdg-unix.c',
            'clib/ctls.c',
	    'clib/cbacktrace-linux.c',
          ]
        }],
        [ 'OS!="linux"', {
          'sources': [
	    'clib/cbacktrace.c',
	  ]
	}],
        [ 'OS=="android"', {
          'link_settings': {
            'libraries': [ '-ldl' ],
          },
          'sources': [
            'clib/cdir-unix.c',
            'clib/cdate-unix.c',
            'clib/cxdg-unix.c',
            'clib/ctls.c',
            'clib/vasprintf.c',
          ]
        }],
        ['_type=="shared_library"', {
          'defines': [ 'BUILDING_CLIB_SHARED=1' ]
        }],
      ]
    },
  ]
}
