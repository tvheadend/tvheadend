A network is the type of carrier for your television signals. Tvheadend
supports several different types of network, notably:

* [Network Types](class/mpegts_network)
 * Cable TV, delivered via a cable to your house
   - [DVB-C](class/dvb_network_dvbc) - common in most of Europe
   - [ISDB-C](class/dvb_network_isdb_c) - common in Brazil and various other countries throughout south America
   - [ATSC-C](class/dvb_network_atsc_c) - common in north and central America and parts of south Asia
 * Satellite, any signal coming in via a dish
   - [DVB-S](class/dvb_network_dvbs) - Available worldwide
   - [ISDB-S](class/dvb_network_isdb_s) - available worldwide but common in Brazil and various other countries throughout south America
 * Terrestrial, over-the-air broadcasts received through a traditional television aerial
   - [DVB-T](class/dvb_network_dvbt) - common in most of Europe
   - [ISDB-T](class/dvb_network_isdb_t) - common in Brazil and various other countries throughout south America
   - [ATSC-T](class/dvb_network_atsc_t) - common in north and central America
 * IPTV - TV over the Internet via your broadband connection
   - [IPTV](class/iptv_network)
   - [IPTV Automatic Network](class/iptv_auto_network) - IPTV using a playlist as the source
  
Click the desired network type (above) to see all available 
[options](#items).

!['Networks' Tab Screenshot](docresources/dvbinputsnetwork.png)

---

###Menu Bar/Buttons

The following functions are available:

Button         | Function
---------------|---------
**Save**       | Save any changes made to the network configuration.
**Undo**       | Undo any changes made to the network configuration since the last save.
**Add**        | Add a new network. You can choose from any of the types described above.
**Delete**     | Delete an existing network. This will also remove any association with an adapter.
**Edit**       | Edit an existing network. This allows you to change any of the parameters you’d otherwise set when adding a new network, e.g. network discovery, idle scan, etc. - similar to using the check boxes to enable/disable functions.
**Force Scan** | Force a new scan (i.e. scan all muxes for services) for the selected networks.
**View Level**| Change the interface view level to show/hide more advanced options.
**Help**       | Displays this help page. 

---

###Add/Edit a Network

To create a network click the *[Add]* button from the menu bar and 
select the required network type, and then using the resultant dialog 
enter/select the desired network options. To edit a network highlight 
(select) the network within the grid, and then press the *[Edit]* 
button from the menu bar.

A common set of fields is used for the _Add_ or _Edit_ functions, most
of which can also be seen in the grid view:

!['Add/Edit Network' Dialog - DVB-S/2](docresources/dvbnetworkedit.png)

**Notes**:

* Once you've created a network (and added muxes) you must assign it to 
an **enabled** adapter.
* There is a 5-10 minute delay before a scan starts, this is so you can 
make changes if needed (this does not apply to IPTV networks).

---

###Force Scanning

You can force a scan by highlighting (selecting) the network(s) within the grid, 
and then pressing the *[Force Scan]* button from the menu bar. 

---

###Deleting a Network

To delete a network highlight (select) the network within the grid, and then 
press the *[Delete]* button.

---
