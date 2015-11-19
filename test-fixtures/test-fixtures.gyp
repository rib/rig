{
  'target_defaults': {
    'cflags': [
      '-std=c11',
    ],
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
        [ 'OS!="win"', { # i.e POSIX
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
          ],
        }],
      ]
    }
  ]
}
