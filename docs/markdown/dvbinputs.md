# DVB Inputs

Contents                                                         | Description
-----------------------------------------------------------------|-------------------------------------
[Overview](#overview)                                            | Tab overview
[Device Types and Configuration](#device-types-and-configuration)| Table of device types and their respective configuration options
[TV Adapters](#tv-adapters)                                      | Where you configure adapters
[Networks](class/mpegts_network)                                 | Where you configure networks
[Muxes](class/mpegts_mux)                                        | Where you configure multiplexes
[Services](class/mpegts_service)                                 | Discovered service(s) management
[Mux Schedulers](class/mpegts_mux_sched)                         | Where you configure mux schedulers - schedule Tvheadend to tune to a specific mux to receive different types of data, EMMs, EIT etc

---

## Overview

This tab is where you configure/manage your adapters/input,
networks, muxes and services.


!['DVB Inputs' Tab](static/img/doc/dvbinputs/dvbinput_tab.png)

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Device Types and Configuration

<tvh_include>inc/dvbinputs_type_table</tvh_include>

---

## TV Adapters

The adapter tree lists the available frontends, LNB configuration and
so on related to your device(s) in sections. Clicking on these sections
will display available parameters and device information. Bold sections
and a green dot indicate that it's enabled. A red dot indicates that it's
disabled. Please see the [Introduction](introduction) (Using the Interface) for
details on how to use the tree/split panel.

### An Example TV Adapter Tree

Here is an example of a device tree - yours will follow the same layout,
but this is just to give you an idea as to what all the bits mean.

!['DVB Input tree explained' Tab](static/img/doc/dvbinputs/the_tree_explained.png)

Number in Image / Text                               | Description
-----------------------------------------------------|------------------------------------
1. /dev/dvb/adapter0 [Panasonic MN88472] #0          | `/dev/dvb/adapter0` indicates the location (or path) of the device.
                                                     | `[Panasonic MN88472]` is the demodulation chipset name given to it by the kernel driver.
                                                     | `#0` is the adapter number (also used in the path).
2. Panasonic MN88472 #0 : DVB-T #0                   | `Panasonic MN88472` is the chipset name.
                                                     | `#0` is the adapter number.
                                                     | `DVB-T` is the delivery system.
                                                     | `#0` is the frontend number. A tuner can have many frontends!
3. Tvheadend:9983 cd33bf4ce5 - 192.168.1.3           | `Tvheadend` is the SAT>IP server name.
                                                     | `9983` is the RTSP server listening port.
                                                     | `cd33bf4ce5` is a unique ID.
                                                     | `192.168.1.3` is the server's IP address.
4. Position #1 (AA)                                  | A tuner can have multiple positional inputs, `Position #1 (AA)` indicates the tuner (in this case `SAT>IP DVB-S Tuner #1 (192.168.1.3:9983)`) is using position 1 (or AA).
                                                     | A position is very similar to a network in that it groups multiplexes (or Transponders) for each satellite you're able to receive. It also allows you to set certain configuration options, such as where the dish should move to in order to receive a multiplex.
                                                     | `192.168.1.3` is the server's IP address.
                                                     | `9983` is the listening port.

---
