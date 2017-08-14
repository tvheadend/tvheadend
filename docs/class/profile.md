Stream Profiles are the settings for output formats. These are used for Live TV
streaming and recordings. The profiles are assigned through 
the [Access Entries](class/access),
DVR Profiles or as parameter for HTTP Streaming.

!['Stream Profiles'](static/img/doc/profile/tab.png)

* Types
 * Built-in
   - [HTSP Profile](class/profile-htsp)
   - [MPEG-TS Pass-thru Profile](class/profile-mpegts)
   - [Matroska Profile](class/profile-matroska)

 * Requires Tvheadend to be built with transcoding/ffmpeg enabled.
   - [MPEG-TS/libav Profile](class/profile-libav-mpegts)
   - [Matroska/libav Profile](class/profile-libav-matroska)
   - [MP4/libav Profile](class/profile-libav-mp4)
   - [Transcode Profile](class/profile-transcode)
   
If you do not have a build of Tvheadend with transcoding enabled 
some of the above profiles (and their associated Help pages) will not 
be available.

---

### Menu Bar/Buttons

The following functions are available:

Button              | Function
--------------------|---------
**Save**            | Save any changes made to the selected configuration.
**Undo**            | Undo any changes made to the selected configuration since the last save.
**Add**             | Add a new profile.
**Delete**          | Delete the selected entry.
**Clone**           | Clone the currently selected profile.

<tvh_include>inc/common_button_table_end</tvh_include>

---
