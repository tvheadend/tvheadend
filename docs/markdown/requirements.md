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
