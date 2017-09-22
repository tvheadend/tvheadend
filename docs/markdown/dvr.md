# Digital Video Recorder

<tvh_include>inc/dvr_contents</tvh_include>

---

<tvh_include>inc/dvr_overview</tvh_include>

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Display Only Grid Items

The following items are display only - they are not part of the entry.

**Details**:
Gives a quick overview as to the status of each entry:

Icon                                                      | Description
----------------------------------------------------------|-------------
![Clock icon](static/img/doc/icons/scheduled.png)         | The program is scheduled (upcoming).
![Recording icon](static/img/doc/icons/rec.png)           | Recording of the program is active and underway (current).
![Information icon](static/img/doc/icons/information.png) | Click to display detailed information about the selected recording.
![Exclamation icon](static/img/doc/icons/exclamation.png) | The program failed to record.
![Accept icon](static/img/doc/icons/accept.png)           | The program recorded successfully.

**Play** / ![Play icon](static/img/doc/icons/control_play.png)
: 
Play the selected entry. Downloads an m3u, or opens the m3u in your default media player. **Finished/Failed Recordings tabs only.**

Note, the links don't point to a stream but to an m3u playlist, for 
use with media players such as VLC. If you'd prefer to receive the raw 
stream instead, you can do so by removing the `/play/` path from 
the URL - see [URL Syntax](url) for more info.

---

<tvh_class_items>dvrentry</tvh_class_items>
