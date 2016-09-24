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

!['Networks' Tab Screenshot](static/img/doc/dvbinputsnetwork.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

The following tab specific buttons are available: 

Button         | Function
---------------|---------
**Force Scan** | Force a new scan (i.e. scan all muxes for services) for the selected networks.

---

###Adding an Entry/Network

To create a network click the *[Add]* button from the menu bar and 
then select the required network type:

!['Network selection'](static/img/doc/networktypeselect.png)

Then using the resultant dialog enter/select the desired network 
options.

!['Add new network'](static/img/doc/addnewnetwork.png)

Once you're happy with what you've entered into the dialog you can save 
the network using the *[Save]* button (closing the dialog), save your 
pending changes and continue making further adjustments by pressing the 
*[Apply]* button, or cancel any unsaved changes (and close the dialog) 
by pressing the *[Cancel]* button.

**Notes**:

* Once you've created a network (and added muxes) you must assign it to 
an **enabled** adapter.
* There is a 5-10 minute delay before a scan starts, this is so you can 
make changes if needed (this does not apply to IPTV networks).

For more detailed information on networks and how to set them up, see 
[Configure Tvheadend.](configure_tvheadend) 

---

<tvh_include>inc/edit_grid_entries</tvh_include>

!['Add/Edit Network' Dialog - DVB-S/2](static/img/doc/dvbnetworkedit.png)

---

<tvh_include>inc/del_grid_entries</tvh_include>

---

###Force Scanning

Highlight (select) the network(s) you would like to force scan, and 
then press the "Force Scan" button from the menu bar.

Force scanning can take some time. You may continue to use Tvheadend 
while a scan is in progress, but doing so will increase the time needed 
for it to complete. Note that the time required can vary depending on a 
number of factors, such as how many tuners you have available and the 
number of muxes on each.

<tvh_include>inc/selecting_entries_tip</tvh_include>

<tvh_include>inc/paging_tip</tvh_include>

---
