Bouquets are broadcaster-defined groupings and orders of channels.

!['Bouqets' Tab](static/img/doc/bouquettab.png)

To use bouquets, ensure to add and scan all available muxes using the
predefined muxes or manual configuration.

The bouquets are obtained automatically from the DVB source during the
mux scan period. Note that bouquets may use more muxes and only services
from scanned muxes are added. The mux with bouquets might require
another scan when all muxes are discovered (manually using the rescan
checkbox).

The fastscan bouquets are pre-defined in the configuration tree. These
bouquets must be manually enabled to let Tvheadend to subscribe and
listen to the specific MPEG-TS PIDs.

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

The following tab specific buttons are available: 

Button         | Function
---------------|---------
**Force Scan** | Rescan the selected mux for changes to the bouquet.

---

<tvh_include>inc/add_grid_entry</tvh_include>

!['Add Bouquet Dialog'](static/img/doc/bouquetadd.png)

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

<tvh_include>inc/del_grid_entries</tvh_include>

---
