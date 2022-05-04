<tvh_include>inc/networks_contents</tvh_include>

---

## Overview

A network is the type of carrier for your television signals. Tvheadend
supports several different types of network.

!['Networks' Tab Screenshot](static/img/doc/dvbinputs/dvbinput_networks.png)

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Network types

[Return to the index](class/mpegts_network)

Network type                                        | Description
----------------------------------------------------|-------------------
**C (Cable)**                                       | **Cable TV, delivered via a cable to your house.**
[DVB-C](class/dvb_network_dvbc)                     | Common in most of Europe.
[ISDB-C](class/dvb_network_isdb_c)                  | Common in Brazil and various other countries throughout south America.
[ATSC-C](class/dvb_network_atsc_c)                  | Common in north and central America and parts of south Asia.
**S (Satellite)**                                   | **Satellite, any signal coming in via a dish.**
[DVB-S](class/dvb_network_dvbs)                     | Available worldwide.
[ISDB-S](class/dvb_network_isdb_s)                  | Available worldwide but common in Brazil and various other countries throughout south America.
**T (Terrestrial)**                                 | Over-the-air broadcasts received through a traditional television aerial/antenna.
[DVB-T](class/dvb_network_dvbt)                     | Common in most of Europe.
[ISDB-T](class/dvb_network_isdb_t)                  | Common in Brazil and various other countries throughout south America.
[ATSC-T](class/dvb_network_atsc_t)                  | Common in north and central America.
**IPTV**                                            | **TV over the Internet via your broadband connection.**
[IPTV](class/iptv_network)                          | Manual IPTV input.
[IPTV Automatic Network](class/iptv_auto_network)   | IPTV using a playlist as the source - **Please read *IPTV Automatic Network - Don't Probe Services* below for important information!**  
  
Click the desired network type (to see all available [items](#items).

---

## Force Scanning

Force scanning can take some time. You may continue to use Tvheadend 
while a scan is in progress, but doing so will increase the time needed 
for it to complete. Note that the time required can vary depending on a 
number of factors, such as how many tuners you have available and the 
number of muxes on each.

---

## Service Probing (IPTV only)

Tvheadend will by default probe each playlist entry for service information. 
Some service providers do not allow such probing & will deny (or rate 
limit) access, leading to scan failures. 

To create services without probing, *Service ID* must be set 
(usually to 1) and the *Scan after creation* check box un-ticked. 

Note, the above two settings are only visible with the view level set to 
Expert.

---
