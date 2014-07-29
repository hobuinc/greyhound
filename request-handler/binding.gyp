{
    'targets':
    [
        {
            'target_name': 'pdalSession',
            'sources': [ './addon/pdal-session.cpp' ],
            'include_dirs': ['./addon'],
            'cflags!':    [ '-fno-exceptions', '-fno-rtti' ],
            'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
            'cflags':
            [
                '-g',
                '-std=c++11',
                '-Wall',
                '-Werror',
                '-pedantic',
                '-fexceptions',
                '-frtti',
            ],
            'link_settings': {
                'libraries': [
                    '-lpdalcpp',
                    '-lboost_system',
                    '-lpthread',
                ]
            }
        }
    ]
}

