---

#### OSCam net protocol (rev >= 10389)

**This is the most preferred mode for the latest OSCam.** It supports
the latest features like the ecm status.

A TCP connection to server is created. All emm/ecm data are send to
oscam using this connection without a requirement for the real linuxdvb
devices in the system with OSCam. This mode is suitable for all DVB
devices including SAT\>IP and IPTV.

If you run your OSCam server on the same machine as TVHeadend, set
*Camd.socket filename / IP Address (TCP mode)* field to 127.0.0.1
and *Listen / Connect port* to 9000 (or any your preferred TCP port
number). Note that this TCP port number must be same as in OSCam's
configuration.

The following lines are required in **[dvbapi]** section of oscam.conf
file:

```
boxtype = pc
pmt_mode = 4
listen_port = 9000 # or your preferred port
```

#### OSCam new pc-nodmx (rev >= 10389)

Similar to *OSCam net protocol (rev >= 10389)* mode, but a named
pipe (/tmp/camd.socket) connection is used instead of the TCP connection.

#### OSCam TCP (rev >= 9574)

A TCP connection to server is created. All emm/ecm data are send to
oscam using this connection without a requirement for the real linuxdvb
devices in the system with OSCam. This mode is suitable for all DVB
devices including SAT\>IP and IPTV.

The following lines are required in **[dvbapi]** section of oscam.conf
file:

```
boxtype = pc
pmt_mode = 4
listen_port = 9000 # or your preferred port
```

#### OSCam pc-nodmx (rev >= 9756)

Similar to *OSCAM TCP (rev >= 9574)* mode, but a named pipe
(/tmp/camd.socket) connection is used instead of the TCP connection.

#### OSCam (rev >= 9095)

This mode uses named pipe (/tmp/camd.socket).

The difference between *Older OSCam* mode is that no UDP connections
are required. All communication is processed through the named pipe
(/tmp/camd.socket). The configuration for OSCam is same as for previous
*Older OSCam* mode.

#### Older OSCam

This mode uses named pipe (/tmp/camd.socket).

If selected, connection will be made directly to oscam without using
LD\_PRELOAD / wrapper hack. TVH listens on a range of UDP ports
starting with the specified port number (standard port range starts
with the port number 9000).

The following lines are required in **[dvbapi]** section of oscam.conf file:

```
boxtype = pc
pmt_mode = 4
```

#### Wrapper (capmt_ca.so)

This mode uses named pipe (/tmp/camd.socket).

LD\_PRELOAD / wrapper hack is active. TVH listens on local specified UDP port
(standard port number is 9000) for the code words. Only one channel can be decoded.

---
