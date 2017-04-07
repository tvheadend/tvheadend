Tvheadend supports connecting to card clients via the cwc (newcamd) and
capmt (linux network dvbapi) protocols for so-called 'softcam' descrambling.

!['CA Client Configuration Example'](static/img/doc/caclient/tab.png)

---

###Menu Bar/Buttons

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

###Available CA types

The following configuration parameters are used, depending on the type 
of CA access:

* List of types

  - [CAPMT (Linux Network DVBAPI)](class/caclient_capmt)
  - [Code word client (newcamd)](class/caclient_cwc)
  - [DES constant code word client](class/caclient_ccw_des)
  - [AES constant code word client](class/caclient_ccw_aes)

---

###Connection Status

The icon next to each entry within the grid indicates the client's 
connection status.

Icon                                         | Description
---------------------------------------------|------------
!['Accept/OK Icon'](icons/accept.png)        | The client is connected.
!['Error Icon'](icons/exclamation.png)       | There was an error.
!['Stop/Disabled Icon'](icons/stop.png)      | The client is disabled.

---
