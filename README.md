Tvheadend
====================================
(c) 2006 - 2013 Andreas Öman, et al.


What it is
----------

Tvheadend is a TV streaming server and digital video recorder, supporting DVB-S, DVB-S2, DVB-C, DVB-T, DVB-T2, ATSC, IPTV, and Analog video (V4L) as input sources.


How to build for Linux
----------------------

First you need to configure:

	$ ./configure

If any dependencies are missing the configure script will complain or attempt
to disable optional features.

Build the binary:

	$ make

After build, the binary resides in `build.linux` directory.

Thus, to start it, just type:

	$ ./build.linux/tvheadend

Settings are stored in `$HOME/.hts/tvheadend`.


Further information
-------------------

For more information about building, including generating packages, please visit:
> https://tvheadend.org/projects/tvheadend/wiki/Building  
> https://tvheadend.org/projects/tvheadend/wiki/Packaging  
> https://tvheadend.org/projects/tvheadend/wiki/Git
