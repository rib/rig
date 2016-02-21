{
  'target_defaults': {
    'cflags': [
      '-std=c11',
      '-Wno-unused-but-set-variable',
      '-Wno-unused-function'
    ],
  },

  'targets': [
    {
      'target_name': 'libh2o',
      'type': 'static_library',
      'dependencies': [
          '../libuv/uv.gyp:libuv',
          '../wslay/wslay.gyp:libwslay'
       ],
      'include_dirs': [
        'include',
        'deps/klib',
        'deps/cloexec',
        'deps/picohttpparser',
        'deps/picotest',
        'deps/yoml'
      ],
      'direct_dependent_settings': {
        'include_dirs': [ 'rut' ],
      },
      'sources': [
        'deps/klib/kgraph.h',
        'deps/klib/kvec.h',
        'deps/klib/kurl.c',
        'deps/klib/ksa.c',
        'deps/klib/klist.h',
        'deps/klib/kmath.c',
        'deps/klib/kstring.h',
        'deps/klib/kson.h',
        'deps/klib/kthread.c',
        'deps/klib/kseq.h',
        'deps/klib/kurl.h',
        'deps/klib/knetfile.c',
        'deps/klib/kbit.h',
        'deps/klib/kstring.c',
        'deps/klib/khmm.h',
        'deps/klib/khmm.c',
        'deps/klib/bgzf.h',
        'deps/klib/knhx.h',
        'deps/klib/ksort.h',
        'deps/klib/kson.c',
        'deps/klib/ksw.h',
        'deps/klib/ksw.c',
        'deps/klib/knetfile.h',
        'deps/klib/bgzf.c',
        'deps/klib/kmath.h',
        'deps/klib/khash.h',
        'deps/klib/kopen.c',
        'deps/klib/kbtree.h',
        'deps/klib/knhx.c',

        'deps/picohttpparser/picohttpparser.c',
        'deps/picohttpparser/picohttpparser.h',

        'deps/picotest/picotest.c',
        'deps/picotest/picotest.h',

        'deps/cloexec/cloexec.h',
        'deps/cloexec/cloexec.c',

        'deps/yoml/yoml-parser.h',
        'deps/yoml/yoml.h',

        'lib/handler/file/templates.c.h',
        'lib/handler/file/_templates.c.h',
        'lib/handler/mimemap.c',
        'lib/handler/mimemap/mimemap.c.h',
        'lib/handler/chunked.c',
        'lib/handler/handler_headers.c',
        'lib/handler/redirect.c',
        'lib/handler/reproxy.c',
        'lib/handler/file.c',
        'lib/handler/handler_proxy.c',
        'lib/handler/access_log.c',
        'lib/handler/configurator_redirect.c',
        'lib/handler/configurator_file.c',
        'lib/handler/configurator_headers.c',
        'lib/handler/configurator_proxy.c',
        'lib/handler/configurator_reproxy.c',
        'lib/handler/configurator_access_log.c',
        'lib/handler/configurator_expires.c',
        'lib/handler/expires.c',
        'lib/common/hostinfo.c',
        'lib/common/time.c',
        'lib/common/serverutil.c',
        'lib/common/http1client.c',
        'lib/common/socket/uv-binding.c.h',
        'lib/common/socket/evloop.c.h',
        'lib/common/socket/evloop/select.c.h',
        'lib/common/socket/evloop/kqueue.c.h',
        'lib/common/socket/evloop/epoll.c.h',
        'lib/common/socket.c',
        'lib/common/string.c',
        'lib/common/socketpool.c',
        'lib/common/url.c',
        'lib/common/timeout.c',
        'lib/common/memory.c',
        'lib/common/multithread.c',
        'lib/core/token_table.h',
        'lib/core/configurator.c',
        'lib/core/util.c',
        'lib/core/config.c',
        'lib/core/context.c',
        'lib/core/headers.c',
        'lib/core/proxy.c',
        'lib/core/token.c',
        'lib/core/request.c',
        'lib/websocket.c',
        'lib/http1.c',
        'lib/http2/frame.c',
        'lib/http2/hpack.c',
        'lib/http2/stream.c',
        'lib/http2/hpack_static_table.h',
        'lib/http2/scheduler.c',
        'lib/http2/hpack_huffman_table.h',
        'lib/http2/connection.c',

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
