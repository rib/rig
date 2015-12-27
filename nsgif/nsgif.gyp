{
  'targets': [
  {
    'target_name': 'nsgif',
    'type': 'shared_library',
    'sources': [
      'src/libnsgif.c'
    ],
    'include_dirs': [
      'include',
      'src',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        'include',
      ],
    },
    'defines': [
      '_BSD_SOURCE',
      '_DEFAULT_SOURCE',
    ],
    'conditions': [
      ['_type=="shared_library"', {
         'cflags': [ '-fPIC' ],
      }]
    ],
  }],
}
