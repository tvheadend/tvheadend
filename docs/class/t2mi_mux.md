Contents                               | Description
---------------------------------------|------------------------
[Overview](#overview)                  | Tab overview
[Items/Properties](#items)             | Items and Properties

[Return to DVB Inputs](dvbinputs)

---

## Overview

A **T2-MI multiplex** decapsulates one inner transport stream carried
inside a mux received elsewhere in this Tvheadend instance. See the
[T2-MI Network](class/t2mi_network) overview for the concept.

To create one you need to identify, on the outer mux:

1. **Source mux**: the mux that carries the stream (for example a DVB-S2
   transponder), selected from the list of muxes.
2. **Carrier service** (recommended): the service ID whose PID carries
   the T2-MI / piped stream. Subscribing through the service lets the
   descrambler run, which is required for scrambled carriers. Set it to
   `0` to read a **carrier PID** directly instead (clear carriers only).
3. **Carrier PID**: the PID inside the source mux carrying the stream
   (often 4096). Leave `0` to detect it from the carrier service's
   components (a stream signalled with a T2MI descriptor, or a lone
   private data component, is picked automatically).

The **carrier format** may be left on *Auto-detect*; it locks onto T2-MI
or plain TS piping from the first packets seen. For multi-PLP T2-MI
streams, set the **PLP ID** to select one (`-1` picks the first data PLP).

Detailed decapsulation activity is available under the **T2-MI**
logging subsystem (enable *debug* / *trace* for it).

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---
