<tvh_include>inc/channels_contents</tvh_include>

---

## Overview

This tab is used to configure the Electronic Program Guide (EPG) 
grabber modules. Tvheadend supports a variety of different EPG 
grabbing mechanisms. These fall into 3 broad categories, within which 
there are a variety of specific grabber implementations.

!['EPG Grabber Configuration'](static/img/doc/channel/grabber_modules_tab.png)

Type                                          | Description
----------------------------------------------|----------------------------
[Over-the-air (OTA)](class/epggrab_mod_ota)   | This type of grabber pulls EPG data directly from the broadcast signal.
[Internal XMLTV](class/epggrab_mod_int_xmltv) | This type of grabber executes an internal (local) [XMLTV](http://xmltv.org) grabber script & parses the output.
[External XMLTV](class/epggrab_mod_ext_xmltv) | This type of grabber reads EPG data from a socket pushed to it using an [XMLTV](http://xmltv.org) grabber script.
[Internal PyEPG](class/epggrab_mod_int_pyepg) | This type of grabber executes an internal (local) [PyEPG](https://github.com/adamsutton/PyEPG) grabber script & parses the output. This isn't widely used!
[External PyEPG](class/epggrab_mod_ext_pyepg) | This type of grabber reads EPG data from a socket pushed to it using an [PyEPG](https://github.com/adamsutton/PyEPG) grabber script. This isn't widely used!

---

## Buttons

<tvh_include>inc/buttons</tvh_include>

---

## Notes

Only OTA EIT and PSIP (ATSC) grabbers are enabled by default. If 
you're missing EPG data, make sure to enable the correct grabber(s) 
for your location/provider. If you use more than one grabber, be sure 
to give a higher priority to the grabber that provides you with richer data.

Don't forget to set the *EIT time offset* for your network(s)!

### OTA Scrapper

Some OTA EIT grabber mechanisms support additional scraping options.

The OTA broadcast data often does not have specific dedicated fields
to describe the programme season, episode, etc. Sometimes this
information is included in the programme summary.

The scraper configuration files contains regular expressions to scrape
additional information from the broadcast information.  For example
the broadcast summary may include the text '(S10 E5)' and the
configuration file will extract this.

This scraping option does not access or retrieve details from the
Internet.

Currently only a limited number of configuration files are shipped and
these are located in the epggrab/eit/scrape directory.

If the "EIT: DVB Grabber" is used then typically you would enter the
configuration file (such as "uk") and enable relevant tickboxes to
enable the additional scraping.

If the scraper configuration is not enabled then the default behaviour
means broadcast information such as summary information will still be
retrieved.

---
