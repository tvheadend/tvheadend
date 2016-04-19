##DVB Inputs - TV Adapters

The adapters and tuners are listed and edited in a tree.

!['TV Adapter tree'](docresources/tvadaptertree.png)

---

###Buttons

The following buttons are available:

Button         | Function
---------------|---------
**Save**       | Save the current configuration.
**Help**       | Display this help page.

---

###Device Tree

The device tree lists the available frontends, LNB configuration and 
so on related to your device(s) in sections. Clicking on these sections 
will display all available parameters and various device information.

!['TV Adapter params'](docresources/tvadapterparams.png)

**Tip**: Remember to save your changes *before* switching panels.

---

##Device Configuration Options

You will generally see the following parameters for your device(s), however 
there are device-specific parameters too, see 
[Device-specific Parameters](#device-specific-parameters) for a full 
list.

<tvh_class_items>linuxdvb_frontend</tvh_class_items>

---

##Device-specific Parameters

###Satellite (DVB-S/ISDB-S)

* Frontend
  - [Master](class/linuxdvb_frontend_dvbs)
  - [Slave](class/linuxdvb_frontend_dvbs_slave)
  - [Master (ISDB-S)](class/linuxdvb_frontend_isdb_s)
* Satellite Configuration
  - [Universal LNB](class/linuxdvb_satconf_lnbonly)
  - [2 Port](class/linuxdvb_satconf_2port)
  - [4 Port](class/linuxdvb_satconf_4port)
* Satellite Configuration (Advanced)
  - [Advanced LNB](class/linuxdvb_satconf_advanced)
  - [Unicable EN50494 (experimental)](class/linuxdvb_satconf_en50494)
  - [DiseqC Rotor](class/linuxdvb_rotor)
  - [DiSEqC Switch](class/linuxdvb_switch)
  - [Rotor (GOTOX)](class/linuxdvb_rotor_gotox)
  - [Rotor (USALS)](class/linuxdvb_rotor_usals)
  
---

###Terrestrial (DVB-T/ATSC-T/ISDB-T)

* Frontend
  - [DVB-T/DVB-T2](class/linuxdvb_frontend_dvbt)
  - [ATSC-T](class/linuxdvb_frontend_atsc_t)
  - [ISDB-T](class/linuxdvb_frontend_isdb_t)
  
---

###Cable (DVB-C/ATSC-C/ISDB-C)

* Frontend
  - [DVB-C](class/linuxdvb_frontend_dvbc)
  - [ATSC-C](class/linuxdvb_frontend_atsc_c)
  - [ISDB-C](class/linuxdvb_frontend_isdb_c)
