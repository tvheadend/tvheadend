This tab lists all defined channels.

!['Channel lists'](static/img/doc/channel/tab.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

The following tab specific functions are available:

Button                      | Function
----------------------------|-------------------
**Reset Icon**              | Reset the selected channel(s) *User Icon* / *Icon URL*, especially useful if you change your Picon settings. 
**Map Services**            | Drop down menu (see mapping button table below). 
**Number Operations**       | Drop down menu (see numbering button table below).

Mapping Button              | Function
----------------------------|--------------------
**Map services**            | Map [services](class/mpegts_service).
**Map all services**        | Map all available [services](class/mpegts_service) as channels

Numbering Button            | Function
----------------------------|--------------------
**Assign Number**           | Assign the lowest available channel number(s) to the selected channel(s).
**Number Up**               | Increment the selected channel number(s) by 1. 
**Number Down**             | Decrement the selected channel numbers by 1. 
**Swap Numbers**            | Swap the numbers of the **two** selected channels.

---

### Example

!['Add Channel Dialog'](static/img/doc/channel/add.png)

In the above example image, we're creating a channel called Channel 4 
and mapping it to the service of the same name. You can name a channel 
whatever you like, it doesn't have to match the service it's linking 
to. 

If you have a lot of services you may want to use the [Map services](class/mpegts_service) 
functions or a [Bouquet](class/bouquet).

Note, that editing a channel created by a bouquet can have unexpected 
results, please see *Detaching Channels* on the [Bouquet](class/bouquet) page for info.

---

<tvh_include>inc/play</tvh_include>

---
