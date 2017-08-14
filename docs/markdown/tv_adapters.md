## DVB Inputs - TV Adapters

The adapters and tuners are listed and edited in a tree.

!['TV Adapter tree'](static/img/doc/tv_adapters/tree.png)

---

### Buttons

The following functions are available:

Button         | Function
---------------|---------
**Save**       | Save the current configuration.
<tvh_include>inc/common_button_table_end</tvh_include>

---

### Device Tree

The device tree lists the available frontends, LNB configuration and 
so on related to your device(s) in sections. Clicking on these sections 
will display available parameters and device information.

!['TV Adapter params'](static/img/doc/tv_adapters/params.png)

**Tip**: Remember to save your changes *before* switching panels.

---

### Device Configuration

Click on an item to display more information.

#### Satellite (DVB-S/ISDB-S)

* Frontend
  - [Master](class/linuxdvb_frontend_dvbs)
  - [Master (ISDB-S)](class/linuxdvb_frontend_isdb_s)
  - [Slave](class/linuxdvb_frontend_dvbs_slave)
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
  
#### Terrestrial (DVB-T/ATSC-T/ISDB-T)

* Frontend
  - [DVB-T/DVB-T2](class/linuxdvb_frontend_dvbt)
  - [ATSC-T](class/linuxdvb_frontend_atsc_t)
  - [ISDB-T](class/linuxdvb_frontend_isdb_t)

#### Cable (DVB-C/ATSC-C/ISDB-C)

* Frontend
  - [DVB-C](class/linuxdvb_frontend_dvbc)
  - [ATSC-C](class/linuxdvb_frontend_atsc_c)
  - [ISDB-C](class/linuxdvb_frontend_isdb_c)

#### SAT>IP (DVB-T/ATSC-T/ATSC-C/DVB-S)

* [Client](class/satip_client)
   
* Frontend
   - [ATSC-T](class/satip_frontend_atsc_t)
   - [ATSC-C](class/satip_frontend_atsc_c)
   - [DVB-T](class/satip_frontend_dvbt)
   - [DVB-S (Master)](class/satip_frontend_dvbs)
   - [DVB-S (Slave)](class/satip_frontend_dvbs_slave)
   
* [Satellite Configuration](class/satip_satconf)

---
