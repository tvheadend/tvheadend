<tvh_include>inc/stream_contents</tvh_include>

---

## Overview

This tab allows you to define rules that filter and order various 
elementary streams. 

!['Stream filters'](static/img/doc/stream/stream_filters_tab.png)

Filter type                                            | Description
-------------------------------------------------------|-----------------------
[Video Stream Filters](class/esfilter_video)           | Video stream filter.
[Audio Stream Filters](class/esfilter_audio)           | Audio stream filter.
[Teletext Stream Filters](class/esfilter_teletext)     | Teletext stream filter.
[Subtitle Stream Filters](class/esfilter_subtit)       | Subtitle stream filter.
[CA Stream Filters](class/esfilter_ca)                 | Conditional Access (CA) stream filter.
[Other Stream Filters](class/esfilter_other)           | Other stream filter.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Filter Basics

* Each rule is executed in sequence (as displayed in the grid). 
* If a rule removes a stream, it will not be available to other rules
unless explicitly added back in (by another rule).
* Elementary streams not marked IGNORE, USE or EXCLUSIVE will not be 
filtered out.
* Rules with fields not defined (or set to *ANY*) will apply to ALL 
elementary streams. For example, not defining/selecting *ANY* for 
the *Language* field will apply the filter to all streams available/not 
already filtered out by another rule.
* USE / EMPTY rules have precedence against IGNORE (if the stream is 
already selected - it cannot be ignored).

### Visual Verification of Filtering

For visual verification of filtering, there is the service 
info dialog in the [Services](class/mpegts_service) tab. 
This dialog shows the received PIDs and filtered PIDs in one window.

### Filtering out a Stream

!['Removing a stream'](static/img/doc/stream/stream_filter_example.png)

Here we're removing the Bulgarian language audio from the 
input (first rule). However, if Bulgarian is the only language 
available add it back in as a last resort (second rule).

### Ignoring Unknown Streams

If you'd like to ignore unknown elementary streams, add a rule to the 
end of grid with the *ANY* (not defined) comparison(s) and the 
action set to *IGNORE*.

---
