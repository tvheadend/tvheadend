SAT\>IP Server is something like DVB network tuner. Tvheadend can
forward mpegts input streams including on-the-fly descramling to SAT\>IP
clients.

!['SAT\>IP Config tab'](static/img/doc/satipconfig.png)

---

###Menu Bar/Buttons

The following functions are available:

Button                      | Function
----------------------------|-------------------
**Save**                    | Save all changes.
**Undo**                    | Revert all changes since last save.
**Discover SAT\>IP servers**| Attempt to discover more SAT>IP servers on the network.
<tvh_include>inc/common_button_table_end</tvh_include>

---

###General Information

Only networks with the “SAT>IP Source” field set are exported through 
the SAT>IP protocol. This field is matched through the “src” parameter 
requested by the SAT>IP client. Usually (and by default) this value 
is 1. For satellite tuners, this value determines the satellite source 
(dish). By specification position 1 = DiseqC AA, 2 = DiseqC AB, 3 = 
DiseqC BA, 4 = DiseqC BB, but any numbers may be used - depends on the 
SAT>IP client. Note that if you use a similar number for multiple 
networks, the first matched network containing the mux with requested 
parameters will win (also applies to unknown muxes).

---

###Basic Configuration Guide

####1. Define the RTSP Port

This can be anything you like, however it is 
recommended that you use 9983 (to avoid permission issues). Entering 
zero (0) in this field will disable the server. 

####2. Export the Tuners

In the *Exported tuners* section enter the 
number of tuners (per delivery system) that you'd like to export. This 
setting lets the client know how many tuners are available for use. 
While you can enter any number you like here, exporting more tuners 
than you have can lead to scanning/tuning failures, e.g. "No free tuner".

####3. Export Your Networks 

Tvheadend won't export any tuners without any assigned networks, if you 
haven't already done so you must enter a *SAT\>IP source number* for 
a [network](mpegts_network). If you don't export a network you may see 
the following debug error message.

`satips: SAT>IP server announces an empty tuner list to a client <IP ADDRESS OF CLIENT> (missing network assignment)` 

####4. Configure Your Client

Hopefully (and if everything went to plan) your client should have 
now detected Tvheadend as a SAT\>IP server, if not, you may want to 
trigger service discovery or restart it.

---
