:

If you have *Network Discovery* enabled, an out-of-date mux list isn't 
usually an issue provided that one of the muxes in the list scans 
successfully and has a [Network Information Table (NIT)](https://en.wikipedia.org/wiki/Program-specific_information#NIT_.28network_information_specific_data.29) 
available. Tvheadend will parse the NIT then the add newly discovered 
muxes automatically.

