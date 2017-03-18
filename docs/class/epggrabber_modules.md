This tab is used to configure the Electronic Program Guide (EPG) 
grabber modules. Tvheadend supports a variety of different EPG 
grabbing mechanisms. These fall into 3 broad categories, within which 
there are a variety of specific grabber implementations.

!['EPG Grabber Configuration'](static/img/doc/epggrabmodules.png)

---

###Menu Bar/Buttons

The following functions are available:

Button                      | Function
----------------------------|-------------------
**Save**                    | Save any changes made to the tab.
**Undo**                    | Revert any changes made since the last save.

<tvh_include>inc/common_button_table_end</tvh_include>

---

###Notes

* Only OTA EIT and PSIP (ATSC) grabbers are enabled by default. If 
you're missing EPG data, make sure to enable the correct grabber(s) 
for your location/provider.
* If you use more than one grabber, be sure to give a higher priority 
to the grabber that provides you with richer data.

**Tip**: Don't forget to set the *EIT time offset* for your network(s).

---
