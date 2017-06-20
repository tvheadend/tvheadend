This tab is where you manage your recordings. Each entry is moved 
between the *Upcoming / Current Recordings*, *Finished Recordings* and 
*Failed Recordings* sub-tabs depending on its status.

**Upcoming / Current Recordings**
: This sub-tab lists current and upcoming recording entries. Entries 
shown here are either currently recording or are soon-to-be recorded.

**Finished Recordings**
: This sub-tab lists all completed recording entries. Entries shown 
here have reached the end of the scheduled (or EITp/f defined) 
recording time.

**Failed Recordings**
: This sub-tab lists all failed recording entries. Entries shown here 
have failed to record due to one (or more) errors that occurred during 
the recording.

**Removed Recordings**
: This sub-tab lists all recording entries that have missing file(s). 
Entries shown here link to file(s) that Tvheadend cannot locate 
(files which have been externally removed).

!['Digital Video Recorder' Tabs](static/img/doc/dvrentry/tab.png)

---

### Menu Bar/Buttons

The following functions are available (tab dependant):

Button                       | Function
-----------------------------|---------
**Add**                      | Add a new (one-time-only) recording entry.
**Save**                     | Save changes made to the grid entries.
**Undo**                     | Revert all changes made to the grid entries since the last save.
**Stop**                     | Gracefully stop the selected in-progress recording entries.
**Abort**                    | Abruptly stop the selected in-progress recording entries. 
**Delete/Remove**            | Delete/Remove the selected grid entries.
**Edit**                     | Edit the selected grid entries.
**Download**                 | Download the recording.
**Re-record**                | Re-schedule the selected entry/recording if possible.
**Move to failed**           | Move the selected recording entries to the **Failed Recordings** tab.
**Move to finished**         | Move the selected recording entries to the **Finished Recordings** tab.
<tvh_include>inc/common_button_table_end</tvh_include>

---

### Entry Overview

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

### Manual Recording Entry Example

This is an example of a one-time recording entry.

!['Add new recording dialog'](static/img/doc/dvrentry/add.png)

### Adding an Entry Using the EPG

Using the Electronic Program Guide search functionality, find the 
program/event you would like to record. Click on it, then using the broadcast 
details dialog you can:

* Record the event once by pressing the *[Record program]* button.
* Automatically record all upcoming events matching the program's title by pressing the *[Autorec]* button.
* Record all upcoming series episodes by pressing the *[Record series]* button. **This replaces the *[Autorec]* button when series link information is available.**

For full instructions on how to search and record using the EPG take a 
look at the [EPG](epg) page.

### Adding an Entry Using Autorec Rules

Autorec rules allow you to match events using various options. 

* Record events using regular expressions, they can be as simple or as powerful as you like.
* Record events that broadcast between certain times or days of the week.

See [Autorec](class/dvrautorec) for more information.

---

### Downloading a Recording

Highlight (select) the desired entry, then press the *[Download]* 
button on the menu bar.

---

### Re-recording an Entry/Re-schedule a Recording

You can re-schedule an entry by pressing the *[Re-record]* button on the menu bar.

**Note**: Your EPG data must have another matching event to be able to re-schedule 
the entry.

---
