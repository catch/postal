# Things we need immediately.

 * Delay of notification delivery.
   - This involves storing collapse keys temporarily with something like
     "device_id:collapse_key". It should timeout after 5 minutes or so.
   - We also might want to delay the sending of the first message by
     a few milliseconds to help catch spurious deliveries. However, I
     suspect that the "device_id:collapse_key" will handle this better.
 * Python drivers for twisted and urllib.
 * Setting "badge" field of device, automatically sends notification
   if necessary.
 * Setting of extra fields. There are cases where you might want to
   tag a device with something like an account_level, or group id.
   This should allow for delivering to devices matching this key.

# Things that would be nice.

 * Right now, everything is on the main loop. It would be nice to move
   libsoup and mongo to separate main loops. Additionally, we should do
   serialization and deserialization on threads. But before doing this,
   make sure to test the upper limit of libsoup, it is likely to make
   things slower if not done right.
 * General code cleanup.
 * Documentation.
 * Code comments.
 * Exponential back-off to GCM, C2DM, and APS services.

