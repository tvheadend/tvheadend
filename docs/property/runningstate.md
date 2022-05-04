:

EITp/f (Event Information Table present/following) is broadcast 
alongside EPG data, it allows broadcasters to tell DVRs/STBs when a 
program starts, pauses or finishes. 

It is recommended that you only enable this option if you're absolutely 
sure the flags are sent correctly and on time. Incorrect EITp/f flags 
can result in failed/broken recordings.
You can set this option per [channel](class/channel) or per 
[DVR profile](class/dvrconfig), 
enabling/disabling per channel overrides the DVR profile setting.

Per Channel Option    | Description
----------------------|------------
**Not set**           | Use DVR profile setting.
**Enabled**           | Enable running state (EITp/f) detection.
**Disabled**          | Don't use running state (EITp/f) detection.
