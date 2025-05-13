# Configuring for the First Time

Contents                                            | Description
----------------------------------------------------|-----------------------------
[Using the Wizard](#using-the-wizard)               | Getting to know the first-time-user wizard
[Manual Set-up](#manual-set-up)                     | Set-up Tvheadend manually

---

## Using the Wizard

![Wizard](static/img/doc/firstconfig/wizard.png)

The wizard helps you get up and running fast, if you don't see the 
wizard on the initial run, or would like to use it, you can do so by 
pressing the **Start wizard** button in **Configuration -> General -> Base**.

**If you haven't already don't so**, please take a look at the 
[Introduction](introduction) before continuing with the wizard, as it 
contains lots of useful info!

### 1. Welcome

The first part of the wizard is where you select the basic language settings, 
if you don't enter a preferred language, US English will be used as a 
default. Not selecting the correct EPG language can result in garbled EPG 
text; if this happens, don't panic, as you can easily change it later.

If you cannot see your preferred language in the language list and would 
like to help translate Tvheadend see 
[here](https://docs.tvheadend.org/documentation/development/translations).

### 2. Access Control

Here you enter the access control details to secure your system. 

The first part of this covers the network details for address-based 
access to the system; for example, 192.168.1.0/24 to allow local access only to 
192.168.1.x clients, or 0.0.0.0/0 or empty value for access from any 
system. This works alongside the second part, which is a familiar 
username/password combination, so provide these for both an 
administrator and regular (day-to-day) user.

* You may enter a comma-separated list of network prefixes (IPv4/IPv6). 
If you were asked to enter a username and password during installation, 
we'd recommend not using the same details for a user here as it may 
cause unexpected behavior, incorrect permissions etc.

* To allow anonymous access for any account (administrative or regular 
user) enter an asterisk ```*``` in the username and password fields. It 
is not recommended that you allow anonymous access to the admin account.
If you plan on accessing Tvheadend over the Internet, make sure you use 
strong credentials and do not allow anonymous access at all. 

### 3. Tuner and Network

Many tuners are able to receive different signal types, If you receive 
your channels through an..

Signal type                      | Use tuners with .. in the name   
---------------------------------|-----------------------------------
Antenna (also known as an aerial)| ```DVB-T/ATSC-T/ISDB-T```
Satellite dish                   | ```DVB-S/S2```
Cable                            | ```DVB-C/ATSC-C/ISDB-C```
Internet m3u playlist            | ```IPTV```


* Tuners already in use will not appear.
* If using IPTV, the playlist you enter must contain valid links to
  streams using codecs/containers supported by Tvheadend.
* For devices with multiple tuners (e.g. either cable or terrestrial),
  be aware that many only allow you to use one tuner at a time.
  Selecting more than one tuner per device can thus result in unexpected
  behavior.
  
### 4. Predefined Muxes

Assign predefined muxes to networks. To save you from manually entering
muxes, Tvheadend includes predefined mux lists. Please select an option 
from the list for each network.

* Select the closest transmitter if using an antenna (T); if using
  cable (C), select your provider; if using satellite (S), the orbital
  position of the satellite your dish is pointing towards; or if using
  IPTV, enter the URL to your playlist.
* If you're unsure as to which list(s) to select you may want to look
  online for details about the various television reception choices
  available in your area.
* Networks already configured will not be shown.
* Selecting the wrong list may cause the scan (on the next page) to fail.

### 5. Scanning

Tvheadend should now be scanning for available services. Please wait until the
scan completes.

* During scanning, the number of muxes and services shown below should
  increase. If this doesn't happen, check the connection(s) to your
  device(s)..
* The status tab (behind this wizard) will display signal information.
  If you notice a lot of errors or the signal strength appears low then
  this usually indicates a physical issue with your antenna, satellite
  dish, cable or network..
* If you don't see any signal information at all, but the number of
  muxes or services is increasing anyway, the driver used by your device
  isn't supplying signal information to Tvheadend. In most cases this
  isn't an issue and can be ignored.
  
### 6. Service Mapping

Here you map all discovered services to channels..

In order for your frontend client(s) (such as Kodi, Movian, and similar)
to see/play channels, you must first map discovered services to
channels.

**You can omit this step (do not check 'Map all services') and
map services to channels manually.**

Option                           | What it does
---------------------------------|---------------------------
Map all services                 | Map all available services, including encrypted, data services and radio.
Create provider tags             | Create and link a provider tag to the mapped channels.
Create network tags              | Create and link a network tag to the mapped channels.

* Many providers include undesirable services - Teleshopping, Adult
  Entertainment, etc; using the 'Map all services' will include these.
* You may need to enable specific EPG grabbers to receive OTA EPG data,
See the *EPG Grabber Modules* Help doc for details.

### 7. Finished

You've now finished the wizard, at this point you should be able to 
view your channels and make further changes; If not, please take a look 
at the manual set-up below.

---

## Manual Set-up

### 1. Ensure Tuners are Available for Use

**Configuration -> DVB Inputs -> TV Adapters**

On this tab, you'll see a tree structure, with the Linux device list at the
top level (e.g. `/dev/dvb/adapter0`)

Individual tuners are then the next level down (e.g. `DiBcom 7000PC : DVB-T #0`)

![Adapter example](static/img/doc/firstconfig/adapters.png)

Click on each tuner that you want Tvheadend to use, and ensure "Enabled"
is checked in the 'Parameters' list

If anything is obviously wrong at this point, you probably have a
driver/firmware error which you'll need to resolve before going any further.

Note, there may be additional levels shown related to various 
other settings, e.g. satellite positions for SAT>IP tuners and so on - 
these are more advanced options that most will not need to use so can 
be ignored (generally).

### 2. Set up Relevant Network(s)

**Configuration -> DVB Inputs -> Networks**

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

### 3. Associate the Network with the Respective Tuner(s)

**Configuration -> DVB Inputs -> TV Adapters**

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

### 4. If Necessary, Manually Add Muxes

**Configuration -> DVB Inputs -> Muxes**

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

**Note**: Some tuners (or drivers) require more tuning parameters than others so 
**be sure to enter as many tuning parameters as possible**.

Good sources of transmitter/mux information include:

* [KingofSat](http://en.kingofsat.net) for all European satellite information

* [ukfree.tv](http://www.ukfree.tv/maps/freeview) for UK DVB-T transmitters

* [Interactive EU DVB-T map](http://www.dvbtmap.eu/mapmux.html) for primarily
central and northern Europe

* [Lyngsat](http://www.lyngsat.com/) for worldwide satellite information.

You can also use [dvbscan](http://www.linuxtv.org/wiki/index.php/Dvbscan) to
force a scan and effectively ask your tuner what it can see.

### 5. Scan for Services

**Configuration -> DVB Inputs -> Services**

This is where the services will appear as your tuners tune to the muxes based
on the network you told them to look on. Again, remember what's happening: 
Tvheadend is telling your tuner hardware (via the drivers) to sequentially
tune to each mux it knows about, and then see what 'programs' it can see
on that mux, each of which is identified by a series of unique identifiers
that describe the audio stream(s), the video stream(s), the subtitle stream(s)
and language(s), and so on.

(For the technically-minded, these unique identifiers - the elementary streams - are referred to as 'packet identifiers' or 'PIDs').

### 6. Map Services to Channels

**Configuration -> DVB Inputs -> Services**

Once scanning for services is complete, you need to map the services to 
channels so your client can actually request them (i.e. so you can watch
or record).
  
See [Services](class/mpegts_service) for a detailed look into service mapping.

**Bouquets**: Many service providers - mostly those using satellite - use bouquets for channel management and just 
like a standard set-top box Tvheadend can use these to automatically 
manage and keep your channels up-to-date. If you would like to use 
bouquets see [here](class/bouquet).

### 7. Watch TV

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
