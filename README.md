Tvheadend
========================================
(c) 2006 - 2015 Tvheadend Foundation CIC


What it is
----------

Tvheadend is a TV streaming server and digital video recorder.

It supports the following inputs:

  * DVB-C(2)
  * DVB-T(2)
  * DVB-S(2)
  * ATSC
  * SAT>IP
  * HDHomeRun
  * IPTV
    * UDP
    * HTTP

It support the following outputs:

  * HTTP
  * HTSP (own protocol)
  * SAT>IP

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

How to build for OS X
---------------------

Same build procedure applies to OS X.
After build, the binary resides in `build.darwin` directory.

Only network sources (IPTV, SAT>IP) are supported on OS X.
There is no support for DVB USB sticks and PCI cards.
Transcoding is currently not supported.

Further information
-------------------

For more information about building, including generating packages, please visit:
> https://tvheadend.org/projects/tvheadend/wiki/Building  
> https://tvheadend.org/projects/tvheadend/wiki/Packaging  
> https://tvheadend.org/projects/tvheadend/wiki/Git
> https://tvheadend.org/projects/tvheadend/wiki/Internationalization
