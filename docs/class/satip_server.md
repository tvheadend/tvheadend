SAT>IP Server is something like DVB network tuner. Tvheadend can
forward mpegts input streams including on-the-fly descramling to SAT\>IP
clients.

Only networks with the “SAT>IP Source” field set are exported through
the SAT>IP protocol. This field is matched through the “src” parameter
requested by the SAT>IP client. Usually (and by default) this value is 1.
For satellite tuners, this value determines the satellite source (dish).
By specification position 1 = DiseqC AA, 2 = DiseqC AB, 3 = DiseqC BA, 4
= DiseqC BB, but any numbers may be used - depends on the SAT\>IP
client. Note that if you use a similar number for multiple networks, the
first matched network containing the mux with requested parameters will
win (also for unknown mux).

!['SAT\>IP Config tab'](docresources/satipconfig.png)

---

###Buttons

The tab has the following buttons:

Button                      | Function
----------------------------|-------------------
**Save**                    | Save all changes.
**Undo**                    | Revert all changes since last save.
**Discover SAT\>IP servers**| Attempt to discover more SAT>IP servers on the network.
**Help**                    | Display this help page.

---
