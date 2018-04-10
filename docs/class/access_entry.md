<tvh_include>inc/users_contents</tvh_include>

---

<tvh_include>inc/users_overview</tvh_include>

!['Access Entries' Tab](static/img/doc/users/access_entries_tab.png)

### Notes on Access Entries

* Wildcard (anonymous) accounts (that require no username or password) 
can be created by entering an asterisk `*` in the username/password field. 
These accounts are matched using the network prefix *(Allowed networks)*, 
acting similar to a username.

* Tvheadend verifies access by scanning through all enabled access control
entries in sequence, from the top of the list to the bottom.
The order of entries is **extremely** important! It's recommended 
that you put wildcard (anonymous) accounts at top and all others 
(with special permissions) at the bottom. 

* An access entry is said to match if the username and the IP source 
address of the requesting peer is within the prefix *(Allowed networks)*, 
wildcard (anonymous) accounts will match **ANY** username.

* The permission flags, streaming profiles, DVR config profiles, channel tags, and channel
number ranges are combined (for all matching access entries). You can 
control which parameters are merged, see *Change parameters* [below](#items). 

---

## Buttons

<tvh_include>inc/buttons</tvh_include>


---

