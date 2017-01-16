{
    'targets':
    [
        {
            'target_name': 'session',
            'sources': [
                './src/session/bindings.cpp',
                './src/session/session.cpp',

                './src/session/commands/read.cpp',
                './src/session/commands/hierarchy.cpp',

                './src/session/read-queries/base.cpp',
                './src/session/read-queries/entwine.cpp',

                './src/session/types/source-manager.cpp',

                './src/session/util/once.cpp'
            ],
            'include_dirs': [
                './src/session', '/usr/include/jsoncpp'
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

