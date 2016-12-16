:

Allows you to control which parameters are merged. If the 
*Change parameters* flag is turned on and a parameter 
(permission flags, all types of profiles, channel tags and ranges)
for an entry is not set the parameter (value, list or range) is cleared 
(unset). This allows the next matching entry (if any) in the sequence 
to set it.

For example, say you have a wildcard account with the theme set to Gray, 
and an admin account with the Blue theme. Unchecking the theme checkbox 
for the admin user would mean that the theme from the last matching 
entry (which in this case would be the wildcard account) applies instead.

Option                     | Description/Properties
---------------------------|---------------------------
**Rights**                 | *Streaming*, *Web interface*, *Video recorder* (DVR), *Admin* and *Anonymize HTSP access*.
**Channel number range**   | *Minimal channel number* and *Maximal channel number*.
**Channel tags**           | *Exclude channel tags* and *Channel tags*.
**DVR configurations**     | *DVR configuration profiles*.
**Streaming profiles**     | *Streaming profiles*.
**Connection limits**      | *Connection limit type* and *Limit connections*.
**Language**               | *Language*.
**Web interface language** | *Web interface language*.
**Theme**                  | *Theme*.
**User interface level**   | *User interface level*.

The above table displays the *Change parameters* option name and the fields that it 
applies to, as shown in add/edit dialog(s).
