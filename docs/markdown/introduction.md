# Introduction

Contents                                          | Description
--------------------------------------------------|--------------------------
[Overview](#overview)                             | Overview of Tvheadend
[Features](#features)                             | List of features
[Before you Begin](#before-you-begin)             | Useful information you'll need to know before using Tvheadend
[Using the Interface](#using-the-interface)       | Getting to grips with the interface
[About This Guide](#about-this-guide)             | About this guide and where to get help if you're stuck

---

## Overview

Tvheadend is a lightweight, easily-configured, general-purpose TV/video
streaming server and recorder (PVR/DVR) for GNU/Linux, FreeBSD and Android.

![Tvheadend interface](static/img/doc/introduction/screenshot.png)

It supports input from a number of sources such as DVB-T, DVB-S and more, see 
[Features](#features) for a full list.

As well as being able to record the input, Tvheadend also offers it up to
client applications via HTTP (VLC, MPlayer), HTSP (Kodi, Movian) and SAT>IP
streaming.

The code is hosted at [github](https://github.com/tvheadend/tvheadend).
Please use github's features if you want to provide patches.
Contributions and improvements are always welcome.

---

## Features

### SDTV and HDTV support
* H.265 (HEVC), H.264 (MPEG-4 AVC) and MPEG2 video supported.
* AC-3, AAC and MP2 audio supported.
* DVB subtitles supported.
* Teletext subtitles supported.

### Input Sources
* Satellite signals via DVB-S and DVB-S2.
* Terrestrial/Over-the-Air signals via DVB-T, DVB-T2 and ATSC.
* Cable signals via DVB-C.
* LAN/IPTV signals such as IPTV, SAT>IP, HDHomeRun.
* A general-purpose MPEG-TS `pipe://` (or `file://`) for analogue and 
non-broadcast sources. 
* Support for multiple adapters of any mix, with each adapter able to
receive simultaneously all programmes on the current mux.
* Powerful many-to-many channel:service:tuner mapping that allows you to select
channels irrespective of the underlying carrier (for channels that broadcast
on multiple sources).

### Output Targets
* Local or remote disk, via the built-in digital video recorder.
* HTSP (Home TV Streaming Protocol).
* HTTP streaming.
* SAT>IP server (including on-the-fly descrambling).

### Transcoding Support
* Subject to your system's capabilities, support for on-the-fly transcoding
for both live and recorded streams in various formats.

### Digital Video Recorder
* Built in video recorder stores recorded programs as Transport Stream (.ts) or Matroska (.mkv) files.
* Multiple simultaneous recordings are supported.
* All original streams (multiple audio tracks, etc) are recorded.
* Streams can be selected and filtered positively or negatively as required.
* Create rule sets manually or based on EPG queries.
* Multiple DVR profiles that support different target directories, post-processing options, filtering options, etc.

### Electronic Program Guide
* Rich EPG support, with data from DVB/OTA, XMLTV (scheduled and socket).
* Searchable and filterable from the web user interface.
* Results can be scheduled for recording with a single click.

### Rich Browser-Driven Interface
* The entire application is loaded into the browser.
* Based on extJS, all pages are dynamic and self-refreshing.
* All sorting/filtering is then done in C by the main application for speed.

### Easy to Configure and Administer
* All setup and configuration is done from the built in web user interface.
* All settings are stored in human-readable text files.
* Initial setup can be done by choosing one of the pre-defined [linuxtv](http://git.linuxtv.org/cgit.cgi/dtv-scan-tables.git) networks
or manually configured.
* Idle scanning for automatic detection of muxes and services.
* Support for broadcaster (primarily DVB-S) bouquets for easy channel mapping.

### Multi-User Support
* Access to system features (streaming, administration, configurations) can
be configured based on username/password and/or IP address.

### Software-Based CSA Descrambling
* Requires a card server (newcamd and capmt protocol is supported).

### Fully-Integrated with Mainstream Media Players
* Movian and Kodi are the main targets.
* All channel data, channel groups/tags, EPG and TV streaming is carried over a single TCP connection.

### Mobile/Remote Client Support
* As well as the web interface, which is accessible through VPN if required,
third-party clients are available for both
[Android](https://play.google.com/store/apps/details?id=org.tvheadend.tvhclient&hl=en_GB)
and [iOS](https://itunes.apple.com/gb/app/tvhclient/id638900112?mt=8) (other
clients may also be available).

### Internationalisation
* All text is encoded in UTF-8 to provide full international support.
* All major character encodings in DVB are supported (e.g. for localised EPG character sets).
* [Web interface internationalization](https://docs.tvheadend.org/documentation/development/translations)

---

## Before you Begin

There are some basic concepts that will make life much easier if you
understand them from the outset.

### Hardware/Software Fundamentals

* A **tuner** is the hardware (chipset) needed to interpret a digital
television signal and extract from it the programme stream. The tuner hardware
is also responsible for communicating with your satellite dish via the LNB
in the case of DVB-S.

* **Network tuners** are small (usually [arm](https://en.wikipedia.org/wiki/ARM_architecture) based)
computers that you connect to your network via Ethernet or Wifi, they often have a large number of tuners
and are controlled via a web interface or software. Many work out-of-the-box with Tvheadend using SAT>IP or the HDHomeRun protocols.

* A **driver** is the piece of software that your operating system uses to
talk to the tuner. This can be built into the OS (e.g. 'supported since kernel X')
or might be a separate piece of software that you need install, and maybe
even compile, separately.

* **Firmware** is a small piece of binary microcode that your system driver
sends to the tuner upon initialisation. This is the cause of more problems
than you'd imagine... if you find yourself in trouble, this is the first
thing to check along with kernel support for your hardware.

### Application/Tvheadend Fundamentals

The Tvheadend software then sets up a series of configuration elements, and
the way in which these interact determines how a TV signal ends up in front
of you. They all use what's known as a *many-to-many* relationship, in that
one configuration element can be related to multiple elements of the next
type, and vice versa: one tuner has multiple networks, one network can
exist on multiple tuners.

* The **network** is the software definition of your carrier network. Broadly,
it lays out what sort of network it is (such as DVB-T or DVB-S2), how it
gets scanned, where the DVB-S satellite is in orbit, and similar. Networks
are used by tuners so the hardware knows where to look for a signal.

* Networks then have **muxes**. These are the carrier frequencies that exist on
the old analogue channels that are used to transmit multiple digital signals
rather than a single analogue one. These signals are multiplexed together,
hence the name _mux_.

* Muxes then carry **services**. These are the individual streams of data.
They can be TV or radio programmes, they can provide data services such as
digital teletext, or they can be used as part of the control code for
catch-up IPTV services.

* And finally, services are mapped to **channels**. These are what you
and your client software think in terms of: _"I'd like to watch BBC One
now, please"_.

### Why the Complexity?

Simply, because 'BBC One' might exist in many different places... it
might have regional variations on multiple frequencies (so different services
on different muxes); it might exist on more than one source (perhaps on two
different satellites); and it might thus be accessible through more than one
piece of hardware (two satellite tuners, or one satellite and one terrestrial
tuner).

When you select the channel you want to watch or record, Tvheadend can
then map a path through all those variables to ask a particular tuner to
go and get the signal for you.

The following diagram explains the relationship between these components:

![Relationship Between Tuners, Neworks, Muxes, Services and Channels](static/img/doc/introduction/schematic.png)

---

## Using the Interface

Tvheadend is operated primarily through a tabbed web interface.

There are some basic navigation concepts that will help you get around and
make the best of it.

### Page Structure

The interface is made up of nested tabs, so similar functions are grouped
together (e.g. all configuration items at the top level, then all configuration
items for a particular topic are below that). Be aware that not all tabs are
shown by default, some are hidden depending on the current [view level](#view-level).

!["Tabs"](static/img/doc/introduction/tabbar.png)

Each tab is then laid out with a [menu bar](#menu-bar-panel-buttons) that provides access
to Add/Save/Edit-type functions, and a [grid](#grids) like a spreadsheet, a panel, or a list of (grouped) settings/options below that.

### Menu bar/Panel Buttons

!["Menubar"](static/img/doc/introduction/menubar.png)

Below is a (VERY) long list of the buttons you'll find in Tvheadend, 
They're listed here so you can quickly refer back to them at a later date (and know where to find them!).

Button                                   | Function
-----------------------------------------|-------------------
Save                                     | Save any changes made to the grid/entries/panel.
Undo                                     | Revert any changes made since the last save.
Add                                      | Display the *Add entry* dialog.
Delete/Remove                            | Delete the selected entry/entries.
Edit                                     | Edit the selected entries.
Reset All                                | Reset/clear all filters. 
                                         | **Electronic Program Guide only.**
Watch TV                                 | Launches Live TV via HTML5 video. 
                                         | **Electronic Program Guide only.**
Create Autorec                           | Creates an auto-recording rule based on the current filter criteria. 
                                         | **Electronic Program Guide only.**
Play                                     | Play the program.
                                         | **Electronic Program Guide/Broadcast details only.**
Record                                   | Record the program/event.
                                         | **Electronic Program Guide/Broadcast details only.**
Autorec/Record Series                    | Create an Auto-record entry matching the current program query. For events that have series-link information available the *Record Series* button will be shown instead.
                                         | **Electronic Program Guide/Broadcast details only.**
Force Scan                               | Forces a scan on the selected network(s) or bouquet(s).
                                         | **Configuration -> DVB Inputs -> Networks/Configuration -> Channel / EPG -> Bouquets only.**
Map Services                             | *Selected*: Map the selected services to channels. *All*: Map all services to channels. *Detach from bouquet*: Detach the (selected) services from it's bouquet (to prevent changes). 
                                         | **Configuration -> Channel / EPG -> Channels/Configuration -> DVB Inputs -> Services only.**
Maintenance                              | *Remove unseen services (PAT/SDT) (7 days+)*: Remove services marked as Missing in PAT/SDT for 7+ days. *Remove all unseen services*: Remove all services not seen for 7+ days. 
                                         | **Configuration -> DVB Inputs -> Services only.**
Number Operations                        | *Assign Number*: Assign the lowest available channel number(s) to the selected channel(s). *Number Up*: Increment the selected channel number(s) by 1. *Number Down*: Decrement the selected channel numbers by 1. *Swap Numbers*: Swap the numbers of the two selected channels. 
                                         | **Configuration -> Channel / EPG -> Channels only.**
Trigger OTA EPG Grabber                  | Force an immediate tune to the OTA EPG mux(es) to request EPG updates. 
                                         | **Configuration -> Channel / EPG -> EPG Grabber only.**
Re-run Internal EPG Grabbers             | Re-run all enabled internal grabbers. 
                                         | **Configuration -> Channel / EPG -> EPG Grabber/EPG Grabber Modules only.**
Move Up                                  | Move the selected entry up in the list. 
                                         | **Configuration -> Users -> Access entries/Configuration -> Stream -> Stream filters only.**
Move Down                                | Move the selected entry down in the list. 
                                         | **Configuration -> Users -> Access entries/Configuration -> Stream -> Stream filters only.**
Clone                                    | Clone the currently selected entry.
                                         | **Configuration -> Conditional Access (CAs)/Configuration -> Stream -> Stream/Codec Profiles only.**
Show/Hide Passwords                      | Reveal/Hide password fields. 
                                         | **Configuration -> Conditional Access (CAs) only.**
Stop                                     | Gracefully stop the selected in-progress recording entry/entries. 
                                         | **Digital Video Recorder -> Upcoming / Current Recordings only.**
Abort                                    | Abruptly stop the selected in-progress recording entry/entries. 
                                         | **Digital Video Recorder -> Upcoming / Current Recordings only.**
Download                                 | Download the selected recording. 
                                         | **Digital Video Recorder -> Finished/Failed/Removed Recordings only.**
Re-record                                | Re-schedule the selected entry/recording if possible.
                                         | **Digital Video Recorder -> Finished/Failed/Removed Recordings only.**
Move to failed                           | Move the selected recording entries to *Failed Recordings*. 
                                         | **Digital Video Recorder -> Finished Recordings only.**
Move to finished                         | Move the selected recording entries to the *Finished Recordings*. 
                                         | **Digital Video Recorder -> Failed Recordings only.**
Start wizard                             | Start the wizard. 
                                         | **Configuration -> General -> Base only.**
Clean image (icon) cache                 | Clean-up the stored image files (empty cache and re-fetch icons). 
                                         | **Configuration -> General -> Image cache only.**
Re-fetch images                          | Re-refresh image cache (reload images from upstream providers). 
                                         | **Configuration -> General -> Image cache only.**
Discover SAT\>IP servers                 | Attempt to discover more SAT>IP servers on the network. 
                                         | **Configuration -> General -> SAT>IP Server only.**
Apply configuration (run time only)      | Apply the entered debugging settings. 
                                         | **Configuration -> Debugging -> Configuration only.**
Clear all statistics                     | Reset all stream statistics, e.g. BER, PER etc.. 
                                         | **Status -> Stream only.**
Drop (displayed) connections             | Drop the currently-shown active connections.
                                         | **Status -> Connections only.**
View Level                               | Show/Hide more advanced options.
Help                                     | Display the help page.
                                         | **Icon-only buttons**
!["Right Arrow"](static/img/doc/icons/arrow_right.png) (Next)  | Display/Jump to the next associated item, channel or EPG event.
!["Left Arrow"](static/img/doc/icons/arrow_left.png) (Previous)   | Display/Jump to the previous associated item, channel or EPG event.

Some of these buttons are only displayed in selected tabs/panels (noted in bold underneath).
For items not listed above, refer to the associated Help page.

### View Level

The *View level* drop-down/button - next to the Help button -
displays/hides the more advanced features. By default it is set to Basic.
Note, depending on [configuration](class/config) the view level drop-down isn't
always visible.

!["View level"](static/img/doc/introduction/viewlevel.png)

View level            | Description
----------------------|-------------------------------------------------
**Basic**             | Display the most commonly used tabs/items.
**Advanced**          | Display the more advanced tabs/items.
**Expert**            | Show all tabs/items.

### Grids

Most configuration items - certainly the ones that are common to all types
of item covered by that tab - are in this grid. 

However, some item-specific configuration items are then only available 
through the *Add* and *Edit* dialog boxes. For example, the main 
network configuration tab grid covers parameters common to 
DVB-S, -T, -C and IPTV networks, but specific things such as FEC 
rolloff or mux URL are then only in the dialogs for networks that need 
these values.

#### Displaying and Manipulating Columns

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
  corner to indicate that a filter is active. Filters persist until
  cleared.

* **Re-arrange** the columns by simply dragging he header to a new spot.

* **Re-size** the columns by dragging the very edges of the column header as
  required.

!["Column options"](static/img/doc/introduction/columnoptions.png)

Note, a cookie is used to remember your column/filtering preferences; Clearing
your cookies will reset the interface to default.

### Split panels

The interface also makes use of split panels, examples 
being the *TV Adapters* and *EPG Grabber Modules* tabs. On the left, 
you'll see a list of items (or a tree as shown in the screenshot below), clicking 
on any of the items will display a parameter panel (on the right).

!["Split panel example"](static/img/doc/introduction/splitpanel.png)

When making changes (in the parameter panel) be sure to save **before** 
clicking on another item in the list.

### Paging Toolbar

The paging toolbar - at the bottom of most grids - offers many useful
tools.

!["Paging options"](static/img/doc/introduction/pagebar.png)

* General paging functions ```|< < > >|``` allow you to quickly move
between data sets.

* Lists the number of current and maximum (per page) rows displayed, followed by
the total number of items available.

* The *Per page* drop-down allows you to control how many rows are
displayed within the grid. By default, this is set to 50, increasing
the number of rows displayed may affect performance.

* The refresh icon allows you to refresh the currently-displayed rows.

* The *Auto refresh* check-box allows the interface to automatically
refresh the rows for you, every 30 seconds.

### Tvheadend log

The log contains information relating to the status of Tvheadend such 
as who's accessing it, what client they're using, and so on. it's mainly 
used for debugging (please see the debugging section for more), it can 
be used as a quick "is-it-working" overview too.

To open the log click the bar at the very bottom of the interface..

!["Log closed"](static/img/doc/introduction/logclosed.png)

The log will remain open for a few moments at a time, if you want to keep 
it open (or to close it), press the chevron ```▲``` (up) or ```▼``` 
(down) arrows. 

!["Log open"](static/img/doc/introduction/logopen.png)

The cog (or gear) ```⚙``` icon allows you to enable/disable a more 
verbose output.

### Adding, Editing and More

Menu bar buttons that display dialogs - certainly in the case of the 
Add and Edit buttons - show a dialog that share's a layout and buttons, 
these are explained in the table below.

Dialog button          | What it does
-----------------------|-------------------------
Cancel                 | Closes the dialog, discarding all unsaved changes.
Save                   | Saves (or add a new entry) & closes the dialog.
Apply                  | Saves pending changes but doesn't close the dialog, so you can more entries without having to fill in the fields again.
View Level             | Change the dialog view level to show/hide more advanced options.
Help                   | Display the help page.

Note, when using Save/Apply, certain fields must differ otherwise existing entries **may** be overwritten.

#### Adding/Editing

To add an entry, click the *Add* button - you should see a
dialog - you can now fill in the desired/required fields.
The entry can then be saved (*Create/Save* button), applied
(*Apply* button), or abandoned (*Cancel* button).
Note, you may need to make a pre-selection, for example, to pick a
network, a network type, or tuner.

To edit/delete etc, select the entry (or entries - see multi-select
below) and press the desired button.

#### Editing (in the Grid)

To edit a single entry, double click on the desired field/cell.
It should now be editable. Once you've made your changes you can then
save (*Save* button), or abandon (*Undo* button).
After a cell is changed, a small red flag or triangle will appear in
the top-left corner to indicate that it has been changed.
To change a check box or radio button, click once.

#### Multi-select

Rows (in the grid) are multi-selectable, so you can carry out certain
actions on more than one entry at a time. So, for example, you can select
multiple items by using ctrl+click, shift+click to select a range, or
ctrl+a to select all.
When dealing with multiple entries, an additional check-box will be
shown before each field in the dialog, remember to tick this
check-box so that the changes are applied to all (selected) entries.

!["Multi-select"](static/img/doc/introduction/multi-select.png)

---

## About This Guide

This guide is intended to give you a high-level overview of how to set
up and use Tvheadend. It does not aim to provide a complete description
of every step or answer every question: more details are available in the
Tvheadend [documentation](https://docs.tvheadend.org/documentation).

Tvheadend includes copies of many of these pages in the application, which
is easier to find when you're wondering what to do next.

If you get really stuck, there's the [forum](https://tvheadend.org/)
and IRC (*#hts* on *Libera*). If you don't have a client installed you can use the [webchat](https://web.libera.chat/?nick=tvhhelp|?#hts).
