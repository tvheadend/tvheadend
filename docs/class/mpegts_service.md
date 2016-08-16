Services are automatically pulled from muxes and can be mapped to Channels.

!['Services'](static/img/doc/configdvbservices.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

The following tab specific buttons are available: 

Button                     | Function
---------------------------|---------
**Map Services**           | Drop down menu (see mapping button table below). 

Mapping Button              | Function
----------------------------|--------------------
**Map selected services**   | Map the highlighted services within the grid. 
**Map all services**        | Map all available services as channels. 

---

###Mapping Services to Channels

  There are a number of methods to mapping available services, 
  mapping uses the following dialog.

  !['Service mapper dialog'](static/img/doc/mapservicesdialog.png)

####Mapping All

Press the *[Map services]* button and then *[Map all services]*.

!['Map All Services'](static/img/doc/mapservicesall.png)

The [Map services to channels](class/service_mapper) will now be displayed with **all** services 
checked - feel free to make changes. Once you're happy with the selection press the "Map services" button, you will 
then be taken to the [Service Mapper](status_service_mapper) tab which 
will begin mapping the selected services to channels. 
  
####Mapping Selected

Click on the services you would like to map as channels, 
once you're done selecting press the "Map services" button and 
then "Map selected services". 

!['Map selected'](static/img/doc/mapselectedservices.png)
    
The [Map services to channels](class/service_mapper) dialog will 
now be displayed with the **selected** services checked - feel free to make 
changes. Once you're happy with the selection press the 
"Map services" button, you will then be taken to the 
[Service Mapper](status_service_mapper) tab which will begin mapping 
the selected services to channels. 

<tvh_include>inc/selecting_entries_tip</tvh_include>

**Tip**: By default Tvheadend will only show a small selection of 
available services - you can increase this by using the paging 
selector at the bottom right of the page.
  
####Mapping/Removing a Service to/from an Existing Channel

You can map/remove a service to/from an existing channel by doing the following:

**1)** Find the desired service from within the services grid. 

If you have a lot of services you may want to use filtering to limit the 
number of grid entries. You can do this by hovering your mouse over the 
*Service name* column, a down arrow ▾ should now be visible, clicking 
the arrow will then display a list of options, move your mouse down to 
"Filters" and a text box should then appear, click on it and enter the 
desired service's name.

!['Service filtering'](static/img/doc/servicefilter.png)

**Tip**: Remember to remove the filter when you're finished (uncheck the 
check box next to the "Filters" option). 

**2)** Double click on the channel field, a drop down listing of all defined 
channels will appear, check/uncheck the check box next to the channel 
you'd like to associate/disassociate the service with. 

!['Add service to channel example'](static/img/doc/addservicetochannel.png)

**3)** Press the *[Save]* button from the menu bar, and you're done!

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

<tvh_include>inc/del_grid_entries</tvh_include>

---

###Service Information

Clicking the !['Information Icon'](static/icons/information.png) 
information icon will display service details.

---

<tvh_include>inc/play</tvh_include>

---
