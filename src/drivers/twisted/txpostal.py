#!/usr/bin/env python

#
# Copyright (C) 2012 Catch.com
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

"""
Twisted Python module for Postal Push Notification Server.
"""

import json
from twisted.application import service
from twisted.internet import defer
from twisted.web import client
import types

class Service(service.Service):
    name = 'PostalService'

    def __init__(self, host='localhost', port=5300):
        assert isinstance(host, basestring)
        assert isinstance(port, int)
        self.host = host
        self.port = port
        self.urlBase = 'http://%s:%d' % (host, port)

    def addDevice(self, user, device):
        """
        Asynchronously creates a device in Postal.

        @param user: a stringable value representing the user id.
        @type user: C{basestring} or coercable to string.

        @param device: A dictionary containing the device parameters.
        @type device: C{dict}

        Returns: L{defer.Deferred}
        """
        assert isinstance(device, types.DictType)
        url = '/'.join([self.urlBase,
                        'v1/users',
                        str(user),
                        'devices',
                        str(device['device_token'])])
        postdata = json.dumps(device)
        headers = {'Content-Type': 'application/json'}
        df = client.getPage(url,
                            method='PUT',
                            postdata=postdata,
                            agent='txpostal',
                            timeout=60,
                            headers=headers)
        df.addCallback(lambda d: json.loads(d))
        return df

    def removeDevice(self, user, device_or_token):
        """
        Asynchronously remove a device in Postal.

        @param user: a stringable value representing the user id.
        @type user: C{basestring}

        @param device_or_token: a string containing the device id.
        @type device_or_token: C{basestring}
        """
        if isinstance(device_or_token, dict):
            device_or_token = device_or_token.get('device_token')
        assert isinstance(user, basestring)
        assert isinstance(device_or_token, basestring)
        user = user.encode('ascii')
        url = '/'.join([self.urlBase,
                        'v1/users',
                        str(user),
                        'devices',
                        str(device_or_token)])
        def onErrback(failure):
            if failure.value.status == '204':
                return None
            failure.raiseException()
        df = client.getPage(url, method='DELETE', agent='txpostal', timeout=60)
        df.addErrback(onErrback)
        return df

    def getDevice(self, user, device_token):
        """
        Asynchronously fetch a device from Postal.

        @param user: a stringable value representing the user id.
        @type user: C{basestring}

        @param device_token: a stringable value representing the device.
        @type user: C{basestring}

        Returns: L{defer.Deferred} to a dict of the device.
        """
        assert isinstance(user, basestring)
        user = user.encode('utf-8')
        device_token = device_token.encode('utf-8')
        url = '/'.join([self.urlBase,
                        'v1/users',
                        str(user),
                        'devices',
                        str(device_token)])
        headers = {'Accept': 'application/json'}
        df = client.getPage(url,
                            method='GET',
                            agent='txpostal',
                            timeout=60,
                            headers=headers)
        df.addCallback(lambda d: json.loads(d))
        return df

    def getDevices(self, user):
        """
        Asynchronously fetch devices for a user from Postal.

        @param user: a stringable value representing the user id.
        @type user: C{basestring}

        Returns: L{defer.Deferred} to a list of devices.
        """
        assert isinstance(user, basestring)
        user = user.encode('utf-8')
        url = '%s/v1/users/%s/devices' % (self.urlBase, user)
        headers = {'Accept': 'application/json'}
        df = client.getPage(url,
                            method='GET',
                            agent='txpostal',
                            timeout=60,
                            headers=headers)
        df.addCallback(lambda d: json.loads(d))
        return df

    def notify(self, notif, users=None, devices=None):
        pass

if __name__ == '__main__':
    from twisted.internet import reactor
    svc = Service()
    user = '000011110000111100001111'
    device = {'device_type': 'aps', 'device_token': '123123123123123123'}
    def p(r):
        print r
        return r
    df = svc.addDevice(user, device)
    df.addBoth(p)
    df.addCallback(lambda d: svc.removeDevice(user, d['device_token']))
    df.addBoth(p)
    df.addCallback(lambda d: svc.getDevices(user))
    df.addBoth(p)
    df.addCallback(lambda d: svc.getDevice(user, d[0]['device_token']))
    df.addBoth(p)
    df.addBoth(lambda *_: reactor.stop())
    reactor.run()
