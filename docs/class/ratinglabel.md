<tvh_include>inc/ratinglabel_contents</tvh_include>

---

## Overview

This tab lists all defined parental rating labels.

!['Complete rating labels list'](static/img/doc/ratinglabel/rating_labels_complete.png)

A 'rating label' is a text code like 'PG', 'PG-13' or 'FSK 12' used to identify the parental rating classification of a TV programme.

Rating labels can be sourced from the OTA EPG grabber or from the XMLTV grabber.

**NOTE:** Rating labels are not enabled by default and must be enabled in the [EPG Grabber](class/epggrab) module under 'General Settings'.


# DVB OTA

Ratings from the OTA EPG do not contain rating text like 'PG', instead, a combination of country code and age is transmitted, eg: AUS + 8.  It is the responsibility of the receiver unit to decode this combination and determine the rating text to display.

When the rating labels module encounters a new country and age combination, it will create a placeholder entry in the rating labels table as follows:

!['Newly learned rating labels list'](static/img/doc/ratinglabel/rating_labels_learned.png)

When a placeholder label is in use, the programme details in the EPG will show this placeholder entry rather than the expected value.

!['EPG with placeholder rating'](static/img/doc/ratinglabel/epg_placeholder.png)

You are required to manually edit this placeholder entry in order to provide the appropriate rating text to display.  The correct text can be found by searching for the specific programme in another EPG source or by obtaining the classification guidelines in the location (country) in question.  This only needs to be done once for each label, all other programmes with that label will be automatically adjusted.

!['Updated rating label details'](static/img/doc/ratinglabel/updated_label.png)

**NOTE:** In the example, the age provided by DVB is '10', whereas the age displayed is '13'.  This is because the DVB standard subtracts 3 from some recommended ages before transmission meaning that the receiver must add 3 to the number received.  When creating a placeholder label, this module will automatically add 3 where appropriate.


# XMLTV

Ratings from XMLTV contain the rating label text, but not the recommended age.
```
<rating system="ACMA">
  <value>MA</value>
</rating>
```

When a new rating is encountered from an XMLTV EPG source, a placeholder label similar to the DVB ones is created and you will need to add the country code and the ages.

!['Rating label learned from xmltv'](static/img/doc/ratinglabel/xmltv_learned.png)

# Combined DVB OTA and XMLTV

If you have multiple EPG sources for different groups of channels, it is possible to map the ratings from those multiple sources to produce a single unified rating system.  This can be done by adjusting the 'display age' and 'display label' of the various sources until they are matched to your requirements.

The same rating label can be used for both DVB OTA and XMLTV EPG sources.  Because DVB OTA is matched on Country+Age and XMLTV is matched on Authority+Label, a rating label that contains all 4 of these values will be selected and the 'display age' and 'display label' will be used for both EPG sources.

Sources can also be kept seperated by ensuring that a DVB OTA rating does not have an 'authority' that matches any XMLTV sources and that an XMLTV rating does not have an 'age' or 'country' that matches a DVB OTA source, only a 'display age'.


---
