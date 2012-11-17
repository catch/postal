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
 * Optimizations in message delivery to coalesce and aggregate when possible.

## Upcoming Features

 * Avoid duplicate sending of notifications with coallese key.
 * Support for GCM devices.
 * Queue notification delivery until the devices registered sleep time
   has elapsed.
 * Act as a MongoDB server for alternative failover scenarios.

## REST API

 * TODO: Describe REST API.
