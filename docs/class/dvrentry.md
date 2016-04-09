DVR entries are used by Tvheadend to keep track of upcoming, finished and failed recordings.

!['Digital Video Recorder' Tabs](docresources/configdvrtabs4.png)

  * Upcoming and currently recording entries remain in the [Upcoming/Current Recordings](dvr_upcoming) tab.
 
  * When a recording completes successfully the entry is moved to the [Finished Recordings](dvr_finished) tab.

  * When a recording fails or gets aborted the entry is moved to the [Failed Recordings](dvr_failed) tab.
  
  
**Note**: Some entry details are not available/incomplete until the recording 
completes or fails, e.g. filesize, total data errors, etc.

---
  
**Details** : Gives a quick overview as to the status of each entry.

Icon                                       | Description
-------------------------------------------|-------------
![Clock icon](icons/scheduled.png)         | the program is scheduled (upcoming)
![Recording icon](icons/rec.png)           | recording of the program is active and underway (current)
![Information icon](icons/information.png) | click to display detailed information about the selected recording
![Exclamation icon](icons/exclamation.png) | the program failed to record
![Accept icon](icons/accept.png)           | the program recorded successfully

