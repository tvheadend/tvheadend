##Overview of Tvheadend

###Welcome to Tvheadend!

Tvheadend is a lightweight, easily-configured, general-purpose TV/video 
streaming server and recorder (PVR/DVR) for GNU/Linux, FreeBSD and Android.

![Tvheadend interface](images/overall_screenshot.png)

It supports input from:

* Cable TV, delivered via a cable to your house (DVB-C)
* Satellite, so any signal coming in via a dish (DVB-S and DVB-S2)
* Terrestrial, so over-the-air broadcasts received through a
traditional television aerial (DVB-T and DVB-T2 in much of the world, ATSC
in north and central America)
* Internet and LAN feeds, such as IPTV, SAT>IP, HDHomeRun and a general-purpose
MPEG-TS `pipe://`

As well as being able to record the input, Tvheadend also offers it up to
client applications via HTTP (VLC, MPlayer), HTSP (Kodi, Movian) and SAT>IP
streaming.

While supported in previous versions, analogue video (V4L) is no longer
supported directly. If you still need this, or need to input signals
from video cameras or other non-broadcast sources, use `pipe://`.

The code is hosted at [github](https://github.com/tvheadend/tvheadend).
Please use github's features if you want to provide patches. Contributions and improvements are always welcome.
