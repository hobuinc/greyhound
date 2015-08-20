{
    'targets':
    [
        {
            'target_name': 'session',
            'sources': [
                './session/bindings.cpp',
                './session/session.cpp',

                './session/commands/read.cpp',

                './session/read-queries/base.cpp',
                './session/read-queries/entwine.cpp',
                './session/read-queries/unindexed.cpp',

                './session/types/source-manager.cpp',

                './session/util/buffer-pool.cpp',
                './session/util/once.cpp'
            ],
            'include_dirs': [
                './session'
            ],
            'cflags!':    [ '-fno-exceptions', '-fno-rtti' ],
            'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
            'cflags': [
                '-g',
                '-std=c++11',
                '-Wall',
                '-Werror',
                '-pedantic',
                '-pthread',
                '-fexceptions',
                '-frtti'
            ],
            'link_settings': {
                'libraries': [
                    '-lpdalcpp',
                    '-lentwine',
                    '-pthread'
                ]
            }
        }
    ]
}

