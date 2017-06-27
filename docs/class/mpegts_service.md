Services are automatically pulled from muxes and can be mapped to Channels.

!['Services'](static/img/doc/mpegts_service/tab.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

The following tab specific buttons are available: 

Button                     | Function
---------------------------|---------
**Map Services**           | Drop-down menu (see *Map Services* button table below). 
**Maintenance**            | Drop-down menu (see *Maintenance* button table below).

Map Services Button         | Function
----------------------------|--------------------
**Map selected services**   | Map the highlighted services within the grid. 
**Map all services**        | Map all available services as channels. 

Maintenance Button                              | Function 
------------------------------------------------|-------------------
**Remove unseen services (PAT/SDT) (7 days+)**  | Remove services marked as *Missing in PAT/SDT* for 7+ days. 
**Remove all unseen services**                  | Remove all services not seen for 7+ days. 

---

### Mapping Services to Channels

There are a number of methods to mapping available services, mapping 
uses the following dialog.

!['Service mapper dialog'](static/img/doc/service_mapper/dialog.png)

#### Mapping All

Press the *[Map services]* button and then *[Map all services]*.

!['Map All Services'](static/img/doc/mpegts_service/map_all.png)

The [Map services to channels](class/service_mapper) will now be displayed with **all** services 
checked - feel free to make changes. Once you're happy with the 
selection, press the "Map services" button, you will 
then be taken to the [Service Mapper](status_service_mapper) tab which 
will begin mapping the selected services to channels. 
  
#### Mapping Selected

Click on the services you would like to map as channels, 
once you're done selecting press the "Map services" button and 
then "Map selected services". 

!['Map selected'](static/img/doc/mpegts_service/map_selected.png)
    
The [Map services to channels](class/service_mapper) dialog will 
now be displayed with the **selected** services checked - feel free to make 
changes. Once you're happy with the selection, press the 
"Map services" button, you will then be taken to the 
[Service Mapper](status_service_mapper) tab which will begin mapping 
the selected services to channels. 

**Tip**: By default Tvheadend will only show a small selection of 
available services - you can increase this by using the 
[paging toolbar](webui_general) at the bottom right of the page.
  
#### Mapping/Removing a Service to/from an Existing Channel

You can map/remove a service to/from an existing channel by doing the following:

**1.** Find the desired service from within the services grid. If you 
have a lot of services you may want to use [filtering](webui_general) to limit the 
number of grid entries. 

**2.** Double click on the channel field, a drop down listing of all defined 
channels will appear, check/uncheck the check box next to the channel 
you'd like to associate/disassociate the service with. 

!['Add service to channel example'](static/img/doc/mpegts_service/add_service.png)

**3.** Press the *[Save]* button from the menu bar, and you're done!

---

### Service Information

Clicking the !['Information Icon'](static/icons/information.png) 
information icon will display service details.

!['Service Information'](static/img/doc/mpegts_service/service_info.png)

---

<tvh_include>inc/play</tvh_include>

---
