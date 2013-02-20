#!/bin/bash

## Example script to pull down the freeview.xml mappings and push to unix socket

wget -O - http://supplement.xmltv.org/tv_grab_uk_rt/lineups/freeview.xml | nc -U ~/.hts/tvheadend/epggrab/xmltv.sock
