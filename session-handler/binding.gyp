{
    'targets':
    [
        {
            'target_name': 'pdalBindings',
            'sources': [
                './addon/pdal-session.cpp',
                './addon/pdal-bindings.cpp',
                './addon/read-command.cpp',
                './addon/once.cpp',
                './addon/pdal-index.cpp',
                './addon/live-data-source.cpp',
                './addon/serial-data-source.cpp'
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
                ]
            }
        }
    ]
}

