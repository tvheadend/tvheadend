##Digital Video Recorder - Upcoming/Current Recordings

This tab shows your all upcoming/current recordings.

!['Upcoming/Current Recordings' Tab](docresources/upcomingrecordings1.png)

---

###Adding and Editing an Entry 

####Adding an Entry Using the EPG

Using the Electronic Program Guide search functionality, find the 
program/event you would like to record. For full instructions on how to 
search the EPG, take a look at the [EPG](epg) page.

Once you've found the program you'd like to record, click on it, then 
using the resultant dialog you can:

* Record it once by pressing the *[Record program]* button.
* Automatically record all upcoming events matching the program's title by pressing the *[Autorec]* button.
* Record all upcoming series episodes by pressing the *[Record series]* button. **This replaces the *[Autorec]* button when series link information is available.**

**Tip**: You can quickly find all matching programs by clicking on the title.

####Adding an Entry Using Autorec Rules

Autorec rules allow you to match events using various options. 

* Record events using regular expressions, they can be as simple or as powerful as you like.
* Record events that broadcast between certain times or days of the week.

Please see [Autorec](dvr_autorec) for more information.

####Manual Event Entry/Editing an Entry

Use the *[Add]* button to manually to add an entry. To edit an entry, 
highlight the desired entry within the grid and then press the *[Edit]* button.

A common set of fields is used for the _Add_ or _Edit_ dialogs, most
of which can also be seen in the grid view:

![Add/Edit Upcoming Recording](docresources/upcomingrecordings3.png)

See the [DVR Entries](class/dvrentry) page for a more detailed look at 
the various entry options.

###Deleting an Entry

You can delete or abort an upcoming entry (or an already in-progress recording) by pressing 
the *[Delete]* or *[Abort]* buttons.

* The *[Delete]* button completely removes the event and any associated files.
* The *[Abort]* button stops an already in-progress recording, moves the entry to the [Failed Recordings](dvr_failed) and marks it as *Aborted by user*.  Note that this does not remove the (partial) recording file from disk.

**Deleting or aborting an entry cannot be undone, you will be prompted for confirmation.**
