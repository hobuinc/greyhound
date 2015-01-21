{
    'targets':
    [
        {
            'target_name': 'pdalBindings',
            'sources': [
                './addon/buffer-pool.cpp',
                './addon/compression-stream.cpp',
                './addon/grey-common.cpp',
                './addon/grey-reader.cpp',
                './addon/grey-writer.cpp',
                './addon/once.cpp',
                './addon/live-data-source.cpp',
                './addon/pdal-bindings.cpp',
                './addon/pdal-index.cpp',
                './addon/pdal-session.cpp',
                './addon/read-command.cpp',
                './addon/read-query.cpp',
                './addon/http/curl.cpp',
                './addon/http/s3.cpp'
            ],
            'include_dirs': ['./addon'],
            'cflags!':    [ '-fno-exceptions', '-fno-rtti' ],
            'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
            'cflags': [
                '-g',
                '-std=c++11',
                '-Wall',
                '-Werror',
                '-pedantic',
                '-fexceptions',
                '-frtti',
                '-I/usr/include/libxml2',
            ],
            'link_settings': {
                'libraries': [
                    '-lpdalcpp',
                    '-lboost_system',
                    '-lpthread',
                    '-lsqlite3',
                    '-lcurl',
                    '-lcrypto'
                ]
            }
        }
    ]
}

