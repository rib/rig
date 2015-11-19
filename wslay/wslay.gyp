{
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
      'conditions': [
        [ 'OS!="win"', { # i.e. POSIX
          'cflags': [
            '-std=c99',
          ],
          'link_settings': {
            'libraries': [ '-lm' ],
            'conditions': [
              ['OS != "android"', {
                'ldflags': [ '-pthread' ],
              }],
            ],
          },
        }],
      ]
    }
  ]
}
