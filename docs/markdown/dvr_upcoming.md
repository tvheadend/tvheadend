##DVR - Upcoming/Current Recordings

This tab shows your all upcoming/current recording entries.

!['Upcoming/Current Recordings' Tab](docresources/upcomingrecordings1.png)

---

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

See the [DVR Entries](class/dvrentry) page for a more detailed look at 
the various entry options.

**Tip**: You can quickly make changes to an entry by double-clicking on 
the desired field within the grid - see *Editing Fields* on the [Web interface Guide - General](webui_general) 
page for details.

###Deleting an Entry/Stopping an In-progress Recording

You can delete, stop or abort an upcoming (or an already in-progress) recording by pressing 
the *[Delete]*, *[Stop]* or *[Abort]* button from the menu bar.

* The *[Delete]* button completely removes the event and any associated file(s).
* The *[Stop]* button gracefully stops an in-progress recording and moves it to the [Finished Recordings](dvr_finished) tab.
* The *[Abort]* button stops an already in-progress recording, moves the entry to the [Failed Recordings](dvr_failed) and marks it as *Aborted by user*.  Note that this does not remove the (partial) recording file from disk.

Note that only in-progress recording can be stopped or aborted.

**Deleting an entry can't be undone, you will be prompted for confirmation.**
