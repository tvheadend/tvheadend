This tab is used to configure various debugging options in tvheadend.

!['Debugging tab'](static/img/doc/debuggingtab.png)

Changes to any of these settings must be confirmed by pressing the 
*[Apply configuration]* button before taking effect.

Note that settings are not saved to a storage. Any change is available 
only while Tvheadend is running, and will be lost on a restart. 
To change the default behaviour permanently, use command line options 
such as `-l,` `–debug`, `–trace`.

Depending on your distribution, the default command-line configuration 
is usually stored in the `/etc/sysconfig` tree or an init script. 
You may also be able to change `/etc/default/tvheadend` to add additional 
command-line parameters.

---

###Menu Bar/Buttons

The following functions are available:

Button     | Function
-----------|---------
**Apply configuration (run-time only)**   | Apply the entered debugging settings.
<tvh_include>inc/common_button_table_end</tvh_include>

---

###Subsystems

The following options can be passed to tvheadend to provide detailed debugging 
information while the application is running.

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

---
