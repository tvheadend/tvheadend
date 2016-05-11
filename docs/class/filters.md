This tab defines rules to filter and order the elementary streams 
(PIDs) like video or audio from the input feed. The execution order of 
commands is granted. It means that first rule is executed for all 
available streams then second and so on.

!['Stream filters'](static/img/doc/streamfiltertab.png)

If any elementary stream is not marked as ignored or exclusive, it is 
used. If you like to ignore unknown elementary streams, add a rule to 
the end of grid with the any (not defined) comparisons and with the 
action ignore.

The rules for different elementary stream groups (video, audio,
teletext, subtitle, CA, other) are executed separately (as visually edited).

For the visual verification of the filtering, there is a service 
info dialog in the [Services](class/mpegts_service) tab. 
This dialog shows the received PIDs and filtered PIDs in one window.

---
<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

Tab specific functions:

Button                 | Function
-----------------------|---------
**Move Up**            | Move the selected entry up the grid.
**Move Down**          | Move the selected entry down the grid. 

---

<tvh_include>inc/add_grid_entry</tvh_include>

!['Stream filter dialog'](static/img/doc/addfilterdialog.png)

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

<tvh_include>inc/del_grid_entries</tvh_include>

---
