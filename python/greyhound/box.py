import json
import sys

class Box(object):


    @staticmethod
    def from_point(x, y, radius = 1000):

       minx = x - radius; miny = y - radius;
       maxx = x + radius; maxy = y + radius;
       maxz = radius;
       minz = -radius;
       return Box(minx, miny, maxx, maxy, minz, maxz)

    def __init__(self, minx, miny, maxx, maxy, minz=sys.float_info.max, maxz=sys.float_info.min):
        self.minx = minx
        self.maxx = maxx
        self.miny = miny
        self.maxy = maxy
        self.minz = minz
        self.maxz = maxz

    def get_url(self):
        box = '['+ ','.join([str(i) for i in [self.minx, self.miny, self.minz, self.maxx, self.maxy, self.maxz]]) + ']'
        return box


    url = property(get_url)
