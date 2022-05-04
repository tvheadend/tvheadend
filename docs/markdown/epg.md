# Electronic Program Guide

Contents                                                               | Description
-----------------------------------------------------------------------|--------------------------------------------
[Overview](#overview)                                                  | EPG overview
[Filtering (or searching)](#filtering-or-searching-)                   | Filtering the EPG
[Items (grid items)](#items)                                           | EPG tab items
[Event Details and Recording](#event-details-and-recording)            | Program event details and recording
[Auto-recordings](#auto-recordings)                                    | Auto-recording
[Watching TV and Browser Codec Support](#watching-tv)                  | Watching TV and browser codec support 

---

## Overview

Tvheadend has a built-in Electronic Program Guide. The EPG is an
in-memory database populated with all the information about events
received from the DVB networks over-the-air or from external grabbers
such as XMLTV.

The EPG tab displays a filterable grid containing all events, sorted
based on start time.

!['Electronic Program Guide' Tab](static/img/doc/epg/tab.png)

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---


## Filtering (or searching) 

In the EPG top tool bar you can access five input fields. These are used
to filter/search for events. The form uses implicit AND between the
input fields. This means that all filters must match for an event to be
displayed.

Filter                     | Function
---------------------------| --------
**All/Now**                | Filter between showing all events (*All*), or those that are currently broadcasting (*Now*). Can be used with the other filters.
**Search title...**        | Only display events that match the given title.
                           | The filter uses case-insensitive regular expressions. If you don’t know what a regular expression is, this simply means that you can type just parts of the title and filter on that - there’s no need for full, exact matching. If the fulltext checkbox is checked, the title text is matched against title, subtitle, summary and description.
**Filter channel...**      | Only display events from the selected channel.
                           | Channels in the drop down are ordered by name and can be filtered (by name) by typing in the box.
**Filter tag...**          | Only display events from channels which are included in the selected tag.
                           | Tags are used for grouping channels together - such as ‘Radio’ or ‘HDTV’ - and are configured by the administrator. You can start typing a tag name to filter the list.
**Filter content type...** | Only display events that match the given content type tag.
                           | Most DVB networks classify their events into content groups. This field allows you to filter based on content type (e.g. “Sports” or “Game Show”). Supported tags are determined by your broadcaster. Again, simply start typing to filter the entries if you have a long list to choose from.
**Filter duration...**     | Only display events that fall between the given minimum and maximum durations.
                           | This allows you to filter for or against, say, a daily broadcast and a weekly omnibus edition of a program, or only look for short news bulletins and not the 24-hour rolling broadcasts.

*Title*, *Channel*, *Tag* and *Content Type* are dependent on your configuration and on what your 
broadcaster sends. Options for the *Duration* are as follows:

Filter Range           | Example Purpose
-----------------------|----------------
00:00:01 to 00:15:00   | Very short news bulletins, children's programs, etc.
00:15:01 to 00:30:00   | Short programs, e.g. daily soap operas
00:30:01 to 01:30:00   | Medium-length programs, e.g. documentaries
01:30:01 to 03:00:00   | Longer programs, e.g. films
03:00:00 to no maximum | Very long programs, e.g. major sporting events

So, if you only want to see Movies from your available HD channels, you
would select ‘HDTV’ in the *[Filter tag…]* field, and select ‘Movie /
Drama’ in the *[Filter content type…]* field. If you wish, you could
then further limit the search to programs of between 90 minutes and 3
hours by selecting ‘01:30:01 to 03:00:00’ in the *[Filter duration…]*
field.

Note that you don’t have to press a ‘Search’ button: the grid
immediately updates itself as you change the filters.

You can clear an individual filter by simply deleting its contents, or
by selecting *‘(Clear filter)’* as appropriate on all except the title
filter. If you want to clear all filters, just press the *[Reset All]*
button.

---

## Items

The main grid items have the following functions:

**Details**
: Displays the current status of a recording event for this program if 
  one applies: 

Icon                                                                  | Description
----------------------------------------------------------------------|-------------
![Clock icon](static/img/doc/icons/scheduled.png)                     | the program is scheduled for recording
![Recording icon](static/img/doc/icons/rec.png)                       | the program is currently recording
![Broadcast details icon](static/img/doc/icons/broadcast_details.png) | click to call up more detailed information about an event
![Exclamation icon](static/img/doc/icons/exclamation.png)             | the program failed to record

**Progress**
: A bar graph display of how far through a program we currently are.

**Title**
: The title of the program. *You can automatically set a filter to the
  value of this field by clicking on it (e.g. click on 'Daily News' will
  automatically filter the whole grid to only show programs with the same
  name).*

**Subtitle**
: The subtitle of the program, if gien by your EPG provider. Note that some
  (notably, UK) providers use this for a program synopsis instead of a true
  subtitle.

**Episode**
: Episode number, if given by your EPG provider.

**Start Time**
: The scheduled start time of the program.

**End Time**
: The scheduled end time of the program.

**Duration**
: The scheduled duration (i.e. start time to end time) of the program. 

**Number**
: The channel number of the broadcasting channel, if defined.

**Channel**
: The name of the broadcasting channel. *You can automatically set a filter to the
  value of this field by clicking on it (e.g. click on 'Channel 4 HD' will
  automatically filter the whole grid to only show programs from that channel).*

**Stars**
: Rating (in stars) of the program.

**Age**
: Age rating of the program.

**Content Type**
: Any content/genre information as provided by the EPG provider. *You can
  automatically set a filter to the value of this field by clicking on it
  (e.g. click on 'Movie/Drama' will automatically filter the whole grid
  to only show programs of the same type).*

---

## Event Details and Recording

If you click on a single event, a popup will display detailed
information about the event. It also allows you to schedule the event
for recording, find alternative events and more.

![EPG Detail 2](static/img/doc/epg/autorec.png)

Toolbar item                                                                          | Description
--------------------------------------------------------------------------------------|------------
Find info from ... drop-down                                                          | Query an online service for more information on an event. Opens in new window.
![Play](static/img/doc/icons/control_play.png) *[Play]*                               | Download a playlist file (XSPF or M3U depending on your startup options); if your system is configured for it, this will automatically launch an appropriate player, otherwise you will need to manually open the playlist to start watching (normally a double-click on the downloaded file).
(default DVR Profile) drop-down                                                       | Choose a specific DVR profile that will apply to the recording or autorec rule. You can define different profiles in the **Configuration -\> Recording -\> [Digital Video Recorder Profiles](class/dvrconfig)** tab. This allows you to set, for example, more post-broadcast padding for a channel that always runs late, or perhaps define a different post-processing command to strip adverts out on a commercial channel.
![Record](static/img/doc/icons/rec.png) *[Record]*                                    | Record the displayed event.
![Record Series](static/img/doc/icons/auto_rec.png) *[Record Series]* / *[Autorec]*   | **Record Series:** Series link, Record all EPG-defined episodes in the series/season. **Autorec:** Create a pseudo-series link using the autorec feature.
![Alternative showings](static/img/doc/icons/control_repeat_blue.png)                 | List/Find alternative showings (exact matches) of this event.
![Related events](static/img/doc/icons/clock.png)                                     | List/Find related EPG events. 

To close the popup, just click on the [X] window button. The popup isn’t
modal, so you don’t have to close it before doing something else, and
you can open as many detailed information popups as you want.

---

## Auto-recordings

Should you wish to record all events matching a specific query (to
record your favourite show every week, for example) you can press the
*[Create AutoRec]* button in the top toolbar.

You can change or delete the autorec rules in 
the **[Autorec](class/dvrautorec)** tab. Use that editor if you 
temporarily want to disable an autorecording or make adjustments to the 
channel, tag, or similar.

---

## Watching TV

If you want to watch live TV in the web UI, the *[Watch TV]* button will
pop up a HTML5 video player, where you can select the channel to watch and a
stream profile to use. A transcoding stream profile is required to transcode 
the stream to a format that is supported by your browser, as browsers only
support certain formats and codecs.

### Supported formats (containers)

Browser | MPEG-TS | MPEG-PS | Matroska | WebM
------- | :-----: | :-----: | :------: | :--:
Google Chrome | ![no](icons/exclamation.png) | ![no](icons/exclamation.png) | ![yes](icons/accept.png) | ![yes](icons/accept.png)
Mozilla Firefox | ![no](icons/exclamation.png) | ![no](icons/exclamation.png) |  | ![yes](icons/accept.png)

### Supported video codecs

Browser | MPEG2 Video | H.264 | VP8
------- | :---------: | :---: | :-:
Google Chrome | ![no](icons/exclamation.png) | ![yes](icons/accept.png) | ![yes](icons/accept.png)
Mozilla Firefox | ![no](icons/exclamation.png) |  | ![yes](icons/accept.png)

### Supported audio codecs

Browser | MPEG2 Audio | Dolby Digital (AC3) | AAC | Vorbis
------- | :---------: | :-----------------: | :-: | :----:
Google Chrome | ![no](icons/exclamation.png) | ![no](icons/exclamation.png) | ![yes](icons/accept.png) | ![yes](icons/accept.png)
Mozilla Firefox | ![no](icons/exclamation.png) | ![no](icons/exclamation.png) |  | ![yes](icons/accept.png)
