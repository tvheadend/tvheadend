<div align="center">

[![Build Status](https://github.com/tvheadend/tvheadend/actions/workflows/build-cloudsmith.yml/badge.svg?branch=master)](https://github.com/tvheadend/tvheadend/actions/workflows/build-cloudsmith.yml)
[![Coverity Scan](https://scan.coverity.com/projects/2114/badge.svg)](https://scan.coverity.com/projects/2114)
[![Github last commit](https://img.shields.io/github/last-commit/tvheadend/tvheadend)](https://github.com/tvheadend/tvheadend)

[![Releases](https://img.shields.io/github/tag/tvheadend/tvheadend.svg?style=flat-square)](https://github.com/tvheadend/tvheadend/releases)
[![License](https://img.shields.io/badge/license-GPLv3-blue)](./LICENSE.md) 
[![GitHub Activity](https://img.shields.io/github/commit-activity/y/tvheadend/tvheadend.svg?label=commits)](https://github.com/tvheadend/tvheadend/commits)

[![Hosted By: Cloudsmith](https://img.shields.io/badge/Packages%20hosted%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.io/~tvheadend/repos/tvheadend/packages/)

</div>

Tvheadend
=========

Tvheadend is the leading TV streaming server and Digital Video Recorder for Linux.

![tvheadend front page](https://github.com/tvheadend/tvheadend/raw/master/src/webui/static/img/epg.png)

It supports the following inputs:

  * ATSC
  * DVB-C(2)
  * DVB-S(2)
  * DVB-T(2)
  * HDHomeRun
  * IPTV
    * UDP
    * HTTP
  * SAT>IP
  * Unix Pipe

It supports the following outputs:

  * HTSP (native protocol)
  * HTTP
  * SAT>IP

Documentation
-------------

Tvheadend documentation can be found here: [https://docs.tvheadend.org](https://docs.tvheadend.org).

Support
-------

Please triage issues and ask questions in the forum: [https://tvheadend.org](https://tvheadend.org) or use the `#hts` IRC channel on Libera.Chat to speak with project staff: [https://web.libera.chat/#hts](https://web.libera.chat/#hts).

Please report triaged bugs via GitHub Issues. 

Building for Linux
------------------

First you need to configure:

	$ ./configure

If build dependencies are missing the configure script will complain or attempt
to disable optional features.

To build the binary:

	$ make

After compiling the Tvheadend binary is in the `build.linux` directory.

To run the Tvheadend binary:

	$ ./build.linux/tvheadend

Settings are stored in `$HOME/.hts/tvheadend`.

Running on Linux
----------------

Instructions for popular distributions are in our public [documentation](https://docs.tvheadend.org/documentation/installation/linux).

Running in Docker
-----------------

Running in Docker can be as simple as:

	$ docker run --rm ghcr.io/tvheadend/tvheadend:latest

See [README.Docker.md](README.Docker.md) for more details.
