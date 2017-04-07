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
[parameters](#items).

!['Networks' Tab Screenshot](static/img/doc/mpegts_network/tab.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

The following tab specific buttons are available: 

Button         | Function
---------------|---------
**Force Scan** | Force a new scan (i.e. scan all muxes for services) for the selected networks.

---

###Force Scanning

Force scanning can take some time. You may continue to use Tvheadend 
while a scan is in progress, but doing so will increase the time needed 
for it to complete. Note that the time required can vary depending on a 
number of factors, such as how many tuners you have available and the 
number of muxes on each.

---
