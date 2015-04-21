{
    'targets':
    [
        {
            'target_name': 'session',
            'sources': [
                './addon/bindings.cpp',
                './addon/session.cpp',

                './addon/commands/read.cpp',

                './addon/read-queries/base.cpp',
                './addon/read-queries/entwine.cpp',
                './addon/read-queries/unindexed.cpp',

                './addon/types/source-manager.cpp',

                './addon/util/buffer-pool.cpp',
                './addon/util/once.cpp'
            ],
            'include_dirs': [
                './addon',
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
                    '-lentwine',
                    '-lpthread'
                ]
            }
        }
    ]
}

