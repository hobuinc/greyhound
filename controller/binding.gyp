{
    'targets':
    [
        {
            'target_name': 'session',
            'sources': [
                './session/bindings.cpp',
                './session/session.cpp',

                './session/commands/read.cpp',
                './session/commands/hierarchy.cpp',

                './session/read-queries/base.cpp',
                './session/read-queries/entwine.cpp',
                './session/read-queries/unindexed.cpp',

                './session/types/source-manager.cpp',

                './session/util/buffer-pool.cpp',
                './session/util/once.cpp'
            ],
            'include_dirs': [
                './session', '/usr/include/jsoncpp'
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
            "conditions": [
                [ 'OS=="mac"', {
                    "xcode_settings": {
                        "OTHER_CPLUSPLUSFLAGS" : [
                            "-std=c++11",
                            "-stdlib=libc++",
                            "-frtti",
                            "-fexceptions"
                        ],
                        "OTHER_LDFLAGS": [ "-stdlib=libc++" ],
                        "MACOSX_DEPLOYMENT_TARGET": "10.7"
                    },
                    'cflags!':    [ '-fno-exceptions', '-fno-rtti' ],
                    'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
                    'cflags': [ '-frtti', '-fexceptions' ]
                }]
            ],
            'link_settings': {
                'libraries': [
                    '-lpdalcpp',
                    '-lentwine',
                    '-pthread',
                    '-ljsoncpp'
                ]
            }
        }
    ]
}

