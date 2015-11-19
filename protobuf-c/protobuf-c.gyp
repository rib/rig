{
  'target_defaults': {

    'conditions': [
      [ 'OS!="win"', { #i.e. POSIX
        'ddefines': [
          '_ALL_SOURCE=1',
          '_GNU_SOURCE=1',
          'STDC_HEADERS=1',
          'HAVE_ALLOCA_H=1',
          'HAVE_MALLOC_H=1',
          'HAVE_SYS_POLL_H=1',
          'HAVE_SYS_SELECT_H=1',
          'HAVE_SYS_UIO_H=1',
          'HAVE_UNISTD_H=1',
        ],
      }],
    ]
  },
  'targets': [
    {
      'target_name': 'protobuf-c',
      'type': 'static_library',
      'toolsets': [ 'host', 'target' ],
      'dependencies': [
       ],
      'include_dirs': [
        '.',
        'protobuf-c'
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '.',
          'protobuf-c'
        ],
      },
      'conditions': [
        ['_type=="shared_library"', {
          'cflags': [ '-fPIC' ],
        }],
      ],
      'sources': [
	'protobuf-c/protobuf-c.c',
	'protobuf-c/protobuf-c.h',
	'protobuf-c/protobuf-c-private.h',
      ],
    },
    {
      'target_name': 'protoc-c',
      'type': 'executable',
      'toolsets': [ 'host' ],
      'dependencies': [
        'protobuf-c'
      ],
      'include_dirs': [
        '.',
        'protobuf-c'
      ],
      'defines': [
        'PACKAGE_STRING="protobufc-c"',
      ],
      'sources': [
	'protoc-c/c_bytes_field.cc',
	'protoc-c/c_bytes_field.h',
	'protoc-c/c_enum.cc',
	'protoc-c/c_enum.h',
	'protoc-c/c_enum_field.cc',
	'protoc-c/c_enum_field.h',
	'protoc-c/c_extension.cc',
	'protoc-c/c_extension.h',
	'protoc-c/c_field.cc',
	'protoc-c/c_field.h',
	'protoc-c/c_file.cc',
	'protoc-c/c_file.h',
	'protoc-c/c_generator.cc',
	'protoc-c/c_generator.h',
	'protoc-c/c_helpers.cc',
	'protoc-c/c_helpers.h',
	'protoc-c/c_message.cc',
	'protoc-c/c_message.h',
	'protoc-c/c_message_field.cc',
	'protoc-c/c_message_field.h',
	'protoc-c/c_primitive_field.cc',
	'protoc-c/c_primitive_field.h',
	'protoc-c/c_service.cc',
	'protoc-c/c_service.h',
	'protoc-c/c_string_field.cc',
	'protoc-c/c_string_field.h',
	'protoc-c/main.cc',
      ],
      'libraries': [
        '-lprotobuf',
        '-lprotoc',
        '-lstdc++',
        '-lz'
      ],
    }
  ]
}
