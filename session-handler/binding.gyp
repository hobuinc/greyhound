{
    'targets':
    [
        {
            'target_name': 'pdalBindings',
            'sources': [
                './addon/buffer-pool.cpp',
                './addon/compression-stream.cpp',
                './addon/once.cpp',
                './addon/pdal-bindings.cpp',
                './addon/pdal-index.cpp',
                './addon/pdal-session.cpp',
                './addon/read-query.cpp',

                './addon/commands/read.cpp',

                './addon/data-sources/arbiter.cpp',
                './addon/data-sources/base.cpp',
                './addon/data-sources/live.cpp',
                './addon/data-sources/multi.cpp',
                './addon/data-sources/serial.cpp',

                './addon/grey/reader.cpp',
                './addon/grey/reader-types.cpp',
                './addon/grey/writer.cpp',

                './addon/http/collector.cpp',
                './addon/http/curl.cpp',
                './addon/http/s3.cpp',

                './addon/third/jsoncpp.cpp',

                './addon/types/bbox.cpp',

                './addon/tree/node.cpp',
                './addon/tree/sleepy-tree.cpp'
            ],
            'include_dirs': [
                './addon',
                './addon/third',
                '/usr/include/libxml2'
            ],
            'cflags!':    [ '-fno-exceptions', '-fno-rtti' ],
            'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
            'cflags': [
                '-g',
                '-std=c++11',
                '-Wall',
                '-Werror',
                '-pedantic',
                '-fexceptions',
                '-frtti'
            ],
            'link_settings': {
                'libraries': [
                    '-lpdalcpp',
                    '-lpthread',
                    '-lsqlite3',
                    '-lcurl',
                    '-lcrypto'
                ]
            }
        }
    ]
}

