##Configure Tvheadend

This section gives a high-level overview of the steps needed to get Tvheadend
up and running. For more detailed information, please consult the rest of
this guide - much of it is arranged in the same order as the tabs on the
Tvheadend interface so you know where to look.

You can also consult the in-application help text, which mirrors this guide
to a very great extent.

###1. Ensure Tuners are Available for Use

**Tvheadend web interface: _Configuration -> DVB Inputs -> TV Adapters_**

On this tab, you'll see a tree structure, with the Linux device list at the
top level (e.g. `/dev/dvb/adapter0`)

Individual tuners are then the next level down (e.g. `DiBcom 7000PC : DVB-T #0`)

Click on each tuner that you want Tvheadend to use, and ensure "Enabled"
is checked in the 'Parameters' list

If anything is obviously wrong at this point, you probably have a
driver/firmware error which you'll need to resolve before going any further.

###2. Set up Relevant Network(s)

**Tvheadend web interface: _Configuration -> DVB Inputs -> Networks_**

Create a network of the appropriate type here. You can have multiple networks
of the same type as necessary, e.g. to have two DVB-T networks defined,
one with HD muxes, one without.

The creation process allows you to select from a series of pre-defined mux
lists for common DVB sources. These are available [here](https://github.com/tvheadend/dtv-scan-tables) -
but they do go out of date as broadcasters move services around and national
authorities change entire pieces of spectrum. As such, you should try the
pre-defined values, but you may need to add muxes manually.

* When creating a DVB-S network, be sure to set the orbital 
position of the satellite to which your dish is pointing, as some satellites 
provide additional information related to other nearby satellites that 
you may not be able to receive.

* Network discovery (enabled by default) increases the likelihood of 
receiving all available muxes and services.

###3. Associate the Network with the Respective Tuner(s)

**Tvheadend web interface: _Configuration -> DVB Inputs -> TV Adapters_**

Associate each of your tuners with the correct network through _Parameters -> Basic Settings_. 

This can be as simple or as complex as necessary. You may simply have, for
example, a single DVB-S2 network defined and then associate this with all
DVB-S2 tuners. Or, you might have multiple networks defined - different
satellites, different encoding. So, as further examples, you might define
and then associate an HD DVB-T2 (e.g. H.264) network with HD tuners, while
having a separate SD network associated with an independent SD (e.g. MPEG-2)
tuner. 

At this point, your tuners now know what networks to use: one network can
appear on multiple tuners (many-to-one), and one tuner can have multiple
networks.

###4. If Necessary, Manually Add Muxes

**Tvheadend web interface: _Configuration -> DVB Inputs -> Muxes_**

Ideally, this is where you'll see a list of the pre-populated muxes as created
when you set up your initial network. However, should there be any issues,
this is where you can manually add missing muxes. You only really need to
worry about this if the pre-defined list didn't work (e.g. because of
out-of-date data as broadcasters re-arrange their services or because automatic
detection (network discovery) hasn't successfully found all the muxes over time. 

If you do need to add something manually, you'll need to search the Internet
for details of the appropriate transmitter and settings: satellites tend not
to change much and are universal over a large area, but terrestrial muxes
are typically very localised and you'll need to know which specific transmitter
you're listening to. 

**Note**: some tuners (or drivers) require more tuning parameters than others so 
**be sure to enter as many tuning parameters as possible**.

Good sources of transmitter/mux information include:

* [KingofSat](http://en.kingofsat.net) for all European satellite information

* [ukfree.tv](http://www.ukfree.tv/maps/freeview) for UK DVB-T transmitters

* [Interactive EU DVB-T map](http://www.dvbtmap.eu/mapmux.html) for primarily
central and northern Europe

* [Lyngsat](http://www.lyngsat.com/) for worldwide satellite information.

You can also use [dvbscan](http://www.linuxtv.org/wiki/index.php/Dvbscan) to
force a scan and effectively ask your tuner what it can see.

###5. Scan for Services

**Tvheadend web interface: _Configuration -> DVB Inputs -> Services_**

This is where the services will appear as your tuners tune to the muxes based
on the network you told them to look on. Again, remember what's happening: 
Tvheadend is telling your tuner hardware (via the drivers) to sequentially
tune to each mux it knows about, and then see what 'programs' it can see
on that mux, each of which is identified by a series of unique identifiers
that describe the audio stream(s), the video stream(s), the subtitle stream(s)
and language(s), and so on.

(For the technically-minded, these unique identifiers - the elementary streams - are referred to as 'packet identifiers' or 'PIDs').

###6. Map Services to Channels

  **Tvheadend web interface: _Configuration -> DVB Inputs -> Services_**

  Once scanning for services is complete, you need to map the services to 
  channels so your client can actually request them (i.e. so you can watch
  or record).
  
  See [Services](class/mpegts_service) for a detailed look into service mapping.

####6.1. Bouquets

  **Tvheadend web interface: _Configuration -> Channel / EPG -> Bouquets_**

  Many service providers use bouquets for channel management and just 
  like a standard set-top box Tvheadend can use these to automatically 
  manage and keep your channels up-to-date.
  
  If you would like to use bouquets see [Bouquets](class/bouquet).

###7. Watch TV

That's it - you're done. You should now have a working basic Tvheadend
installation with channels mapped and ready for use!

As required, you may now wish to look into:

* Setting up different EPGs (inc. localised character sets and timing offsets)
* Setting up channel icons
* Setting up recording profiles
* Setting up streaming profiles (including transcoding)
* Arranging your channels into groups (channel tags)
* Setting up softcams for descrambling
* Setting up access control rules for different client types/permission levels
