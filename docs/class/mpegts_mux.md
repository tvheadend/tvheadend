Muxes are locations at which services can be found. On traditional 
networks (DVB-C, -T and -S), these are carrier signals on which the 
individual channels are multiplexed, hence the name. However, Tvheadend 
also uses the term ‘mux’ to describe a source for multiple IP 
streams - so an IP address, in effect.

!['Mux List'](static/img/doc/configdvbmux.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

---

###Adding an Entry/Mux

To add a mux press the *[Add]* button from the menu bar and select the 
network you want to add the mux to:

!['Network selection'](static/img/doc/addmuxnetworkselection.png)

Then enter the mux information:

!['Add Mux Dialog'](static/img/doc/configaddmuxexample.png)

Pressing the *[Save]* button (at the bottom of the dialog) 
will commit your changes and close the dialog, pressing the *[Apply]* 
button will commit your changes but won't close the dialog, pressing 
the *[Cancel]* button closes the dialog - any unsaved changes will be 
lost.

Note: You only really need to add muxes if the pre-defined list didn't 
work, e.g. because of out-of-date data as broadcasters re-arrange their 
services or because automatic detection (network discovery) hasn't 
successfully found all the muxes over time.

**Tips**: 
* If you're not sure what to enter here, take a look at the "If Necessary, 
Manually Add Muxes" section on the [Configure Tvheadend](configure_tvheadend) 
page.
* Some tuners (or drivers) require more tuning parameters than 
others so be sure to enter as many tuning parameters as possible.
* Newly added muxes are automatically set to the *PEND* state.
* Tvheadend won't scan the newly added mux instantly, it can take up to 
10 minutes to begin an initial scan.

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

###Deleting an Entry/Mux

To delete a mux highlight (select) the desired muxes from within the 
grid, and press the *[Delete]* button from the menu bar. 

Deleting a mux will also remove any associated services, including 
those mapped to channels. If you have network discovery enabled any 
previously deleted muxes found in the NIT during a scan will 
automatically be re-added.

<tvh_include>inc/selecting_entries_tip</tvh_include>

<tvh_include>inc/paging_tip</tvh_include>

---

###Playing a Mux

You can stream a complete multiplex by copying/pasting the *Play* icon link 
into the desired player/software.

**Notes**:
* The links don't link to the actual stream but to a playlist for 
use with media players such as VLC, If you'd prefer to receive the raw 
transport stream instead, you can do so by removing the `/play/` path from 
the URL.
* Not all devices support receiving a complete mux, notably low 
powered set-top-boxes and older USB tuners. 

---
