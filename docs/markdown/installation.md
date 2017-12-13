# Installation

Contents                                                 | Description
---------------------------------------------------------|------------------------------------ 
[Basic Requirements](#basic-requirements)                | What's needed to run Tvheadend
[Install Your Tuners](#install-your-tuners)              | Installing and setting up your hardware
[Install Tvheadend](#install-tvheadend)                  | Where to get Tvheadend and how-to install it

---

## Basic Requirements

### Physical Architecture

It's perfectly possible to install and run Tvheadend as a single-seat installation,
with the software running on the same system as any client software (e.g. Kodi),
with all files stored locally.

Alternatively, you can run Tvheadend on a server, perhaps on an always-on
system that houses your media, perhaps on a dedicated low-power system - it's your choice.

Where you have aerial/coax connections might influence your choice - unless
you use SAT>IP or have some other way to transport your TV signal over a LAN,
your Tvheadend installation has to live where you can actually connect your
tuners. 

### System Requirements

Wherever you install it, Tvheadend primarily runs on **Linux** - pre-built
binaries are available for most Debian-based distributions (Debian itself, Ubuntu,
Mint...) and RPMs for Fedora, or you can build it yourself. It runs on both
32- and 64-bit x86 and ARM processors, and so also can be built for Android
(which uses the Linux kernel).

You will only need **c. 30MB disk space** for the application and associated
files, and maybe anything up to **1GB** for your configuration - depending on 
how many tuners of what type you have, how many channels you receive, your
choice of programme guide, and so on. You'll clearly need much more for
your recordings, though: as a guide, an hour of SD MPEG-2 video will take
about 1GB, while high bitrate HD H.264 will easily consume 5GB+ per hour.

Tvheadend is intended to be lightweight, so it will run on a NAS or similar
**low-powered CPU**. Note that the exception here is transcoding: if you want
to convert high-definition video in real time then you will need a powerful,
multi-core system. It will happily run in less than **1GB of RAM**, and many
people run it successfully on original Raspberry Pis with perhaps only 256MB
of usable free memory. This does depend on what else you're using the computer
for, though, as a GUI will drain your system as will any serious file serving.

And, of course, you'll need one or more **TV tuners** if you want to receive
regular broadcast television - otherwise, you're limited to IP sources.

An **Internet connection** is recommended but not essential. You need to 
have an accurate clock for EPG timers to work, for example, but this can be 
synchronised from the broadcast signal if you're not in a position to use
`ntp` or similar.

### How Lightweight?

How about light enough to run on a travel router? Take a look at this
[example](https://tvheadend.org/boards/4/topics/16579) from one of our users...

---

## Install Your Tuners

This section will give you some basic ideas on how to get your tuner working
with your operating system. However, it's clearly way beyond the scope of
this guide to tell you everything: consult specialist forums, search around, 
and at least do some research to work out what's likely to work or not
before you hand over any money.

There is a discussion about supported hardware on [the Tvheadend
forums](https://tvheadend.org/boards/5/topics/5102).

### 1. Install the Tuner Hardware

This is obviously a core requirement that's outside of the scope of this guide.

You basically have the choice of:

* External USB tuners that plug in
* Internal (e.g. PCI) tuners that go inside the computer chassis
* External SAT>IP tuners that send MPEG-TS streams over a LAN connection
* External HDHomeRun tuners that send MPEG-TS streams over a LAN connection

Follow the appropriate installation instructions and, if relevant, the
setup instruction (e.g. for SAT>IP, which are effectively small, standalone
computers).

#### A Note on USB Tuners

USB tuners are cheap, work well and are frequently well-matched to physically-smaller
builds (e.g. HTPCs) which simply don't have the internal slots. However, please
remember that many need external power, or need a powered hub to work properly.

In addition, even USB3 doesn't have the greatest practical bandwidth per bus. That
means you're probably asking for problems if you have four DVB-S2 dongles on the same USB
connection to the motherboard.

This is particularly true of systems such as the Raspberry Pi which share USB
bandwidth with the Ethernet port. Don't be surprised if this kind of platform struggles
and/or reports errors in a multi-tuner configuration, especially on
high-bandwidth (e.g. HD) streams.

### 2. Install Firmware and/or Drivers

Similar to the above, Tvheadend can do nothing if your tuners aren't working
properly. A good place to check how to set up your tuners is the
[LinuxTV wiki device library](http://www.linuxtv.org/wiki/index.php/Hardware_Device_Information) -
this will not only tell you what's supported under Linux, but also
how to get it all working.

As a broad guide, though, you need two main components: a driver, and firmware. 

The **driver** is the piece of software that, as far as the operating system is concerned,
controls the tuner hardware. 

Driver software typically comes either built-in to the operating system
(a clue here is documentation that says *"supported since kernel 3.16"*, for example)
or as an external program that needs to be compiled in (e.g. how you'd build TBS'
or Digital Devices drivers, or perhaps where the driver is supported in a later version
of LinuxTV V4L-DVB than has made it to your kernel - the giveaway here is 
*"compile and install the latest media_build"*).

Many tuners then also require **firmware** - normally, a binary file that's been
extracted from the proprietary drivers used by Windows.

Many Linux distros include a package for the most common devices (e.g.
*linux-firmwares* under Ubuntu or *firmware-linux-nonfree* under Debian).
If this isn't sufficient, a good source of firmware files are the
[OpenElec](https://github.com/OpenELEC/dvb-firmware) and [LibreELEC](https://github.com/libreELEC/dvb-firmware)
firmware repositories on Github.

Typically, download the binary file and install it into `/lib/firmware`, owned
by `root:root`, permissions `rw-r--r--` (0644)

---

## Install Tvheadend

This section tells you how to get hold of the software in the first place,
and how to get it onto your system.

Follow the instructions that are specific to your Linux distribution
(Ubuntu/Debian/Mint, Arch, Fedora...). This will typically be PPA-and-dpkg
for Debian, but most other distros will need you to build your own version from source.

* [Debian/Ubuntu installation instructions](https://tvheadend.org/projects/tvheadend/wiki/AptRepository)

* [Instructions on how to build from source](https://tvheadend.org/projects/tvheadend/wiki/Building)

Do not assume that your distro's package manager will give you the latest
version of Tvheadend - indeed, give you any version at all. Always check.

Where a pre-built package exists, this will usually get you the last official
stable version. However, more advanced users may be interested in running
a development version - either a nightly build or a self-compiled version.

