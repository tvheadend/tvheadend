Tvheadend can pretend to be an HDHomeRun device.
This is only available if Tvheadend has been compiled
with the feature enabled.

This is currently an experimental feature.  It may
be withdrawn in the future.

This allows some media players to be able to access
Live TV channels.  Some other media players use a
different technique to access HDHomeRun so will not
be able to play Live TV channels.

On the media player, autodetect of devices will not work.
Instead, you need to enter the IP address and port
number of the Tvheadend web interface.  For example
```1.2.3.4:9981```.  If the media player does not
support manually entering details then it is not
supported.

Typically the media player will then ask for xmltv
details to populate the TV Guide.  This can normally
be retrieved from Tvheadend via the web interface.
For example ```http://1.2.3.4:9981/xmltv/channels```.
