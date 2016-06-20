
import numpy as np
import urllib2
import json
import box
import struct

class Resource(object):

    def __init__(self, server, name):
        self.server = server
        self.name = name
        self.info = self.get_info()

    def get_url(self):
        url = self.server.url+"resource/"+self.name
        return url

    url = property(get_url)

    def get_info(self):

        def buildNumpyDescription(schema):
            output = {}
            formats = []
            names = []
            for s in schema:
                t = s['type']
                if t == 'floating':
                    t = 'f'
                elif t == 'unsigned':
                    t = 'u'
                else:
                    t = 'i'

                f = '%s%d' % (t, int(s['size']))
                names.append(s['name'])
                formats.append(f)
            output['formats'] = formats
            output['names'] = names
            return output

        command = self.url + "/info"
        u = urllib2.urlopen(command)
        data = u.read()
        j = json.loads(data)
        j['dtype'] = buildNumpyDescription(j['schema'])

        return j


    def read(self, bounds, depthBegin, depthEnd, compress=False):

        command = self.url + '/read?'
        command += 'bounds=%s&depthEnd=%d&depthBegin=%d&compress=false' % (bounds.url, depthEnd, depthBegin)
        u = urllib2.urlopen(command)
        data = u.read()

        # last four bytes are the point count
        count = struct.unpack('<L',data[-4:])[0]
        array = np.ndarray(shape=(count,),buffer=data,dtype=self.info['dtype'])
        return array



