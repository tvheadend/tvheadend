:

The tuner (or network if using IPTV) with the highest priority value 
will be used out of preference. If the tuner is busy the next available 
with the highest priority value will be used. 

An example:

Tuner            | Tuner A | Tuner B | Tuner C 
-----------------|---------|---------|--------
Priority         | 100     | 50      | 80
Status           | BUSY    | IDLE    | IDLE

In the above table *Tuner A* is busy so Tvheadend will have to use the 
next available idle tuner which in this example is *Tuner B* and *Tuner C* 
but because *Tuner C* has the higher priority of the two Tvheadend will 
use that instead of *Tuner B*. If no priority value is set for any tuners 
Tvheadend will use the first available idle tuner.
