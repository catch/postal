# Things we need immediately.

 * Delay of notification delivery.
 * Add /status HTTP page.

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

