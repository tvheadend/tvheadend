<tvh_include>inc/stream_contents</tvh_include>

---

## Overview

Stream Profiles are the settings for output formats. These are used for Live TV
streaming and recordings. The profiles can be assigned through Access Entries,
DVR Profiles or as parameter for HTTP Streaming.

!['Stream Profiles'](static/img/doc/stream/stream_profiles_tab.png)

Type                                               | Description 
-------------------------------------------------------------------|-------------
**Built-in**                                                       | **These profiles are always available.**
[HTSP Profile](class/profile-htsp)                                 | The HTSP profile, generally used with HTSP clients such as Kodi and Movian.
[MPEG-TS Pass-thru Profile](class/profile-mpegts)                  | MPEG-TS pass-thru, this is a simple profile that just passes on the data received, can be configured to remove unneeded data packets.
[MPEG-TS Spawn](class/profile-mpegts-spawn)                        | Pipe stream out to script/binary for transcoding. Spawned script/binary must pipe the output back in as MPEG-TS.
[Matroska Profile](class/profile-matroska)                         | A general Matroska container profile.
[Audio Profile](class/profile-audio)                               | An audio-only profile.
**FFMPEG**                                                         | **The following profiles (and their help docs) require Tvheadend to be built with transcoding/ffmpeg enabled.**
[MPEG-TS/libav Profile](class/profile-libav-mpegts)                | MPEG-TS profile.
[Matroska/libav Profile](class/profile-libav-matroska)             | Matroska profile.
[MP4/libav Profile](class/profile-libav-mp4)                       | MP4 profile.
[Transcode Profile](class/profile-transcode)                       | General avlib profile.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---
