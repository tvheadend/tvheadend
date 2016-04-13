##Debugging

###Trace Options

The following options can be passed to tvheadend to provide detailed debugging 
information while the application is running. Trace debugging has to be enabled
at build time (`--enable-debugging`) and can then either be specified on the command-
line (e.g. `tvheadend -u hts -g video --trace <module>`) or in the web interface (_Configuration -> Debugging_).

More than one option can be used, e.g. _--trace cwc,dvr,linuxdvb_

no-highlight

```
all
access
bouquet
capmt
cwc
descrambler
diseqc
dvbcam
dvr
eit
en50221
epg
epggrab
fastscan
fsmonitor
gtimer
htsp
httpc
idnode
linuxdvb
main
mkv
mpegts
opentv
parser
pass
psi
satip
satips
scanfile
sdt
service
service_mapper
settings
subscription
tcp
thread
time
timeshift
transcode
tsfile
tvhdhomerun
upnp
```
