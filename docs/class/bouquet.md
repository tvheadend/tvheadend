Bouquets are broadcaster-defined groupings and orders of channels.

!['Bouqets' Tab](static/img/doc/bouquettab.png)

To use bouquets, ensure to add and scan all available muxes using the
predefined muxes or manual configuration.

Bouquets are usually obtained automatically from the DVB source during the
mux scan period. Note that bouquets may use more muxes and only services 
from scanned muxes are added. The mux with bouquets might require
another scan when all muxes are discovered (manually using the rescan
checkbox).

The fastscan bouquets are pre-defined in the configuration tree. These
bouquets must be manually enabled to let Tvheadend to subscribe and
listen to the specific MPEG-TS PIDs.

You may import your own bouquet using enigma2 (.tv) formatted files.

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

The following tab specific buttons are available: 

Button         | Function
---------------|---------
**Force Scan** | Rescan the selected mux for changes to the bouquet.

---

<tvh_include>inc/add_grid_entry</tvh_include>

####Example

!['Add Bouquet Dialog'](static/img/doc/bouquetadd.png)

Note that the URL must begin with `file://` or `http(s)://`.

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

###Detaching Channels

If you're mapping another service to a channel created by a bouquet you 
must first detach the channel to prevent unexpected 
changes, you can do this by selecting the desired channels from within 
the grid and then pressing the *Detach selected channels from bouquet* 
option from the *[Map services]* button.

!['Map All Services'](static/img/doc/mapallserviceschannels.png)

If you do not detach channel(s) before mapping additional 
services the following changes can occur..

* The last mapped service's values will override the channel values set by the bouquet, e,g, if service "MyTV" with channel number 155 gets mapped to the channel "BBC One" on channel number 1 it will be renamed "MyTV" with channel number 155.
* Any changes (mapped services, number changes etc) to the channels can be lost if new changes in the bouquet override them.

Detaching channels from a bouquet will prevent any further updates 
provided by the bouquet, which unfortunately means you will have to 
manually re-map when changes to services occur (e.g, mux moves, ceased broadcasting etc).

<tvh_include>inc/selecting_entries_tip</tvh_include>

---

<tvh_include>inc/del_grid_entries</tvh_include>

---
