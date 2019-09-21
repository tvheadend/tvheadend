## URL syntax

Please, add `http://IP:Port` to complete the URL.

### /play/REMAIN

Return the playlist in *xspf* or *m3u* format. If the agent is in the list
of direct agents (like wget/curl/vlc), the stream is returned instead.

The remain part can be any URL starting with /stream .

Option   | Explanation
---------|------------------------------------------
playlist | Playlist type, can be *xspf* or *m3u*

**Example:** `http://127.0.0.1:9981/play/stream/channelname/Life?playlist=xspf`

### /playlist[/AUTH][/TYPE][/WHAT][/IDENTIFIER]

Return the m3u playlist in Enigma2 format. By default (if the rest of path
is ommitted), an redirection answer will be sent where /channels remainder
is used.

TYPE          | Playlist type
--------------|------------------------------------------------------------
*empty*       | default - HTTP authentication
ticket        | temporary ticket valid for 5 minutes
auth          | pernament code which must be enabled in the password table

TYPE          | Playlist type
--------------|------------------------------------------------------------
*empty*       | M3U
e2            | Enigma2
satip         | M3U using SAT>IP extensions

WHAT          | Playlist contents
--------------|------------------------------------------------------------
channels      | All channels
tags          | All tags, for Enigma2 - tags are converted to labels
recordings    | All recordings
channelnumber | One channel specified by channel number
channelname   | One channel specified by channel name
channelid     | One channel specified by short channel ID
tag           | Tagged channels specified by UUID or tag name
tagname       | Tagged channels specified by tag name
tagid         | Tagged channels specified by short tag ID
dvrid         | One DVR record specified by short DVR ID

Option   | Explanation
---------|------------------------------------------------------------------------------
profile  | Override streaming profile, otherwise the default profile for the user is used.
sort     | Sorting method

Sorting method | Scope    | Description
---------------|----------|-----------------------------------------------------------
numname        | channel  | Channel number as first key, channel name as second key
name           | channel  | Channel name only
idxname        | tag      | Tag index as first key, tag name as second key
name           | tag      | Tag name only


### /stream/WHAT/IDENTIFIER

This URL scheme is used for streaming. The stream contents depends on the
streaming profile. It might be MPEG-TS, Matroska or MP4.

WHAT          | Stream for
--------------|------------------------------------------------------------
channelnumber | Channel specified by channel number
channelname   | Channel specified by channel name
channel       | Channel specified by channel UUID
channelid     | Channel specified by short channel ID
service       | Service specified by service UUID
mux           | Mux specified by mux UUID

Option     | Explanation
-----------|------------------------------------------------------------------------------
profile    | (except /mux) Override streaming profile
weight     | Override subscription weight
qsize      | Override queue size in bytes (default value is 1500000 for channel/service, 10000000 for mux)
descramble | (/service only) do not descramble (if set to 0)
emm        | (/service only) pass EMM to the stream (if set to 1)
pids       | (/mux only) list of subscribed PIDs (comma separated)

### /xmltv[/WHAT][/IDENTIFIER]

Return the XMLTV EPG export. By default (if the rest of path
is ommitted), an redirection answer will be sent where /channels remainder
is used.

WHAT          | Playlist contents
--------------|------------------------------------------------------------
channels      | All channels
channelnumber | One channel specified by channel number
channelname   | One channel specified by channel name
channelid     | One channel specified by short channel ID
tag           | Tagged channels specified by UUID or tag name
tagname       | Tagged channels specified by tag name
tagid         | Tagged channels specified by short tag ID

Option     | Explanation
-----------|------------------------------------------------------------------------------
lcn        | Use _lcn_ tag instead _display-name_ (standard) for the channel number

### /special/srvid2

Copy this contents to your oscam.srvid2 and start/restart
the server.