

import resource

class Server(object):

    def __init__(self, url):
        self.url = url

    def get_resources(self):
        raise Exception("not implemented")

    resources = property(get_resources)

    def get_resource(self, name):
        return resource.Resource(self, name)
