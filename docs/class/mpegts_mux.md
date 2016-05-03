Muxes are locations at which services can be found. On traditional 
networks (DVB-C, -T and -S), these are carrier signals on which the 
individual channels are multiplexed, hence the name. However, Tvheadend 
also uses the term ‘mux’ to describe a source for multiple IP 
streams - so an IP address, in effect.

!['Mux List'](docresources/configdvbmux.png)

---

###Menu Bar/Buttons

The following functions are available:

Button         | Function
---------------|---------
**Save**       | Save any changes made to the mux entries.
**Undo**       | Undo any changes made to the mux entries since the last save.
**Add**        | Add a new mux.
**Delete**     | Delete an existing mux.
**Edit**       | Edit an existing mux. This allows you to change any of the parameters you’d otherwise set when adding a new mux, e.g. EPG scan, character set, etc.
**View Level** | Change the interface view level to show/hide more advanced options.
**Help**       | Displays this help page. 

---

###Playing a Mux

You can stream a complete multiplex by copying/pasting the *Play* link 
into the desired player/software.

**Notes**:
* The *Play* links don't link to the actual stream but to a playlist for 
use with media players such as VLC, If you'd prefer to receive the raw 
transport stream instead, you can do so by removing the `/play/` path from 
the *Play* URL.
* Not all devices support receiving a complete mux, notably low 
powered set-top-boxes and older USB tuners. 

---

###Adding a Mux

To add a mux press the *[Add]* button from the menu bar and select the 
network you want to add the mux to.

You only really need to add muxes if the pre-defined list didn't 
work, e.g. because of out-of-date data as broadcasters re-arrange their 
services or because automatic detection (network discovery) hasn't 
successfully found all the muxes over time.

!['Add Mux Dialog'](docresources/configaddmuxexample.png)

**Notes**: 
* Some tuners (or drivers) require more tuning parameters than 
others so be sure to enter as many tuning parameters as possible.
* Newly added muxes are automatically set to the *PEND* state.
* Tvheadend won't scan the newly added mux instantly, it can take up to 
10 minutes to begin an initial scan.

---

###Editing a Mux

To edit a mux highlight the desired mux from within the grid, and 
press the *[Edit]* button from the menu bar.

**Tip**: You can also make changes to a mux from within the grid.

---

###Deleting a Mux

To edit a mux highlight the desired mux from within the grid, and 
press the *[Delete]* button from the menu bar. 

Deleting a mux will also remove any associated services, including 
those mapped to channels. If you have network discovery enabled any 
previously deleted muxes found in the NIT during a scan will 
automatically be re-added.

**Tip**: You can select all muxes within the grid by pressing ctrl+A. 
You can also ctrl+click to make additional selections, or shift+click to 
select a range. 

---
