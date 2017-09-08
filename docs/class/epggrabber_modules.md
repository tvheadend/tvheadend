This tab is used to configure the Electronic Program Guide (EPG) 
grabber modules. Tvheadend supports a variety of different EPG 
grabbing mechanisms. These fall into 3 broad categories, within which 
there are a variety of specific grabber implementations.

!['EPG Grabber Configuration'](static/img/doc/epggrabber_modules/tab.png)

---

### Menu Bar/Buttons

The following functions are available:

Button                      | Function
----------------------------|-------------------
**Save**                    | Save any changes made to the tab.
**Undo**                    | Revert any changes made since the last save.

<tvh_include>inc/common_button_table_end</tvh_include>

---

### Notes

* Only OTA EIT and PSIP (ATSC) grabbers are enabled by default. If 
you're missing EPG data, make sure to enable the correct grabber(s) 
for your location/provider.
* If you use more than one grabber, be sure to give a higher priority 
to the grabber that provides you with richer data.

**Tip**: Don't forget to set the *EIT time offset* for your network(s).

---

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
