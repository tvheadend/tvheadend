

Tvheadend
====================================
(c) 2006 - 2013 Andreas Öman, et al.


What it is
----------

Tvheadend is a lightly TV streaming server and digital video recorder supporting DVB-S, DVB-S2, DVB-C, DVB-T, DVB-T2, ATSC, IPTV, and Analog video (V4L) as input sources.


How to build for Linux
----------------------

First, you need to configure:

	$ ./configure

If any dependencies are missing the configure script will complain or attempt to disable optional features.

You can manually enable or disable optional features using command:

+ for enable: `--enable-<feature>`
+ for disable: `--disable-<feature>`

Where `<feature>` could be:
+ `cwc` (default enabled)
+ `v4l` (default enabled)
+ `linuxdvb` (default enabled)
+ `dvbscan` (default enabled)
+ `timeshift` (default enabled)
+ `imagecache` (default auto)
+ `avahi` (default auto)
+ `zlib` (default auto)
+ `libav` (default auto)
+ `inotify` (default auto)
+ `bundle` (default disabled)
+ `dvbcsa` (default disabled)

You must append all you want apply to configure command. Syntax is:

	$ ./configure [option1] [option2] [option3] ...
  
> Note: On ARM machines, at least, might be useful enable **dvbcsa** to improve stream decoding performances.


Now, build the binary:

	$ make

After build, the binary resides in **build.linux/**


Thus, to start the binary just type:

	$ ./build.linux/tvheadend

Settings are stored in **$HOME/.hts/tvheadend**


Further information
-------------------

For more information about building, including generating packages please visit:
> https://www.lonelycoder.com/redmine/projects/tvheadend/wiki/Building
