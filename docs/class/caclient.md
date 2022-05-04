<tvh_include>inc/caclient_contents</tvh_include>

---

## Overview

Tvheadend supports connecting to card clients via the cwc (newcamd) and
capmt (linux network dvbapi) protocols for so-called 'softcam' descrambling.

!['CA Client Configuration Example'](static/img/doc/caclient/cas.png)

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Client Types

Type                                                        | Description
------------------------------------------------------------|------------
[Linux DVB CAM Client](class/caclient_dvbcam)               | For use with devices that have a CAM module.
[Code Word Client (CWC) / newcamd](class/caclient_cwc)      | For newcamd or CWC.
[CCCam](class/caclient_cccam)                               | For CCCam connections.
[CAPMT (Linux DVBAPI)](class/caclient_capmt)                | For DVBAPI connections
[CSA CBC Constant Code Word](class/caclient_ccw_csa_cbc)    | For Constant Code Word connections (CSA/CBC variant)
[DES NCB Constant Code Word](class/caclient_ccw_des_ncb)    | For Constant Code Word connections (DES/NCB variant)
[AES ECB Constant Code Word](class/caclient_ccw_aes_ecb)    | For Constant Code Word connections (AES/ECB variant)
[AES ECB Constant Code Word](class/caclient_ccw_aes128_ecb) | For Constant Code Word connections (AES128/ECB variant)

Click a type to see its properties (below).


---

## Connection Status

The icon next to each entry within the grid indicates the clients 
connection status.

Icon                                         | Description
---------------------------------------------|------------
!['Accept/OK Icon'](icons/accept.png)        | The client is connected.
!['Error Icon'](icons/exclamation.png)       | There was an error.
!['Stop/Disabled Icon'](icons/stop.png)      | The client is disabled.

---

## OSCam Modes

| Mode                              | Notes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
|-----------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| OSCam net protocol (rev >= 10389) | A TCP connection to the server is created. All EMM/ECM data is sent to OSCam using this connection without the need for real linuxdvb devices to be present in the system.   This mode is suitable for all DVB devices including SAT\>IP and IPTV. If you run your OSCam server on the same machine as TVHeadend, set *Camd.socket filename / IP Address (TCP mode)* field to 127.0.0.1 and *Listen / Connect port* to 9000 (or your preferred TCP port). Note that the TCP port must match OSCam's configuration. The following lines are required in **[dvbapi]** section of oscam.conf: ```boxtype = pc pmt_mode = 4 listen_port = 9000 # or your preferred port```.|
| OSCam new pc-nodmx (rev >= 10389) | Similar to *OSCam net protocol (rev >= 10389)* mode, but a namedpipe (/tmp/camd.socket) connection is used instead of the TCP connection.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| OSCam TCP (rev >= 9574)           | A TCP connection to the server is created. All EMM/ECM data is sent to OSCam using this connection without the need for real linuxdvb devices to be present in the system. This mode is suitable for all DVB devices including SAT\>IP and IPTV. The following lines are required in **[dvbapi]** section of oscam.conf: ```boxtype = pc pmt_mode = 4 listen_port = 9000 # or your preferred port```                                                                                                                                                                                                                                                                   |
| OSCam pc-nodmx (rev >= 9756)      | Similar to *OSCam TCP (rev >= 9574)* mode, but a named pipe(/tmp/camd.socket) connection is used instead of the TCP connection.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| OSCam (rev >= 9095)               | This mode uses named pipe (/tmp/camd.socket). The difference between *Older OSCam* mode is that no UDP connections are required. All communication is processed through the named pipe(/tmp/camd.socket). The configuration for OSCam is same as the previous *Older OSCam* mode.                                                                                                                                                                                                                                                                                                                                                                                      |
| Older OSCam                       | This mode uses named pipe (/tmp/camd.socket). If selected, connection will be made directly to OSCam without using the LD\_PRELOAD / wrapper hack. TVH listens on a range of UDP ports starting with the specified port number (standard port range starts with 9000). The following lines are required in **[dvbapi]** section of oscam.conf: ```boxtype = pc pmt_mode = 4```                                                                                                                                                                                                                                                                                         |
| Wrapper (capmt_ca.so)             | This mode uses named pipe (/tmp/camd.socket). With the LD\_PRELOAD / wrapper hack active. TVH listens on the local specified UDP port (standard is 9000) for the code words. Only onechannel can be decoded at a time.                                                                                                                                                                                                                                                                                                                                                                                                                                                 |

Note, because of how markdown generates tables, the OSCam variables ```highlighted``` above must be on separate lines in your config file.

For example..
```
boxtype = pc
pmt_mode = 4 
listen_port = 9000 # or your preferred port
``` 

---
