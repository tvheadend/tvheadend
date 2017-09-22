## Overview

This tab is where you manage your recordings. Each entry is moved 
between the *Upcoming / Current Recordings*, *Finished Recordings* and 
*Failed Recordings* sub-tabs depending on status. 

!['Digital Video Recorder' Tabs](static/img/doc/dvr/tab.png)

Sub-tab                                                       | Description
--------------------------------------------------------------|-------------------------------------
[Upcoming / Current Recordings](class/dvrentry)               | Lists current and upcoming recording entries. Entries shown here are either currently recording or are soon-to-be recorded.
[Finished Recordings](class/dvrentry)                         | Lists all completed recording entries. Entries shown here have reached the end of the scheduled (or EITp/f defined) recording time.
[Failed Recordings](class/dvrentry)                           | Lists all failed recording entries. Entries shown here have failed to record due to one (or more) errors that occurred during the recording.
[Removed Recordings](class/dvrentry)                          | Lists all recording entries that have missing file(s). Entries shown here link to file(s) that Tvheadend cannot locate (files which have been externally (re)moved).
[Auto-recording (Autorecs)](class/dvrautorec)                 | Lists all EPG-driven recording rules. Events matched (by an auto-record rule) will be added to the *Upcoming / Current Recordings* tab - including those currently broadcasting.
[Time-based Recording (Timers)](class/dvrtimerec)             | Lists all time-driven recording rules. Events matched (by a timer rule) will be added to the *Upcoming / Current Recordings* tab - including those currently broadcasting.

Click the title (in the table) to display all information ([items](#items) etc.) specific to that tab.

### Notes About the DVR

* Make sure you have enough tuners free to record (and watch) multiple 
services, insufficient tuners may result in missed recordings. However, 
depending on the tuner, most are able to receive a full multiplex, so 
you usually only need one per frequency/mux. You can quickly check if 
your tuner supports "full mux receive" by going to the *Muxes* tab and 
playing a freq/mux in something like VLC.

* If you're unsure as to why a scheduled recording failed, check the 
status column. Take a look at the *status* property [below](#items) for 
a list of possible reasons. 

* You can schedule a one-time only recording by pressing the [Add] 
button from the menu bar in the *Upcoming / Current Recordings* tab. 
You must enter a title, a start (and an end) time, and pick a channel.

* The time-based "Timers" tab allows you to schedule multiple recordings 
based on time and/or day.

* The *Auto-recording (Autorecs)* tab offers many powerful features, 
regular expressions for title, synopsis and description event matching, 
duplicate episode handling, time-frame based rules and much more!

* In the *Upcoming / Current Recordings* tab, duplicates are shown with 
a line-through.
























