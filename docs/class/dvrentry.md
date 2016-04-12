DVR entries are used by Tvheadend to keep track of upcoming/current, finished 
and failed recordings.

!['Digital Video Recorder' Tabs](docresources/configdvrtabs4.png)

  * Upcoming and currently recording entries remain in the *Upcoming/Current Recordings* tab.
  * When a recording completes successfully the entry is moved to the *Finished Recordings* tab.
  * When a recording fails or gets aborted the entry is moved to the *Failed Recordings* tab.
  
**Some entry details are not available/incomplete until the recording 
completes or fails, e.g. filesize, total data errors, etc.**

---

##Upcoming/Current Recordings

This tab shows your all upcoming/current recording entries.

!['Upcoming/Current Recordings' Tab](docresources/upcomingrecordings1.png)

###Adding and Editing an Entry 

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


####Adding a Timer Event

You can set time-based entries using the [Timer](class/dvrtimerec) tab.


####Manual Event Entry/Editing an Entry

To add an entry press the *[Add]* button from the menu bar. To edit an 
entry highlight the desired entry within the grid and 
then press the *[Edit]* button.

A common set of fields is used for the _Add_ or _Edit_ dialogs, most
of which can also be seen in the grid view:

![Add/Edit Upcoming Recording](docresources/upcomingrecordings3.png)

**Tip**: You can quickly make changes to an entry by double-clicking on 
the desired field within the grid. See *Editing Fields* on the [Web interface Guide - General](webui_general) 
page for details.


###Deleting an Entry/Stopping an In-progress Recording

You can delete, stop or abort an upcoming (or an already in-progress) recording by pressing 
the *[Delete]*, *[Stop]* or *[Abort]* button from the menu bar.

* The *[Delete]* button completely removes the event and any associated file(s).
* The *[Stop]* button gracefully stops an in-progress recording and moves it to the Finished Recordings tab.
* The *[Abort]* button stops an already in-progress recording, moves the entry to the Failed Recordings and marks it as *Aborted by user*.  Note that this does not remove the (partial) recording file from disk.

Note that only in-progress recording can be stopped or aborted.

**Deleting an entry can't be undone, you will be prompted for confirmation.**

---

##DVR - Finished Recordings

This tab shows all your finished recording entries.

!['Finished Recordings' Tab](docresources/finishedrecordings1.png)

###Playing a Recording

You can play a recording by clicking the *Play* link, 
note that these links do not link to an actual file but to a playlist.
This will automatically launch an appropriate player, otherwise you will
need to manually open the playlist to start watching (normally a
double-click on the downloaded file).

###Downloading a Recording

Highlight (select) the desired entry then press the *[Download]* button on the 
menu bar.

###Editing an Entry

You can edit an entry by clicking the *[Edit]* button on the menu bar, 
note that not all fields can be edited.

###Deleting an Entry

Highlight (select) the desired entry(s) then press the *[Delete]* button on the 
menu bar. 

**Deleting can't be undone. You 
will be prompted to confirm deletion.**

**Tip**: You can highlight multiple entries by holding ctrl or shift 
(to select a range) while clicking.



---

##DVR - Failed Recordings

This tab shows all failed recording entries.

!['Failed Recordings' Tab](docresources/failedrecordings1.png)

The *Status* column gives the reason why a recording failed. 

###Playing a Failed Recording

You can play a partial recording by clicking the *Play* link, 
note that these links do not link to an actual file but to a playlist.
This will automatically launch an appropriate player, otherwise you will
need to manually open the playlist to start watching (normally a
double-click on the downloaded file).

###Downloading a Recording

Highlight (select) the desired entry then press the *[Download]* button on the menu bar.

###Re-recording an Entry/Re-schedule a Failed Recording

You can re-schedule an entry by pressing the *[Re-record]* button on the menu bar.

**Note**: Your EPG data must have another matching event to be able to re-schedule 
the entry.

###Moving an Entry

Failed recordings can be moved to the Finished Recordings tab by 
highlighting (selecting) the desired entry and then pressing the *[Move to finished]*
button from the menu bar.

###Deleting an Entry

Highlight (select) the desired entry(s) then press the *[Delete]* button on the menu bar. 

**Deleting can't be undone. You will be prompted to confirm deletion.**

**Tip**: You can highlight multiple entries by holding ctrl or shift 
(to select a range) while clicking.

---
  
**Details** : Gives a quick overview as to the status of each entry.

Icon                                       | Description
-------------------------------------------|-------------
![Clock icon](icons/scheduled.png)         | the program is scheduled (upcoming)
![Recording icon](icons/rec.png)           | recording of the program is active and underway (current)
![Information icon](icons/information.png) | click to display detailed information about the selected recording
![Exclamation icon](icons/exclamation.png) | the program failed to record
![Accept icon](icons/accept.png)           | the program recorded successfully

