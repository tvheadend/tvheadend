A network is the type of carrier for your television signals. Tvheadend
supports several different types of network, notably:

* Cable TV, delivered via a cable to your house
  - [DVB-C](class/dvb_network_dvbc)
  - [ISDB-C](class/dvb_network_isdb_c)
  - [ATSC-C](class/dvb_network_atsc_c) - common in north and central America and parts of south Asia
  
* Satellite, any signal coming in via a dish
  - [DVB-S](class/dvb_network_dvbs)
  - [ISDB-S](class/dvb_network_isdb_s)
  
* Terrestrial, over-the-air broadcasts received through a traditional television aerial
  - [DVB-T](class/dvb_network_dvbt) - common in most of Europe
  - [ISDB-T](class/dvb_network_isdb_t) - common in Brazil and various other countries throughout south America
  - [ATSC-T](class/dvb_network_atsc_t) - common in north and central America
  
* IPTV - TV over the Internet via your broadband connection
  - [IPTV](class/iptv_network)
  - [IPTV Automatic Network](class/iptv_auto_network) - IPTV using a playlist as the source

!['Networks' Tab Screenshot](docresources/dvbinputsnetwork.png)

---

###Add/Edit Dialog Example

A common set of fields is used for the _Add_ or _Edit_ functions, most
of which can also be seen in the grid view:

!['Add/Edit Network' Dialog - DVB-S/2](docresources/dvbnetworkedit.png)

!['Add/Edit Network' Dialog - IPTV](docresources/configdvbnetwork_iptv.png)

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
**Help**       | Displays this help page. 

---
