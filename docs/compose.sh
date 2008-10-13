#!/bin/sh

if true; then

    emit() {
	echo -n '<div class="hts-doc-'
	echo -n $1
	echo -n '">'
	echo -n $2
	echo -n ' '
	echo -n $3
	echo '</div>'
	cat $4
    }
else
    emit() {
	true
    }
fi

emit chapter 1     "Overview"                              html/tvheadend.html
emit section 1.1   "List of features"                      html/features.html
emit section 1.2   "Install and initial setup"             html/install.html
emit section 1.3   "Frequently Asked Questions"            html/faq.html
emit chapter 2     "Electronic Program Guide"              html/epg.html
emit chapter 3     "Digital Video Recorder"                html/dvr.html
emit section 3.1   "DVR Log"                               html/dvrlog.html
emit section 3.2   "DVR Autorecorder"                      html/autorec.html
emit chapter 4     "Configuration and administration"      html/config.html
emit section 4.1   "Access configuration"                  html/config_access.html
emit section 4.2   "Channel configuration"                 html/config_channels.html
emit section 4.3   "XML-TV configuration"                  html/config_xmltv.html
emit section 4.4   "Tags configuration"                    html/config_tags.html
emit section 4.5   "Digital Video Recorder configuration"  html/config_dvr.html
emit section 4.6   "DVB configuration"                     html/config_dvb.html
emit section 4.7   "CWC configuration"                     html/config_cwc.html

echo "</BODY></HTML>"