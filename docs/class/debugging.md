Contents                                          | Description
--------------------------------------------------|------------------------
[Overview](#overview)                             | Tab overview
[Subsystems](#subsystems)                         | Available debugging subsystems
[Items/Properties](#items)                        | Items/Properties

---

## Overview

This tab is used to configure various debugging options in Tvheadend.

!['Debugging tab'](static/img/doc/debugging/tab.png)

### Notes

* Changes to any of these settings must be confirmed by pressing the 
*[Apply configuration]* button before taking effect.

* Settings are not saved to a storage. Any change is available 
only while Tvheadend is running, and will be lost on a restart. 
To change the default behaviour permanently, use command line options 
such as `-l,` `–debug`, `–trace`.

* Depending on your distribution, the default command-line configuration 
is usually stored in the `/etc/sysconfig` tree or an init script. 
You may also be able to change `/etc/default/tvheadend` to add additional 
command-line parameters.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Subsystems

The following options can be passed to Tvheadend to provide detailed debugging 
information while the application is running.



Subsystem      | Short Description
---------------|------------------------
START          | START
STOP           | STOP
CRASH          | CRASH
main           | Main
tprof          | Time profiling
qprof          | Queue profiling
CPU            | CPU
thread         | Thread
tvhpoll        | Poll multiplexer
time           | Time
spawn          | Spawn
fsmonitor      | Filesystem monitor
lock           | Locking
uuid           | UUID
idnode         | Node subsystem
url            | URL
tcp            | TCP Protocol
rtsp           | RTSP Protocol
upnp           | UPnP Protocol
settings       | Settings
config         | Configuration
access         | Access (ACL)
cron           | Cron
dbus           | DBUS
avahi          | Avahi
bonjour        | Bonjour
api            | API
http           | HTTP Server
httpc          | HTTP Client
htsp           | HTSP Server
htsp-sub       | HTSP Subscription
htsp-req       | HTSP Request
htsp-ans       | HTSP Answer
imagecache     | Image Cache
tbl            | DVB SI Tables
tbl-base       | Base DVB SI Tables (PAT,CAT,PMT,SDT etc.)
tbl-csa        | DVB CSA (descrambling) Tables
tbl-eit        | DVB EPG Tables
tbl-time       | DVB Time Tables
tbl-atsc       | ATSC SI Tables
tbl-pass       | Passthrough Muxer SI Tables
tbl-satip      | SAT>IP Server SI Tables
fastscan       | Fastscan DVB
pcr            | PCR Clocks
parser         | MPEG-TS Parser
TS             | Transport Stream
globalheaders  | Global Headers
tsfix          | Time Stamp Fix
hevc           | HEVC - H.265
muxer          | Muxer
pass           | Pass-through muxer
audioes        | Audio muxer
mkv            | Matroska muxer
service        | Service
channel        | Channel
subscription   | Subscription
service-mapper | Service Mapper
bouquet        | Bouquet
esfilter       | Elementary Stream Filter
profile        | Streaming Profile
descrambler    | Descrambler
descrambler-emm| Descrambler EMM
caclient       | CA (descrambling) Client
csa            | CSA (descrambling)
capmt          | CAPMT CA Client
cwc            | CWC CA Client
cccam          | CWC CCCam Client
dvbcam         | DVB CAM Client
dvr            | Digital Video Recorder
dvr-inotify    | DVR Inotify
epg            | Electronic Programme Guide
epgdb          | EPG Database
epggrab        | EPG Grabber
charset        | Charset
dvb            | DVB
mpegts         | MPEG-TS
muxsched       | Mux Scheduler
libav          | libav / ffmpeg
transcode      | Transcode
iptv           | IPTV
iptv-pcr       | IPTV PCR
iptv-sub       | IPTV Subcription
linuxdvb       | LinuxDVB Input
diseqc         | DiseqC
en50221        | CI Module
en50494        | Unicable (EN50494)
satip          | SAT>IP Client
satips         | SAT>IP Server
tvhdhomerun    | TVHDHomeRun Client
psip           | ATSC PSIP EPG
opentv         | OpenTV EPG
pyepg          | PyEPG Import
xmltv          | XMLTV EPG Import
webui          | Web User Interface
timeshift      | Timeshift
scanfile       | Scanfile
tsfile         | MPEG-TS File
tsdebug        | MPEG-TS Input Debug
codec          | Codec
vaapi          | VA-API
ddci           | DD-CI

---
