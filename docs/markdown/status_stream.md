# Status - Stream

<tvh_include>inc/status_contents</tvh_include>

---

## Overview

This tab shows information about all currently-open streams.

This is a read-only tab; nothing is configurable.

!['Status - Stream' Tab](static/img/doc/status/stream.png)

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Items

**Sweep/Clean Icon !['Status - Stream' Tab](static/icons/clean.png)**
: Clear all "Uncorrected Blocks", "BER", etc stats.

**Input**
: Device used to receive the stream.

**Sub No**
: Number of subscriptions using the stream.

**Weight**
: Stream weighting.

**PID list**
: Input source Program Identification (PIDs) numbers in use.

**Bandwidth**
: Total stream input bandwidth.

**BER**
: [Bit Error Ratio](https://en.wikipedia.org/wiki/Bit_error_rate)

**PER**
: [Packet Error Ratio](https://en.wikipedia.org/wiki/Bit_error_rate#Packet_error_ratio)

**Uncorrected Blocks**
: Number of uncorrected blocks. A value higher than 0 can indicate a
weak signal or interference, note that some devices can send a false value.

**Transport Errors**
: Number of transport streams errors. A fast increasing value here can
indicate signal issues. Device drivers can sometimes send garbage data at
the beginning of a stream, as long as the value doesn't increase at a fast
pace and you have no playback issues, there is nothing to worry about.

**Continuity Errors**
: Continuity Count Error. Number of stream errors, a high value here can indicate a signal problem.

**SNR**
: Signal (To) Noise Ratio. [The level of a desired signal to the level of background noise](https://en.wikipedia.org/wiki/Signal-to-noise_ratio),
note that not all devices supply correct signal information,
the value here can sometimes be ambiguous.

**Signal Strength**
: The signal strength as reported by the device, note that not all devices
supply correct signal information, the value here can sometimes be ambiguous
