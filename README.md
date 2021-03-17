Tvheadend
========================================
(c) 2006 - 2021 Tvheadend Foundation CIC

Status
------

[![Build Status](https://travis-ci.org/tvheadend/tvheadend.svg?branch=master)](https://travis-ci.org/tvheadend/tvheadend) 
[![Coverity Scan](https://scan.coverity.com/projects/2114/badge.svg)](https://scan.coverity.com/projects/2114) 
[![Releases](https://img.shields.io/github/tag/tvheadend/tvheadend.svg?style=flat-square)](https://github.com/tvheadend/tvheadend/releases) 
[![License](https://img.shields.io/badge/license-GPLv3-blue)](./LICENSE.md) 
[![GitHub Activity](https://img.shields.io/github/commit-activity/y/tvheadend/tvheadend.svg?label=commits)] 
[![GitHub Release][releases-shield]][releases-link] 
[![GitHub Release Date][release-date-shield]][releases-link] 
[![GitHub Releases][latest-download-shield]][traffic-link] 
[![GitHub Releases][total-download-shield]][traffic-link] 
[![GitHub Activity][activity-shield]][activity-link] 
[![Open bugs][bugs-shield]][bugs-link] 
[![Open enhancements][enhancements-shield]][enhancement-link]
[![Project Maintenance][maintenance-shield]] 

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

It supports the following outputs:

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

Packages
--------

Install instructions for various distributions can be found at the [Wiki](https://tvheadend.org/projects/tvheadend/wiki/Download).

Further information
-------------------

For more information about building, including generating packages, please visit:
* https://tvheadend.org/projects/tvheadend/wiki/Building
* https://tvheadend.org/projects/tvheadend/wiki/Packaging
* https://tvheadend.org/projects/tvheadend/wiki/Git
* https://tvheadend.org/projects/tvheadend/wiki/Internationalization
