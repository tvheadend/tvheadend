##General Overview of Web Interface

Tvheadend is operated primarily through a tabbed web interface. 

There are some basic navigation concepts that will help you get around and
make the best of it.

###Page Structure

The interface is made up of nested tabs, so similar functions are grouped
together (e.g. all configuration items at the top level, then all configuration
items for a particular topic are below that). However, be aware that not all tabs are 
shown by default, some tabs are hidden depending on the current [view level](#view-level).

Each tab is then typically laid out with a menu bar that provides access 
to Add/Save/Edit-type functions, and a grid like a spreadsheet below that.
The grid items are frequently editable.

Most configuration items - certainly the ones that are common to all types
of item covered by that tab - are in this grid. However, some item-specific
configuration items are then only available through the *Add* and *Edit*
dialog boxes. For example, the main network configuration tab grid covers
parameters common to DVB-S, -T, -C and IPTV networks, but specific things
such as FEC rolloff or mux URL are then only in the dialogs for networks
that need these values.

####View level

The *View level* drop-down/button - next to the Help button, 
displays/hides the more advanced features. By default it is set to Basic.

View level            | Description
----------------------|-------------------------------------------------
**Basic**             | Display the most commonly used tabs/items.
**Advanced**          | Display the more advanced tabs/items.
**Expert**            | Show all tabs/items.

Depending on configuration, the view-level drop-down is not always visible.

###Displaying and Manipulating Columns

* Not all columns are necessarily visible. If you hover your mouse over a
  column heading, you'll see a down arrow - click here, and a drop-down menu
  will appear to give you access to **which columns are shown and which are not**.
  
* The same drop-down menu gives you access to a **sort** function if defined
  (it doesn't always make sense to have a sortable column for some parameters).
  You can also sort a column by simply clicking on the column header; reverse
  the sort order by clicking again.

* And the same drop-down menu also gives you access to a **filter** function
  if defined. The filter does simple pattern-matching on any string you
  provide. A small blue flag or triangle will appear in the top-left 
  corner to indicate that a filter is active.
  
* **Re-arrange** the columns by simply dragging he header to a new spot.

* **Re-size** the columns by dragging the very edges of the column header as
  required. 
  
* A cookie is used to remember your column/filtering preferences. Clearing
  your cookies will reset the interface to default.

###Adding, Editing and More

* Rows (in the grid) are multi-selectable, so you can carry out certain actions on
  more than one entry at the same time. So, for example, you can select
  multiple items by using ctrl+click on each entry or click, 
  shift+click to select a range, or ctrl+A to select all.

* To add an entry, click the *Add* button from the menu bar. You'll then 
  see a dialog, or in some cases (where a list/split panel is used), a 
  parameter panel. You can now fill in the desired/required fields, the 
  entry can then be saved (*Create/Save* button), applied (*Apply* button), 
  or abandoned (*Cancel button).
  
* To edit a single entry in the grid, double click on the desired field/cell. 
  It should now be editable. Once you've made your changes you can then 
  save (*Save* button), apply (*Apply* button), or abandon (*Cancel* button) 
  them. 
  
  After a cell is changed, a small red flag or triangle will appear in 
  the top-left corner to indicate that it has been changed.
  
  To change a check box or radio button, click once.

* To edit multiple entries, select the desired entries (as explained above), and 
  then press the *Edit* button - a dialog will be displayed. You can now make 
  your changes to each field. These changes can then be kept (*Save* button), 
  applied (*Apply* button), or abandoned (*Undo* button). Remember to tick the 
  check box before each field when saving/applying, so that the changes are applied 
  to all selected entries.
  
* To delete entries, simply select the entry/entries and press the *Delete* button.
  You will be prompted to confirm deletion.
