Tvheadend (TV streaming server)
====================================
(c) 2006 - 2013 Andreas Öman, et al.

How to build for Linux
----------------------

First you need to configure:

	$ ./configure

If any dependencies are missing the configure script will complain or attempt
to disable optional features.

Build the binary:

	$ make

After build, the binary resides in `build.linux/`.

Thus, to start it, just type:

	$ ./build.linux/tvheadend

Settings are stored in `$HOME/.hts/tvheadend`.

Further information
-------------------

For more information about building, including generating packages please visit:
> https://www.lonelycoder.com/redmine/projects/tvheadend/wiki/Building
> https://www.lonelycoder.com/redmine/projects/tvheadend/wiki/Packaging
> https://www.lonelycoder.com/redmine/projects/tvheadend/wiki/Git
