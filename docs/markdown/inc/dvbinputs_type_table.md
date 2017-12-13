Type                                                   | Description
-------------------------------------------------------|--------------------------------------------
**Frontends**                                          | **Where you configure the frontend, whether or not it's enabled etc.**
[Master (DVB-S)](class/linuxdvb_frontend_dvbs)         | The master DVB-S/S2 frontend (most DVB-S tuners use this type).
[Master (ISDB-S)](class/linuxdvb_frontend_isdb_s)      | The master ISDB-S/S2 frontend.
[Slave (DVB-S/ISDB-S)](class/linuxdvb_frontend_dvbs_slave) | A slave frontend (can be used to link with a master, mainly used for buggy drivers or frontends that share an input).
[Master DVB-T](class/linuxdvb_frontend_dvbt)           | The master DVB-T/T2 frontend (most DVB-T tuners use this type).
[ATSC-T](class/linuxdvb_frontend_atsc_t)               | The master ATSC-T frontend (most ATSC-T tuners use this type).
[ISDB-T](class/linuxdvb_frontend_isdb_t)               | The master ISDB-T frontend (most ISDB-T tuners use this type).
[DVB-C](class/linuxdvb_frontend_dvbc)                  | The master DVB-C/C2 frontend (most DVB-C tuners use this type).
[ATSC-C](class/linuxdvb_frontend_atsc_c)               | The master ATSC-C frontend (most ATSC-C tuners use this type).
[ISDB-C](class/linuxdvb_frontend_isdb_c)               | The master ISDB-C frontend (most ISDB-C tuners use this type).
[DVB-S (SAT>IP Master)](class/satip_frontend_dvbs)     | The master SAT>IP DVB-S/S2 frontend (most SAT>IP DVB-S tuners use this type).
[DVB-S (SAT>IP Slave)](class/satip_frontend_dvbs_slave)| A slave frontend (can be used to link with a master, mainly used for buggy drivers or frontends that share an input).
**Satellite Configuration**                            | **Where you configure various settings related to your DVB-S tuners.**
[Universal LNB](class/linuxdvb_satconf_lnbonly)        | Universal LNB - most DVB-S tuners.
[2 Port](class/linuxdvb_satconf_2port)                 | 2 Port configuration.
[4 Port](class/linuxdvb_satconf_4port)                 | 4 Port configuration.
[Advanced LNB](class/linuxdvb_satconf_advanced)        | Advanced LNB configuration.
[Unicable EN50494 (experimental)](class/linuxdvb_satconf_en50494) | Unicable LNB configuration.
[DiseqC Rotor](class/linuxdvb_rotor)                   | DiseqC rotor configuration.
[DiSEqC Switch](class/linuxdvb_switch)                 | DiSEqC Switch configuration.
[Rotor (GOTOX)](class/linuxdvb_rotor_gotox)            | Rotor (GOTOX) configuration.
[Rotor (USALS)](class/linuxdvb_rotor_usals)            | Rotor (USALS) configuration.
[SAT>IP Client](class/satip_client)                           | SAT>IP client configuration.
[SAT>IP Satellite Configuration](class/satip_satconf)  | SAT>IP Satellite Configuration

For more information, click on a type.
