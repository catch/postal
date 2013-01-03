#!/usr/bin/env python

import bson
from   collections import OrderedDict
from   datetime import datetime
import os
import types
import re

def toson(doc):
    return bson.SON(doc)

def encode(doc):
    return bson.BSON.encode(toson(doc))

def save(name, doc):
    file(name, 'w').write(encode(doc))

save('test1.bson', {'int': 1})
save('test2.bson', {'int64': long(1)})
save('test3.bson', {'double': 1.123})
save('test4.bson', {'utc': datetime(2011, 10, 22, 12, 13, 14, 123000)})
save('test5.bson', {'string': 'some string'})
save('test6.bson', {'array[int]': [1, 2, 3, 4, 5, 6]})
save('test7.bson', {'array[double]': [1.123, 2.123]})
save('test8.bson', {'document': toson({'int': 1})})
save('test9.bson', {'null': None})
save('test10.bson', {'regex': re.compile('1234', re.IGNORECASE)})
save('test11.bson', {'hello': 'world'})
save('test12.bson', {'BSON': ['awesome', 5.05, 1986]})
save('test13.bson', {'array[bool]': [True, False, True]})
save('test14.bson', {'array[string]': ['hello', 'world']})
save('test15.bson', {'array[datetime]': [datetime(1970, 1, 1, 0, 0, 0), datetime(2011, 10, 22, 12, 13, 14, 123000)]})
save('test16.bson', {'array[null]': [None, None, None]})

d = OrderedDict()
d['_id'] = bson.ObjectId('12345'.zfill(24))
d['document'] = OrderedDict()
d['document']['_id'] = bson.ObjectId('12345'.zfill(24))
d['document']['tags'] = ['1','2','3','4']
d['document']['text'] = 'asdfanother'
d['document']['source'] = {'name': 'blah'}
d['type'] = ['source']
d['missing'] = ['server_created_at']
save('test17.bson', d)
