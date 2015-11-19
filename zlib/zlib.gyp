{
  'targets': [
  {
    'target_name': 'zlib',
    'type': 'shared_library',
    'sources': [
      'adler32.c',
      'compress.c',
      'crc32.c',
      'crc32.h',
      'deflate.c',
      'deflate.h',
      'gzclose.c',
      'gzguts.h',
      'gzlib.c',
      'gzread.c',
      'gzwrite.c',
      'infback.c',
      'inffast.c',
      'inffast.h',
      'inffixed.h',
      'inflate.c',
      'inflate.h',
      'inftrees.c',
      'inftrees.h',
      'mozzconf.h',
      'trees.c',
      'trees.h',
      'uncompr.c',
      'zconf.h',
      'zlib.h',
      'zutil.c',
      'zutil.h',
    ],
    'include_dirs': [
      'zlib',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
	'zlib',
      ],
    },
    'defines': [
      '_CRT_NONSTDC_NO_DEPRECATE',
    ],
  }],
}
