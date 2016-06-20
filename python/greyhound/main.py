import greyhound

def entry():
    import argparse

    parser = argparse.ArgumentParser(description='')
    parser.add_argument('query', nargs='+')
    parser.add_argument('count', nargs="?", default=2)

    args = parser.parse_args()


    url='http://data.greyhound.io/'

    s = greyhound.server.Server(url)
    r = s.get_resource('mn-h')

    x = -10375539.03
    y = 6210523.43
    b = greyhound.box.Box.from_point(x,y,1000)
    data = r.read(b, 0, 16)

    greyhound.util.writeLASfile(data, 'somefile.las')


