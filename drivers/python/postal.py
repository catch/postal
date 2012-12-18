#!/usr/bin/env python

#
# Copyright 2012 Catch.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Python driver for communicating with the Postal Push Notification Server.
"""

import json
import urllib2

class PostalClient(object):
    def __init__(self, host='127.0.0.1', port=5300):
        self.urlBase = 'http://%s:%d' % (str(host), int(port))
        self.opener = urllib2.build_opener(urllib2.HTTPHandler)

    def addDevice(self, user, device):
        url = '/'.join([self.urlBase,
                        'v1/users',
                        str(user),
                        'devices',
                        str(device['device_token'])])
        postdata = json.dumps(device)
        request = urllib2.Request(url)
        request.get_method = lambda: 'PUT'
        url = self.opener.open(request)
        return json.loads(url.read())

    def removeDevice(self, user, device_or_token):
        if isinstance(device_or_token, dict):
            device_or_token = device_or_token.get('device_token')
        url = '/'.join([self.urlBase,
                        'v1/users',
                        str(user),
                        'devices',
                        str(device_or_token)])
        request = urllib2.Request(url)
        request.get_method = lambda: 'DELETE'
        url = self.opener.open(request)
        data = url.read()
        return not data

    def getDevice(self, user, device_or_token):
        if isinstance(device_or_token, dict):
            device_or_token = device_or_token.get('device_token')
        url = '/'.join([self.urlBase,
                        'v1/users',
                        str(user),
                        'devices',
                        str(device_or_token)])
        request = urllib2.Request(url)
        request.get_method = lambda: 'GET'
        url = self.opener.open(request)
        return json.loads(url.read())

    def getDevices(self, user):
        url = '/'.join([self.urlBase, 'v1/users', str(user), 'devices'])
        request = urllib2.Request(url)
        request.get_method = lambda: 'GET'
        url = self.opener.open(request)
        return json.loads(url.read())

if __name__ == '__main__':
    client = PostalClient()
    user = '000011110000111100001111'
    for device in client.getDevices(user):
        assert client.removeDevice(user, device)
        device2 = client.getDevice(user, device)
        device.pop('removed_at')
        device2.pop('removed_at')
        assert device == device2
