## Features of Tvheadend

#### SDTV and HDTV support
* H.265 (HEVC), H.264 (MPEG-4 AVC) and MPEG2 video supported.
* AC-3, AAC and MP2 audio supported.
* DVB subtitles supported.
* Teletext subtitles supported.

#### Input Sources
* Satellite signals via DVB-S and DVB-S2
* Terrestrial/Over-the-Air signals via DVB-T, DVB-T2 and ATSC
* Cable signals via DVB-C
* LAN/IPTV signals such as IPTV, SAT>IP, HDHomeRun
* A general-purpose MPEG-TS `pipe://` for analogue and non-broadcast sources
* Support for multiple adapters of any mix, with each adapter able to
receive simultaneously all programmes on the current mux.
* Powerful many-to-many channel:service:tuner mapping that allows you to select
channels irrespective of the underlying carrier (for channels that broadcast
on multiple sources). 

#### Output Targets
* Local or remote disk, via the built-in digital video recorder.
* HTSP (Home TV Streaming Protocol).
* HTTP streaming.
* SAT>IP server (including on-the-fly descrambling).

#### Transcoding Support
* Subject to your system's capabilities, support for on-the-fly transcoding
for both live and recorded streams in various formats.

#### Digital Video Recorder
* Built in video recorder stores recorded programs as Transport Stream (.ts) or Matroska (.mkv) files.
* Multiple simultaneous recordings are supported.
* All original streams (multiple audio tracks, etc) are recorded.
* Streams can be selected and filtered positively or negatively as required.
* Create rule sets manually or based on EPG queries.
* Multiple DVR profiles that support different target directories, post-processing options, filtering options, etc.

#### Electronic Program Guide
* Rich EPG support, with data from DVB/OTA, XMLTV (scheduled and socket).
* Searchable and filterable from the web user interface.
* Results can be scheduled for recording with a single click.

#### Rich Browser-Driven Interface
* The entire application is loaded into the browser.
* Based on extJS, all pages are dynamic and self-refreshing.
* All sorting/filtering is then done in C by the main application for speed.

#### Easy to Configure and Administer
* All setup and configuration is done from the built in web user interface.
* All settings are stored in human-readable text files.
* Initial setup can be done by choosing one of the pre-defined [linuxtv](http://git.linuxtv.org/cgit.cgi/dtv-scan-tables.git) networks
or manually configured.
* Idle scanning for automatic detection of muxes and services.
* Support for broadcaster (primarily DVB-S) bouquets for easy channel mapping. 

#### Multi-User Support
* Access to system features (streaming, administration, configurations) can
be configured based on username/password and/or IP address.

#### Software-Based CSA Descrambling
* Requires a card server (newcamd and capmt protocol is supported).

#### Fully-Integrated with Mainstream Media Players
* Movian and Kodi are the main targets.
* All channel data, channel groups/tags, EPG and TV streaming is carried over a single TCP connection.

#### Mobile/Remote Client Support
* As well as the web interface, which is accessible through VPN if required, 
third-party clients are available for both
[Android](https://play.google.com/store/apps/details?id=org.tvheadend.tvhclient&hl=en_GB)
and [iOS](https://itunes.apple.com/gb/app/tvhclient/id638900112?mt=8) (other
clients may also be available).

#### Internationalisation
* All text is encoded in UTF-8 to provide full international support.
* All major character encodings in DVB are supported (e.g. for localised EPG character sets).
* [Web interface internationalization](https://tvheadend.org/projects/tvheadend/wiki/Internationalization)
