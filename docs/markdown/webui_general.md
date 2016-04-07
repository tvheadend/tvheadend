##General Overview of Web Interface

Tvheadend is operated primarily through a tabbed web interface. 

There are some basic navigation concepts that will help you get around and
make the best of it.

####Page Structure

The interface is made up of nested tabs, so similar functions are grouped
together (e.g. all configuration items at the top level, then all configuration
items for a particular topic are below that).

Each tab is then typically laid out with a menu bar across the top that
provides access to Add/Save/Edit-type functions, and a grid like a spreadsheet
below that. The grid items are frequently editable.

Most configuration items - certainly the ones that are common to all types
of item covered by that tab - are in this grid. However, some item-specific
configuration items are then only available through the *Add* and *Edit*
dialog boxes. For example, the main network configuration tab grid covers
parameters common to DVB-S, -T, -C and IPTV networks, but specific things
such as FEC rolloff or mux URL are then only in the dialogs for networks
that need these values.

####Displaying and Manipulating Columns

* Not all columns are necessarily visible. If you hover your mouse over a
  column heading, you'll see a down arrow - click here, and a drop-down menu
  will appear to give you access to **which columns are shown and which are not**.
  
* The same drop-down menu gives you access to a **sort** function if defined
  (it doesn't always make sense to have a sortable column for some parameters).
  You can also sort a column by simply clicking on the column header; reverse
  the sort order by clicking again.

* And the same drop-down menu also gives you access to a **filter** function
  if defined. The filter does simple pattern-matching on any string you
  provide.
  
* **Re-arrange** the columns by simply dragging he header to a new spot.

* **Re-size** the columns by dragging the very edges of the column header as
  required. 
  
####Editing Fields

* To edit a cell, double click on it. After a cell is changed, a small red
  flag or triangle will appear in the top-left corner to indicate that it
  has been changed. These changes can now be kept (*Save* button), or
  abandoned (*Undo* button).
  
* To change a check box or radio button, click once.

* To add a new entry, press the *Add* button. The new (empty) entry will
  be created on the server but will not be saved and will not necessarily
  be enabled. You can now change all the cells to the desired values, check
  the ‘enable’ box if applicable and then press *Save* to activate the new
  entry.

* Most rows are multi-selectable, so you can carry out certain actions on
  more than one entry at the same time. So, for example, you can select
  multiple items by using ctrl+click on each
  entry or click, shift+click to select a range.
