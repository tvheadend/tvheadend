Contents                               | Description
---------------------------------------|------------------------
[Overview](#overview)                  | Tab overview
[Items/Properties](#items)             | Items and Properties

[Return to DVB Inputs](dvbinputs)

---

## Overview

A **T2-MI network** is a virtual network whose multiplexes are not tuned
directly but are *carried inside* another multiplex already received by
this Tvheadend instance. It is used to decapsulate:

* **T2-MI** streams: the DVB-T2 Modulator Interface (ETSI TS 102 773),
  where a whole DVB-T2 transport stream is packed into baseband frames
  and transported over one PID of a satellite or cable mux. This is how
  terrestrial (DTT) multiplexes are distributed to transmitter sites over
  satellite (for example the Abertis/Cellnex feeds on Hispasat 30°W).

* **Plain TS data piping**: an inner transport stream carried verbatim
  inside one PID of the outer mux (ETSI EN 301 192 data piping), without
  the baseband-frame layer.

The carrier can be scrambled. When a **carrier service** is configured,
the carrier PID passes through the normal service and descrambler path
first, so any working CA client descrambles it exactly as it would a
normal service. Only then is the inner stream extracted.

The network references one or more **source muxes** and discovers the
T2-MI / piping carriers among their services, creating one inner
multiplex per carrier. Once scanned, each inner multiplex exposes its
services like any other mux: they can be mapped to channels, streamed
and recorded normally. Muxes can also be added by hand.

A multiplex whose carrier temporarily disappears is kept (its scan
fails, like a DVB multiplex that went dark), so services and channel
mappings survive changes in the feed. Multiplexes are removed only when
their source mux is removed from Tvheadend or the network itself is
deleted. Set *Network discovery* to **Disable** to stop new carriers
being added automatically.

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---
