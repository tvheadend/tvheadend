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
