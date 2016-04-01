Setting up access control is an important initial step as **the system
is initially wide open**. 

Tvheadend verifies access by scanning through all enabled access control
entries in sequence, from the top of the list to the bottom. The permission
flags, streaming profiles, DVR config profiles, channel tags and so on are
combined for all matching access entries. An access entry is said to match
if the username matches and the IP source address of the requesting peer
is within the prefix. There is also anonymous access, if the user is set
to asterisk. Only network prefix is matched then.
