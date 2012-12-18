# Postal

Postal is a push notification server for various mobile platforms.

## Requirements

 * Add device for notifications.
 * Remove device from notifications.
 * Query devices for a particular user id.
 * Deliver a notification to one or more devices.
 * Deliver a notification to all devices matching a query.

## Features

 * JSON API over HTTP.
 * Support for APS devices.
 * Support for C2DM devices.
 * Support for GCM devices.
 * Optimizations in message delivery to coalesce and aggregate when possible.
 * Doesn't require client side libraries, just use native platform libraries.

## Upcoming Features

 * Avoid duplicate sending of notifications with coallese key.
 * Queue notification delivery until the devices registered sleep time
   has elapsed.
 * Act as a MongoDB server for alternative failover scenarios.

## Important Notes

 * This server is meant to be installed behind your firewall. It does not
   perform authentication for your users. You do that in your API and then
   communicate with Postal internally.

## Installation

### Requirements

 * gobject >= 2.32
 * libsoup >= 2.26
 * json-glib >= 0.14

#### Optional

 * hiredis for metrics delivery

### Linux

#### Installation from release

 * TODO: Update for first release

#### Installation from git

```sh
git clone https://github.com/catch/postal.git
cd postal
./autogen.sh --enable-silent-rules --enable-debug=minimum --enable-trace=no
make
make install
```

## REST API

 * TODO: Describe REST API.

### Add Device

```sh
$ cat device.json
{
  "device_type": "aps",
  "device_token": "1212121212121212121212121212121212121212121212121212121212121212",
  "user": "012345678901234567890123"
}
$ curl -i -X PUT http://localhost:5300/v1/users/012345678901234567890123/devices/1212121212121212121212121212121212121212121212121212121212121212 --data-binary @device.json
HTTP/1.1 201 Created
Server: Postal/0.1.0
Date: Tue, 18 Dec 2012 02:46:33 GMT
Location: /v1/users/012345678901234567890123/devices/1212121212121212121212121212121212121212121212121212121212121212
Content-Type: application/json
Content-Length: 217

{
  "device_token" : "1212121212121212121212121212121212121212121212121212121212121212",
  "device_type" : "aps",
  "user" : "012345678901234567890123",
  "created_at" : "2012-12-18T02:46:33Z",
  "removed_at" : null
}
```
