DVR entries are how Tvheadend manages upcoming/current, finished and 
failed recordings.

!['Digital Video Recorder' Tabs](static/img/doc/configdvrtabs4.png)

Each entry is moved between the tabs depending on its state:

* Upcoming and currently recording entries remain in 
the *Upcoming/Current Recordings* tab.
* When a recording completes successfully the entry is moved to 
the *Finished Recordings* tab.
* When a recording fails (or is aborted) the entry is moved to 
the *Failed Recordings* tab.

Please note that the grid in each tab may have different columns and 
not all entry information is available until it completes or fails, 
e.g filesize, total data errors, etc.

---

###Menu Bar/Buttons

The following functions are available:

Button                       | Function
-----------------------------|---------
**Delete**                   | Delete the selected grid entries.
**Edit**                     | Edit the selected grid entries.
<tvh_include>inc/common_button_table_end</tvh_include>

The following functions are only available in the 
**Upcoming/Current Recordings** tab.

Button                       | Function
-----------------------------|---------
**Add**                      | Add a new (one-time-only) recording entry.
**Save**                     | Save changes made to the grid entries.
**Undo**                     | Revert all changes made to the grid entries since the last save.
**Stop**                     | Gracefully stop the selected in-progress recording entries.
**Abort**                    | Abruptly stop the selected in-progress recording entries. 

The following functions are only available in the **Finished Recordings** 
and **Failed Recordings** tabs:

Button                       | Function
-----------------------------|---------
**Download**                 | Download the recording.
**Re-record**                | Re-schedule the selected entry/recording if possible.

The following functions are only available in the **Finished Recordings** 
tab.

Button                       | Function
-----------------------------|---------
**Move to failed**           | Move the selected recording entries to the **Failed Recordings** tab.

The following functions are only available in the **Failed Recordings** 
tab.

Button                       | Function
-----------------------------|---------
**Move to finished**         | Move the selected recording entries to the *Finished Recordings* tab.

---

###Entry Overview

The *Details* column gives a quick overview as to the status of each 
entry:

Icon                                       | Description
-------------------------------------------|-------------
![Clock icon](icons/scheduled.png)         | the program is scheduled (upcoming)
![Recording icon](icons/rec.png)           | recording of the program is active and underway (current)
![Information icon](icons/information.png) | click to display detailed information about the selected recording
![Exclamation icon](icons/exclamation.png) | the program failed to record
![Accept icon](icons/accept.png)           | the program recorded successfully

---

<tvh_include>inc/add_grid_entry</tvh_include>

Note that the *[Add]* functionality is only available in 
the *Upcoming/Current Recordings* tab. 

####Manual Recording Entry Example

This is an example of a one-time recording entry:

!['Add new recording dialog'](static/img/doc/addnewrecentry.png)

####Adding an Entry Using the EPG

Using the Electronic Program Guide search functionality, find the 
program/event you would like to record. Click on it, then using the broadcast 
details dialog you can:

* Record the event once by pressing the *[Record program]* button.
* Automatically record all upcoming events matching the program's title by pressing the *[Autorec]* button.
* Record all upcoming series episodes by pressing the *[Record series]* button. **This replaces the *[Autorec]* button when series link information is available.**

For full instructions on how to search and record using the EPG take a 
look at the [EPG](epg) page.

####Adding an Entry Using Autorec Rules

Autorec rules allow you to match events using various options. 

* Record events using regular expressions, they can be as simple or as powerful as you like.
* Record events that broadcast between certain times or days of the week.

See [Autorec](class/dvrautorec) for more information.

---

<tvh_include>inc/edit_grid_entries</tvh_include>

---

<tvh_include>inc/del_grid_entries</tvh_include>

---

###Playing a Recording

You can play a recording by clicking the *Play* icon.
This will automatically launch an appropriate player, otherwise you will
need to manually open the playlist to start watching (normally a
double-click on the downloaded file).

Note that these are not links to an actual file but to a playlist.

---

###Downloading a Recording

Highlight (select) the desired entry, then press the *[Download]* 
button on the menu bar.

---

###Re-recording an Entry/Re-schedule a Recording

You can re-schedule an entry by pressing the *[Re-record]* button on the menu bar.

**Note**: Your EPG data must have another matching event to be able to re-schedule 
the entry.

---
