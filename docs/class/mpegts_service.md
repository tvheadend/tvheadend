Services are automatically pulled from muxes and can be mapped to Channels.

!['Services'](docresources/configdvbservices.png)

---

###Menu Bar/Buttons

The following functions are available:

Button           | Function
-----------------|---------
**Save**         | Save any changes made.
**Undo**         | Undo any changes made since the last save.
**Edit**         | Edit an existing service.
**Delete**       | Delete a service.
**Map Services** | Drop down menu: Map selected services, map the highlighted services within the grid. Map all services, map all available services as channels. Both options use the *Map services to channels* dialog. 
**View Level**   | Change the interface view level to show/hide more advanced options.
**Help**         | Displays this help page. 

---

###Mapping Services to Channels

There are a number of methods to mapping available services.

*Map all services* and *Map selected services* functions use the 
following dialog.

!['Service mapper dialog'](docresources/mapservicesdialog.png)

####Mapping All

Press the "Map services" button and then "Map all services". 
  
The *Map services to channels* dialog will then appear listing all available services and various 
other [mapping options.](class/service_mapper) The ticked check boxes 
[✓] indicate which services will be mapped, when you're happy with the selection press 
the "Map services" button. You will then be taken to the Service 
Mapper tab which will begin mapping services to channels. 
  
####Mapping Selected

While holding ctrl (single selection) or shift (to select a range), 
click on the services you would like to map as channels. Once you're 
done selecting, press the "Map services" button and then 
"Map selected services" - **be careful not to click on the grid or 
you'll lose your selection!**
    
The *Map services to channels* dialog will then appear listing all available services and various 
other [mapping options.](class/service_mapper) The ticked 
check boxes [✓] indicate which services will be mapped, when you're 
happy with the selection press the "Map services" button. You will 
then be taken to the Service Mapper tab which will begin mapping 
services to channels. 

**Tip**: By default Tvheadend will only show a small selection of 
available services - you can increase this by using the paging 
selector at the bottom right of the page.
  
####Mapping/Removing a Service to/from an Existing Channel

You can map/remove a service to/from an existing channel by doing the following:

**1)** Find the desired service from within the services grid. 

If you have a lot of services you may want to use filtering to limit the 
number of grid entries, you can do this by moving your mouse over the 
*Service name* column, a down arrow ▾ should now be visible, clicking 
the arrow will then display a list of options, move your mouse down to 
"Filters", a text box should then appear, click on it and enter the 
desired service's name.

!['Service filtering'](docresources/servicefilter.png)

**Tip**: Remember to remove the filter when you're finished (untick the 
check box next to the "Filters" option). 

**2)** Double click on the channel field, a drop down listing of all defined 
channels will appear, check/untick the check box next to the channel 
you'd like to associate/disassociate the service with. 

!['Add service to channel example'](docresources/addservicetochannel.png)

**3)** Press the *[Save]* button from the menu bar and you're done!

---

###Editing Services

You can edit services by highlighting (selecting) the services from 
within the grid and then clicking the *[Edit]* button from the menu bar.

!['Edit service'](docresources/serviceedit.png)

If you select more than one service from within the grid the 
*Edit service* dialog (above) will look slightly different.

!['Edit multiple services'](docresources/serviceedit2.png)

Ticking the additional check box (before each setting) will apply that 
setting to all selected services.

---

###Deleting Services

You can delete services by selecting them from within the grid, and then 
pressing the *[Delete]* button from the menu bar.

**Tip**: You can select all services within the grid by pressing ctrl+A. 
You can also ctrl+click to make additional selections, or shift+click to 
select a range. 

---
