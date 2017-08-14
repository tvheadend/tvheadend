This tab allows you to define rules that filter and order various 
elementary streams. 

!['Stream filters'](static/img/doc/filters/tab.png)

---

<tvh_include>inc/common_button_table_start</tvh_include>

<tvh_include>inc/common_button_table_end</tvh_include>

Tab specific functions:

Button                 | Function
-----------------------|---------
**Move Up**            | Move the selected entry up the grid.
**Move Down**          | Move the selected entry down the grid. 

---

### Filter Basics

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

---

### Visual Verification of Filtering

For visual verification of filtering, there is the service 
info dialog in the [Services](class/mpegts_service) tab. 
This dialog shows the received PIDs and filtered PIDs in one window.

---

### Filtering out a Stream

!['Removing a stream'](static/img/doc/filters/example.png)

Here we're removing the Bulgarian language audio from the 
input (first rule). However, if Bulgarian is the only language 
available add it back in as a last resort (second rule).

### Ignoring Unknown Streams

If you'd like to ignore unknown elementary streams, add a rule to the 
end of grid with the *ANY* (not defined) comparison(s) and the 
action set to *IGNORE*.

---
