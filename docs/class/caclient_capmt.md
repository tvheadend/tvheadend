Tvheadend supports connecting to card clients via the cwc (newcamd) and
capmt (linux network dvbapi) protocols for so-called 'softcam' descrambling.

!['Channel tag'](static/img/doc/caclient_capmt/tab.png)

---

### Menu Bar/Buttons

The following functions are available:

Button              | Function
--------------------|---------
**Save**            | Save any changes made to the CA client configuration.
**Undo**            | Undo any changes made to the CA client configuration since the last save.
**Add**             | Add a new CA client configuration.
**Delete**          | Delete an existing CA client configuration.
**Clone**           | Clone the currently selected configuration.
**Move Up**         | Move the selected CA client configuration up in the list.
**Move Down**       | Move the selected CA client configuration down in the list.
**Show/Hide Passwords**  | Reveal/Hide any stored CA client passwords.
**Help**            | Display this help page.

---

### Connection Status

The icon next to each entry within the grid indicates the client's 
connection status.

Icon                                         | Description
---------------------------------------------|------------
!['Accept/OK Icon'](icons/accept.png)        | The client is connected.
!['Error Icon'](icons/exclamation.png)       | There was an error.
!['Stop/Disabled Icon'](icons/stop.png)      | The client is disabled.

---

### Modes

#### OSCam net protocol (rev >= 10389)

**This is the most preferred mode for the latest OSCam.** It supports
the latest features like ECM status.

A TCP connection to the server is created. All EMM/ECM data is sent to
OSCam using this connection without the need for real linuxdvb
devices to be present in the system. This mode is suitable for all DVB
devices including SAT\>IP and IPTV.

If you run your OSCam server on the same machine as TVHeadend, set
*Camd.socket filename / IP Address (TCP mode)* field to 127.0.0.1
and *Listen / Connect port* to 9000 (or your preferred TCP port).
Note that the TCP port must match OSCam's configuration.

The following lines are required in **[dvbapi]** section of oscam.conf:

```
boxtype = pc
pmt_mode = 4
listen_port = 9000 # or your preferred port
```

#### OSCam new pc-nodmx (rev >= 10389)

Similar to *OSCam net protocol (rev >= 10389)* mode, but a named
pipe (/tmp/camd.socket) connection is used instead of the TCP connection.

#### OSCam TCP (rev >= 9574)

A TCP connection to the server is created. All EMM/ECM data is sent to
OSCam using this connection without the need for real linuxdvb
devices to be present in the system. This mode is suitable for all DVB
devices including SAT\>IP and IPTV.

The following lines are required in **[dvbapi]** section of oscam.conf:

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
(/tmp/camd.socket). The configuration for OSCam is same as the previous
*Older OSCam* mode.

#### Older OSCam

This mode uses named pipe (/tmp/camd.socket).

If selected, connection will be made directly to OSCam without using the
LD\_PRELOAD / wrapper hack. TVH listens on a range of UDP ports
starting with the specified port number (standard port range starts
with 9000).

The following lines are required in **[dvbapi]** section of oscam.conf:

```
boxtype = pc
pmt_mode = 4
```

#### Wrapper (capmt_ca.so)

This mode uses named pipe (/tmp/camd.socket).

With the LD\_PRELOAD / wrapper hack active. TVH listens on the local
specified UDP port (standard is 9000) for the code words. Only one
channel can be decoded at a time.

---
